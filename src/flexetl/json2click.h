#pragma once

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include <concurrentqueue/blockingconcurrentqueue.h>

#include "handler.h"
#include "clicktbl.h"

#include "stem/metrics.h"

class Json2Click : public IHandler
{
    stem::metrics::counter_u m_ErrorFS;
    stem::metrics::counter_u m_ErrorJSON;
    stem::metrics::counter_u m_Rows;
    stem::metrics::counter_u m_DataSize;


    typedef moodycamel::BlockingConcurrentQueue<std::filesystem::path> Queue;

    ClickTable m_table;

    INode *m_pnode = 0;

    std::string m_metrics;

    std::string m_outputDir;
    std::string m_failedDir;
    std::string m_backupDir;
    int m_delay = 0;     // seconds
    int m_blockSize = 0; // max size

    std::time_t m_saveTime = 0;
    int m_saveCounter = 0;

    std::unique_ptr<Queue> m_pQueue;

    std::thread m_t;
    std::atomic<bool> m_run;

    void dump(bool bForced = false);
    void lifeProc();

public:
    Json2Click();
    virtual ~Json2Click();

    virtual bool doProc(const std::filesystem::path &fileName) override;
    virtual bool init(INode *pnode) override;
    virtual bool start() override;
};