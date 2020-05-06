#include <unistd.h>
#include <sys/poll.h>
#include <iostream>

#include <experimental/filesystem>

namespace std
{
using namespace experimental;
}

#include "backuper.h"

Backuper::Backuper(int cleanTime, std::shared_ptr<spdlog::logger> logger) : m_run(false), m_logger(logger), m_cleanTime(cleanTime)
{
	m_run.store(true, std::memory_order_release);
	m_t = std::thread(&Backuper::lifeProc, this);
}
Backuper::~Backuper()
{
	m_run.store(false, std::memory_order_release);
	if (m_t.joinable())
		m_t.join();
}
void Backuper::addBackupDir(const std::string &path, const std::string &ext)
{
	m_registerQueue.enqueue({path, ext});
}

void Backuper::lifeProc()
{
	std::chrono::steady_clock::time_point lastTime;
	int period = 3600; // check interval in second
	while (m_run.load(std::memory_order_acquire))
	{

		bool bForceCheck = false;
		std::pair<std::string, std::string> newPath;
		if (m_registerQueue.wait_dequeue_timed(newPath, std::chrono::seconds(1)))
		{
			m_backupDirs[newPath.first].insert(newPath.second);
			bForceCheck = true;
			m_logger->debug("Backuper:: add clean \"{}\" for ext \"{}\"", newPath.first, newPath.second);
		}

		auto curTime = std::chrono::steady_clock::now();
		if (!bForceCheck && std::chrono::duration_cast<std::chrono::seconds>(curTime - lastTime).count() < period)
		{
			continue;
		}
		lastTime = curTime;

		auto curSysTime = std::chrono::system_clock::now();
		for (const auto &path : m_backupDirs)
		{
			if (!std::filesystem::exists(std::filesystem::path(path.first)))
				continue;

			std::error_code err;
			std::filesystem::directory_iterator end_itr;
			for (std::filesystem::directory_iterator itr(path.first, err); itr != end_itr; itr++)
			{
				if (is_directory(itr->status()))
				{
					continue;
				}

				if (path.second.end() == path.second.find(itr->path().extension().string()))
				{
					continue;
				}

				auto fileTime = std::filesystem::last_write_time(itr->path());

				if (std::chrono::duration_cast<std::chrono::hours>(curSysTime - fileTime).count() > m_cleanTime)
				{
					std::error_code err;
					std::filesystem::remove(itr->path(), err);
					if (err)
					{
						m_logger->warn("Backuper::Remove failed: {}: {}", itr->path().string(), err.message());
					}
					else
					{
						//m_logger->debug("Remove: {}", itr->path().string());
					}
				}
				else
				{
					//m_logger->debug("Skipe: {}", itr->path().string());
				}
			}
			if (err)
			{
				m_logger->warn("Clean for path \"{}\" failed: {}", path.first, err.message());
			}
		}
	}

	m_logger->debug("Backuper::thread exit");
}
