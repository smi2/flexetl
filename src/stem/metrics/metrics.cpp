// #include <netdb.h>
// #include <sys/socket.h>
// #include <netinet/tcp.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <sys/ioctl.h>
// #include <ifaddrs.h>
// #include <net/if.h>

#if !defined(_win_)
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/tcp.h>
#endif

#include <thread>

#include <iostream>
#include "metrics.h"

namespace stem
{

static const int retryTimeout = 60;

metrics::metrics(std::string host,
				 uint16_t port,
				 std::string root,
				 std::shared_ptr<spdlog::logger> logger) : m_host(host),
														   m_port(port),
														   m_logger(logger),
														   m_active(false),
														   m_root(root),
														   socket_(-1),
														   socket_output_(socket_)
{
	if (m_host.empty())
		return;
	m_sendQueue = std::make_unique<SendQueue>(1000);

	m_active = true;
	m_run = true;

	m_sendThread = std::thread(&metrics::sendProc, this);
	m_agregateThread = std::thread(&metrics::agregateProc, this);
}

metrics::~metrics()
{
	m_run.store(false, std::memory_order_relaxed);

	if (m_sendThread.joinable())
		m_sendThread.join();
	if (m_agregateThread.joinable())
		m_agregateThread.join();
}

void metrics::stop()
{
	m_active = false;
}
void metrics::start()
{
	if (m_host.empty())
		m_active = false;
	else
		m_active = true;
}

void metrics::ResetConnection()
{
	SocketHolder s(SocketConnect(NetworkAddress(m_host, std::to_string(m_port))));

	if (s.Closed())
	{
		throw std::system_error(errno, std::system_category());
	}
	s.SetTcpKeepAlive(30, 5, 3);

	socket_ = std::move(s);
	socket_output_ = SocketOutput(socket_);
	m_logger->info("Metrics connected to {}:{}", m_host, m_port);
}

void metrics::sendProc()
{
	auto sleepTime = std::chrono::milliseconds(500);
	int connectCounterLimit = retryTimeout * 2;

	int connectCounter = connectCounterLimit; // для установки соединения на первом шаге
	while (m_run.load(std::memory_order_relaxed))
	{
		if (socket_.Closed() && connectCounter >= connectCounterLimit)
		{
			connectCounter = 0;
			try
			{
				ResetConnection();
			}
			catch (std::exception &ex)
			{
				m_logger->warn("Metrics: can't connect to {}:{}: {}", m_host, m_port, ex.what());
			}
			catch (...)
			{
				m_logger->warn("Metrics: can't connect to {}:{}", m_host, m_port);
			}
		}
		if (socket_.Closed())
		{
			connectCounter++;
			std::this_thread::sleep_for(sleepTime);
			continue;
		}

		std::string metric;
		if (m_sendQueue->wait_dequeue_timed(metric, sleepTime))
		{
			try
			{
				socket_output_.Write(metric.c_str(), metric.size());
				m_logger->debug("Metrics: sended");
			}
			catch (const std::exception &ex)
			{
				m_logger->warn("Metrics: disconnected: {}", ex.what());

				// вновь поместим в очередь на отправку
				m_sendQueue->enqueue(metric);

				socket_.Close();
				connectCounter = connectCounterLimit; // что бы сразу попытаться установить соединение
			}
		}
	}
	socket_.Close();
	m_logger->debug("metrics::threadSendProc exit");
}
void metrics::agregateProc()
{
	auto curTime = std::chrono::system_clock::now();
	auto lastTime = std::chrono::time_point_cast<std::chrono::minutes>(curTime);
	while (m_run.load(std::memory_order_relaxed))
	{
		auto curTime = std::chrono::time_point_cast<std::chrono::minutes>(std::chrono::system_clock::now());
		if (curTime != lastTime)
		{
			lastTime = curTime;

			auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(curTime).time_since_epoch().count();

			std::stringstream buffer;
			int counter = 0;

			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto &itr : m_counterMap)
			{
				buffer << itr.first << " " << itr.second->flush() << " " << seconds << "\n";
				counter++;
			}

			for (const auto &itr : m_metricsMap)
			{
				buffer << itr.first << " " << itr.second << " " << seconds << "\n";
				counter++;
			}
			m_metricsMap.clear();
			m_logger->info("Metrics: agregate done: {}\n{}", counter, buffer.str());

			if (counter > 0)
				m_sendQueue->enqueue(buffer.str());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	m_logger->debug("metrics::threadAgregateProc exit");
}
std::string metrics::getName(std::string root, std::string name)
{
	auto joinRoot = [](const std::string &root, std::string name) {
		std::string mtr;
		if ((root[root.length() - 1] == '.' && name[0] != '.') ||
			(root[root.length() - 1] != '.' && name[0] == '.'))
		{
			mtr = root + name;
		}
		else if (root[root.length() - 1] != '.' && name[0] != '.')
		{
			mtr = root + "." + name;
		}
		else
		{
			mtr = root;
			mtr.append(name.begin()++, name.end());
		}
		return mtr;
	};

	if (!m_root.empty() && !root.empty())
		root = joinRoot(m_root, std::move(root));
	else if (!m_root.empty())
		root = m_root;
	else if (root.empty())
		return name;

	return joinRoot(root, std::move(name));
}

} // namespace stem