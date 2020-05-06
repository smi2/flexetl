#pragma once

#include <list>
#include <atomic>
#include <memory>

#include "handler.h"
#include "watcher.h"
#include "nodes.h"

class FlexEtl
{
public:
    FlexEtl(std::shared_ptr<spdlog::logger> logger);

    ~FlexEtl();

    bool init(const std::string &configFile,int cleanBackupDays);

    bool start();

    void stop();

    bool IsRunning() const;

private:

    std::shared_ptr<spdlog::logger> m_logger;
    std::shared_ptr<stem::metrics> m_metrics;
    libconfig::Config cfg;
    std::shared_ptr<Node> m_rootNode;
    std::atomic<bool> run;
    Watcher m_watcher;
};
