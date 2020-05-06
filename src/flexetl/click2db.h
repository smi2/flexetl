#pragma once

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <thread>

#include <concurrentqueue/blockingconcurrentqueue.h>

#include <clickhouse-cpp/client.h>
#include <clickhouse-cpp/error_codes.h>
#include <clickhouse-cpp/types/type_parser.h>
#include <clickhouse-cpp/columns/factory.h>

#include "stem/metrics.h"

#include "handler.h"
#include "clicktbl.h"


class Click2DB : public IHandler
{
    stem::metrics::counter_u g_ErrorDB;
    stem::metrics::counter_u g_ErrorFS;
    stem::metrics::counter_u g_ErrorTypes;
    stem::metrics::counter_u g_InsertedRows;


    typedef moodycamel::BlockingConcurrentQueue<std::pair<int, std::filesystem::path>> Queue;
    //ClickTable m_table;

    INode *m_pnode = 0;

    std::string m_metrics;

    std::string m_outputDir;

    std::string m_host;
    std::string m_user;
    std::string m_password;
    std::string m_tableName;
    std::string m_toSend;
    std::string m_loadFailed;
    std::string m_sendFailed;
    std::map<std::string, std::string> m_tblType;

    std::unique_ptr<clickhouse::Client> m_pClickDB;
    std::unique_ptr<Queue> m_pQueue;

    std::chrono::steady_clock::time_point m_lastCheckTime;

    std::thread m_t;
    std::atomic<bool> m_run;

    void lifeProc();

    bool descibeTable();
    bool tryConnect();
    void sendTable(const ClickTable &table);

public:
    Click2DB();
    virtual ~Click2DB();

    virtual bool doProc(const std::filesystem::path &fileName) override;
    virtual bool init(INode *pnode) override;
    virtual bool start() override;
};