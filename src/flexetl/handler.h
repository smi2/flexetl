#pragma once

#include <libconfig.h++>
#include <memory>
#include <string>
#include <experimental/filesystem>

#include <spdlog/spdlog.h>

namespace std
{
using namespace experimental;
}

namespace stem{class metrics;}


struct IHandler;
struct INode
{
    virtual bool watch(const std::string &name, const std::string &ext) = 0;
    virtual void cleanBackupDir(const std::string &name, const std::string &ext) = 0;

    virtual int subCount() = 0;
    virtual std::shared_ptr<INode> subNode(int num) = 0;
    virtual std::shared_ptr<IHandler> subHundler(int num) = 0;
    virtual bool getStrParam(const std::string &name, std::string &value) = 0;
    virtual bool getIntParam(const std::string &name, int &value) = 0;
    virtual bool moveFile(const std::filesystem::path &toPath, const std::filesystem::path &fileName) = 0;
    virtual std::shared_ptr<spdlog::logger> logger() = 0;
    virtual std::shared_ptr<stem::metrics> metrics() = 0;
};

struct IHandler
{
    virtual bool doProc(const std::filesystem::path &fileName) { return true; }
    virtual bool init(INode *pnode) { return true; }
    virtual bool start() { return true; }
};
