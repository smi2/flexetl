#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <sys/inotify.h>

#include "stem/metrics.h"

#include "handler.h"
#include "watcher.h"
#include "backuper.h"

typedef libconfig::Setting etlsettings;

class Node : public INode
{
    std::string m_name;
    std::shared_ptr<IHandler> m_pHandler;
    std::set<int> m_watchFd;
    std::vector<std::shared_ptr<Node>> m_subnodes;
    Watcher *m_pWatcher = 0;
    const libconfig::Setting *m_pSettings = 0;

    std::shared_ptr<spdlog::logger> m_logger;
    std::shared_ptr<stem::metrics>  m_metrics;
    std::shared_ptr<Backuper>  m_backuper;

    static std::shared_ptr<IHandler> makeHandler(const std::string &name);

    //void onWatch(const std::string &path, const std::string &file);

public:
    Node(std::shared_ptr<spdlog::logger> logger,std::shared_ptr<stem::metrics> metrics,std::shared_ptr<Backuper>  backuper);
    virtual ~Node();
    bool init(Watcher *pWatcher);
    bool create(const libconfig::Setting &settings);
    bool start();

    virtual bool watch(const std::string &name, const std::string &ext) override;
    virtual void cleanBackupDir(const std::string &name, const std::string &ext) override;

    virtual int subCount() override;
    virtual std::shared_ptr<INode> subNode(int num) override;
    virtual std::shared_ptr<IHandler> subHundler(int num) override;
    virtual bool getStrParam(const std::string &name, std::string &value) override;
    virtual bool getIntParam(const std::string &name, int &value) override;
    virtual bool moveFile(const std::filesystem::path &toPath, const std::filesystem::path &fileName) override;
    
    virtual std::shared_ptr<spdlog::logger> logger() override;
    virtual std::shared_ptr<stem::metrics> metrics() override;
};
