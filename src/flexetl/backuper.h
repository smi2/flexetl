#pragma once

#include <thread>
#include <string>
#include <memory>
#include <list>
#include <map>
#include <set>
#include <functional>
#include <mutex>

#include <spdlog/spdlog.h>
#include <concurrentqueue/blockingconcurrentqueue.h>

class Backuper
{
   typedef moodycamel::BlockingConcurrentQueue<std::pair<std::string,std::string> > Queue;

    Queue   m_registerQueue;
    std::map<std::string, std::set<std::string> > m_backupDirs;
    std::atomic<bool> m_run;
    std::thread m_t;
    std::shared_ptr<spdlog::logger> m_logger;
    int m_cleanTime;// in hours

    void lifeProc();

public:
    Backuper(int cleanTime, std::shared_ptr<spdlog::logger> logger);
    ~Backuper();
    bool start();

    void addBackupDir(const std::string &path, const std::string &ext);
};