#include <unistd.h>
#include <sys/poll.h>
#include <iostream>

#include <experimental/filesystem>

namespace std
{
using namespace experimental;
}

#include "watcher.h"

#define WATCH_FLAGS (IN_CLOSE_WRITE | IN_MOVED_TO)
#define MAX_EVENTS 1024								   /*Максимальное кличество событий для обработки за один раз*/
#define LEN_NAME 16									   /*Будем считать, что длина имени файла не превышает 16 символов*/
#define EVENT_SIZE (sizeof(struct inotify_event))	  /*размер структуры события*/
#define BUF_LEN (MAX_EVENTS * (EVENT_SIZE + LEN_NAME)) /*буфер для хранения данных о событиях*/

Watcher::Watcher(std::shared_ptr<spdlog::logger> logger) : m_run(false), m_logger(logger)
{
	m_fd = inotify_init();
}
Watcher::~Watcher()
{
	m_run.store(false, std::memory_order_release);

	if (m_t.joinable())
		m_t.join();
}

void Watcher::lifeProc()
{
	char buffer[BUF_LEN];

	struct pollfd fds = {m_fd, POLLIN, 0};

	while (m_run.load(std::memory_order_acquire))
	{
		// ждём до 10 секунд
		int ret = poll(&fds, 1, 1000);
		// проверяем успешность вызова
		if (ret == -1) // ошибка
			break;
		else if (ret == 0) // таймаут, событий не произошло
			continue;

		int i = 0;
		int length = read(m_fd, buffer, BUF_LEN);

		if (length < 0)
		{
			break;
		}
		while (i < length)
		{
			struct inotify_event *event = (struct inotify_event *)&buffer[i];
			if (event->len)
			{
				if ((event->mask & WATCH_FLAGS) != 0)
				{
					auto itr = m_watchMap.find(event->wd);
					if (itr != m_watchMap.end())
					{

						std::filesystem::path notifyFile(event->name);
						std::filesystem::path fileName = std::get<0>(itr->second) / notifyFile;
						std::filesystem::path watchExtension = std::get<1>(itr->second);
						if (watchExtension.empty() || notifyFile.extension() == watchExtension)
						{
							m_logger->debug("onWarching {}", fileName.string());
							std::get<2>(itr->second)(fileName);
						}
						else
						{
							m_logger->debug("onWarching {}: ignored", fileName.string());
						}
					}
					else
					{
						m_logger->warn("ERROR: no callback for {}", std::string(event->name));
					}
				}

				// if ( event->mask & IN_CREATE) {
				//     if (event->mask & IN_ISDIR)
				//         printf( "The directory %s was Created.\n", event->name );
				//     else
				//     printf( "The file %s was Created with WD %d\n", event->name, event->wd );
				// }
				// if ( event->mask & IN_MODIFY) {
				//     if (event->mask & IN_ISDIR)
				//         printf( "The directory %s was modified.\n", event->name );
				//     else
				//         printf( "The file %s was modified with WD %d\n", event->name, event->wd );
				// }

				// if ( event->mask & IN_DELETE) {
				//     if (event->mask & IN_ISDIR)
				//         printf( "The directory %s was deleted.\n", event->name );
				//     else
				//         printf( "The file %s was deleted with WD %d\n", event->name, event->wd );
				// }
			}
			i += EVENT_SIZE + event->len;
		}
	}

	m_logger->debug("Watcher::thread exit");
}

bool Watcher::startAndWait()
{
	if (m_run.load(std::memory_order_acquire))
		return true;

	m_run.store(true, std::memory_order_release);
	m_t = std::thread(&Watcher::lifeProc, this);
	m_t.join();
	return true;
}
bool Watcher::start()
{
	if (m_run.load(std::memory_order_acquire))
		return true;
	m_run.store(true, std::memory_order_release);
	m_t = std::thread(&Watcher::lifeProc, this);
	return true;
}

int Watcher::addWatch(const std::string &watch, const std::string &ext, watch_cb callBack)
{
	if (!std::filesystem::exists(std::filesystem::path(watch)))
	{
		std::filesystem::create_directories(std::filesystem::path(watch));
	}

	int wd = inotify_add_watch(m_fd, watch.c_str(), WATCH_FLAGS);
	m_watchMap[wd] = std::make_tuple(watch, ext, callBack);
	return wd;
}
