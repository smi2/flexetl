#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "nodes.h"
#include "json2click.h"
#include "click2db.h"
#include "pipeline.h"
#include "pipeset.h"

Node::Node(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<stem::metrics> metrics, std::shared_ptr<Backuper> backuper) : m_logger(logger), m_metrics(metrics), m_backuper(backuper)
{
}
Node::~Node()
{
}

std::shared_ptr<spdlog::logger> Node::logger()
{
    return m_logger;
}
std::shared_ptr<stem::metrics> Node::metrics()
{
    return m_metrics;
}

bool Node::init(Watcher *pWatcher)
{
    m_pWatcher = pWatcher;
    if (m_pHandler && !m_pHandler->init(this))
    {
        m_logger->error("ERROR: at init {}", m_name);
        return false;
    }
    for (auto &node : m_subnodes)
    {
        if (!node->init(pWatcher))
        {
            m_logger->error("ERROR: at init node");
            return false;
        }
    }
    return true;
}

bool Node::create(const libconfig::Setting &settings)
{

    m_pSettings = &settings;
    if (!m_pSettings)
        return false;

    if (m_pSettings->lookupValue("handler", m_name))
    {
        m_pHandler = makeHandler(m_name);
        if (!m_pHandler)
        {
            m_logger->error("ERROR: create handler '{}' failed", m_name);
            return false;
        }
    }

    try
    {
        if (!m_pSettings->exists("nodes"))
        {
            return true;
        }
        const libconfig::Setting &subnodes = (*m_pSettings)["nodes"];
        int count = subnodes.getLength();
        for (int i = 0; i < count; ++i)
        {
            m_subnodes.push_back(std::make_shared<Node>(m_logger, m_metrics, m_backuper));
        }
        for (int i = 0; i < count; ++i)
        {
            if (!m_subnodes[i]->create(subnodes[i]))
                return false;
        }
    }
    catch (const libconfig::SettingNotFoundException &nfex)
    {
        m_logger->error("Create failed: {}", nfex.what());
        return false;
    }
    return true;
}
bool Node::start()
{
    if (m_pHandler && !m_pHandler->start())
    {
        m_logger->error("ERROR: at start handler");
        return false;
    }
    for (auto &node : m_subnodes)
    {
        if (!node->start())
        {
            m_logger->error("ERROR: at start node");
            return false;
        }
    }
    return true;
}

int Node::subCount()
{
    return m_subnodes.size();
}
std::shared_ptr<INode> Node::subNode(int num)
{
    if (m_subnodes.size() < num)
        return std::shared_ptr<INode>();
    return m_subnodes[num];
}
bool Node::getStrParam(const std::string &name, std::string &value)
{
    if (!m_pSettings)
        return false;
    bool res = m_pSettings->lookupValue(name, value);
    if (!res)
    {
        m_logger->warn("{}:getStrParam({}) failed", m_name, name);
    }
    return res;
}
bool Node::getIntParam(const std::string &name, int &value)
{
    if (!m_pSettings)
        return false;
    bool res = m_pSettings->lookupValue(name, value);
    if (!res)
    {
        m_logger->warn("{}:getIntParam({}) failed", m_name, name);
    }
    return res;
}
std::shared_ptr<IHandler> Node::makeHandler(const std::string &name)
{
    if (name == std::string("pipeline"))
    {
        return std::shared_ptr<IHandler>(new PipeLine);
    }
    if (name == std::string("pipeset"))
    {
        return std::shared_ptr<IHandler>(new PipeSet);
    }
    if (name == std::string("json2click"))
    {
        return std::shared_ptr<IHandler>(new Json2Click);
    }

    if (name == std::string("click2db"))
    {
        return std::shared_ptr<IHandler>(new Click2DB);
    }

    return std::shared_ptr<IHandler>();
}

bool Node::watch(const std::string &name, const std::string &ext)
{
    if (!m_pHandler)
        return false;
    m_logger->info("addWatch {}", name);

    m_pWatcher->addWatch(name, ext, [this](const std::filesystem::path &fileName) {
        bool res = m_pHandler ? m_pHandler->doProc(fileName) : false;
    });
    return true;
}
void Node::cleanBackupDir(const std::string &name, const std::string &ext)
{
    if (!m_backuper)
        return;
    m_backuper->addBackupDir(name, ext);
}

bool Node::moveFile(const std::filesystem::path &toPath, const std::filesystem::path &fileName)
{
    if (toPath.empty())
    {
        std::error_code err;
        if (!std::filesystem::remove(fileName, err) || err)
        {
            m_logger->error("Remove failed: {}: {}", fileName.string(), err.message());
            return false;
        }
        m_logger->debug("Remove: {}", fileName.string());
        return true;
    }

    std::filesystem::path backupName(toPath / fileName.filename());
    std::error_code err;
    std::filesystem::rename(fileName, backupName, err);
    if (err)
    {
        m_logger->error("Backup failed: {} -> {}: {}", fileName.string(), backupName.string(), err.message());
        return false;
    }

    err = std::error_code();
    std::filesystem::last_write_time(backupName, std::chrono::system_clock::now(),err);
    if (err)
    {
        m_logger->error("Set write time failed: {}:  {}", fileName.string(), err.message());
        return false;
    }
    m_logger->debug("Backup: {} -> {}", fileName.string(), backupName.string());
    return true;
}

std::shared_ptr<IHandler> Node::subHundler(int num)
{
    if (m_subnodes.size() < num)
        return std::shared_ptr<IHandler>();
    return m_subnodes[num]->m_pHandler;
}
