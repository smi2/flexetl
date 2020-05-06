#pragma once

#include <thread>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <sys/inotify.h>
#include <spdlog/spdlog.h>
class Watcher
{
    typedef std::function<void(const std::filesystem::path &fileName)> watch_cb;

    //todo сделать несколько потоков
    int m_fd;
    std::map<int, std::tuple<std::filesystem::path,std::filesystem::path, watch_cb>> m_watchMap;
    std::set<int> m_watchFd;
    std::atomic<bool> m_run;
    std::thread m_t;
    std::shared_ptr<spdlog::logger> m_logger;

    void lifeProc();

public:
    Watcher(std::shared_ptr<spdlog::logger> logger);
    ~Watcher();
    bool startAndWait();
    bool start();
    int addWatch(const std::string &watch,const std::string&ext, watch_cb callBack);
};