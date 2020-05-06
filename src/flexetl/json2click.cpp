#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <iostream>

#include "json2click.h"
#include "rapidjson/reader.h"

using namespace rapidjson;

struct MyHandler
{
    std::string m_key;
    std::stringstream m_object;
    int m_objectCounter = 0;

    bool m_fillArray = false;
    std::vector<Value> m_arrayData;
    ClickTable &m_table;
    MyHandler(ClickTable &table) : m_table(table) {}

    template <class ValueT>
    bool append(ValueT &&val)
    {
        if (m_objectCounter == 1)
        {
            if (m_fillArray)
            {
                m_arrayData.emplace_back(std::move(val));
                return true;
            }
            else
            {
                checkKey(true);
                bool bRes = m_table.append(m_key, std::move(val));
                m_key.clear();
                return bRes;
            }
        }
        return true;
    }

    bool StartObject()
    {
        m_objectCounter++;
        if (m_objectCounter > 1)
        {
            m_object << "{";
        }
        return true;
    }
    bool EndObject(SizeType memberCount)
    {
        m_objectCounter--;
        if (m_objectCounter > 0)
        {
            m_object << "}";
        }
        return true;
    }

    bool StartArray()
    {
        m_fillArray = true;
        return true;
    }
    bool EndArray(SizeType elementCount)
    {
        m_fillArray = false;
        bool res = append(std::move(m_arrayData));
        m_arrayData.clear();
        return res;
    }
    bool RawNumber(const char *str, SizeType length, bool copy)
    {
        std::cout << "Number(" << str << ", " << length << ", " << std::boolalpha << copy << ")" << std::endl;
        return true;
    }

    bool Key(const char *str, SizeType length, bool copy)
    {
        if (m_objectCounter > 1)
        {
            m_object << "\"" << std::string(str, length) << "\":";
            return true;
        }
        checkKey(false);
        m_key = std::string(str, length);
        return true;
    }
    void checkKey(bool b)
    {
        if (b == m_key.empty())
            throw std::runtime_error("Error at parse json");
    }
    bool Null() { return append(Value()); }
    bool Bool(bool val) { return append(val); }
    bool Int(int val) { return append(val); }
    bool Uint(unsigned val) { return append(val); }
    bool Int64(int64_t val) { return append(val); }
    bool Uint64(uint64_t val) { return append(val); }
    bool Double(double val) { return append(val); }
    bool String(const char *str, SizeType length, bool copy) { return append(std::string(str, length)); }
};
Json2Click::Json2Click() : m_run(false), m_pQueue(std::make_unique<Queue>(10000))
{
}
Json2Click::~Json2Click()
{
    if (m_run.load(std::memory_order_acquire))
    {
        m_run.store(false, std::memory_order_release);

        if (m_t.joinable())
            m_t.join();
    }
}
bool Json2Click::doProc(const std::filesystem::path &fileName)
{
    if (m_pQueue)
        m_pQueue->enqueue(fileName);

    return true;
}

bool Json2Click::init(INode *pnode)
{
    m_pnode = pnode;

    std::string watchDir;
    if (!pnode->getStrParam("watch", watchDir))
    {
        m_pnode->logger()->error("Json2Click: No parametr 'watch'");
        return false;
    }
    if (!pnode->getStrParam("output", m_outputDir))
    {
        m_pnode->logger()->error("Json2Click: No parametr 'output'");
        return false;
    }
    if (!pnode->getStrParam("failed", m_failedDir))
    {
        m_pnode->logger()->error("Json2Click: No parametr 'failed'");
        return false;
    }
    pnode->getStrParam("succeeded", m_backupDir);
    pnode->getStrParam("metrics", m_metrics);
    if (!m_metrics.empty())
    {
        pnode->metrics()->connect(m_ErrorFS, "ErrorFS", m_metrics);
        pnode->metrics()->connect(m_ErrorJSON, "ErrorJSON", m_metrics);
        pnode->metrics()->connect(m_Rows, "Rows", m_metrics);
        pnode->metrics()->connect(m_DataSize, "DataSize", m_metrics);
    }

    if (!pnode->getIntParam("delay", m_delay))
    {
        m_pnode->logger()->debug("Default delay: 0");
    }
    else
    {
        m_pnode->logger()->debug("delay: {}", m_delay);
    }
    if (!pnode->getIntParam("blockSize", m_blockSize))
    {
        m_pnode->logger()->debug("Default blockSize: 0");
    }
    else
    {
        m_pnode->logger()->debug("blockSize: {}", m_blockSize);
    }

    if (!m_backupDir.empty() && !std::filesystem::exists(std::filesystem::path(m_backupDir)))
    {
        std::filesystem::create_directories(std::filesystem::path(m_backupDir));
    }
    if (!m_failedDir.empty() && !std::filesystem::exists(std::filesystem::path(m_failedDir)))
    {
        std::filesystem::create_directories(std::filesystem::path(m_failedDir));
    }
    if (!m_outputDir.empty() && !std::filesystem::exists(std::filesystem::path(m_outputDir)))
    {
        std::filesystem::create_directories(std::filesystem::path(m_outputDir));
    }
    if (!watchDir.empty() && !std::filesystem::exists(std::filesystem::path(watchDir)))
    {
        std::filesystem::create_directories(std::filesystem::path(watchDir));
    }

    // перебираем папку наблюдения на предмет существующих файлов
    std::filesystem::directory_iterator end_itr;
    for (std::filesystem::directory_iterator itr(watchDir); itr != end_itr; itr++)
    {
        if (!is_directory(itr->status()))
        {
            if (itr->path().extension() == std::filesystem::path(".json"))
                m_pQueue->enqueue(itr->path());
        }
    }

    pnode->watch(watchDir, ".json");
    pnode->cleanBackupDir(m_backupDir, ".json");
    return true;
}
bool Json2Click::start()
{
    m_run.store(true, std::memory_order_release);
    m_t = std::thread(&Json2Click::lifeProc, this);
    return true;
}

void Json2Click::dump(bool bForced)
{
    if (0 == m_table.rowCount())
        return;

    std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    if (!bForced && m_saveTime == 0)
    {
        m_saveTime = now_c;
        return;
    }

    // защита от сохранение чаще чем раз в секунду
    bool rowCountOver = m_blockSize != 0 ? m_table.rowCount() >= m_blockSize : false;
    bool timeOver = m_delay != 0 ? (now_c - m_saveTime) >= m_delay : false;

    if (!bForced && !rowCountOver && !timeOver)
        return;

    char timestr[100];
    std::strftime(timestr, 300, "%Y%m%d%H%M%S", std::localtime(&now_c));
    m_saveCounter++;

    if (now_c == m_saveTime)
    {
        if (m_saveCounter > 0)
        {
            char buffer[200];
            sprintf(buffer, "%s_%d", timestr, m_saveCounter);
            strcpy(timestr, buffer);
        }
    }
    else
    {
        m_saveCounter = 0;
    }

    std::string fileName = m_outputDir + "/" + std::string(timestr) + ".cdb";

    try
    {
        m_table.save(fileName);
    }
    catch (const std::exception &pExcept)
    {
        ++m_ErrorFS;
        m_pnode->logger()->error(pExcept.what());
    }

    m_table.flushTbl();

    m_saveTime = now_c;
}

void Json2Click::lifeProc()
{
    char *buffer = new char[65536];
    while (m_run.load(std::memory_order_acquire))
    {

        std::filesystem::path fileName;
        if (m_pQueue->wait_dequeue_timed(fileName, std::chrono::milliseconds(1000)))
        {
            if (!fileName.empty())
            {
                bool bOk = true;
                // парсим файл
                FILE *fl = fopen(fileName.string().c_str(), "r");
                if (!fl)
                {
                    m_pnode->logger()->error("Error at open file '{}'", fileName.string());
                    continue;
                }
                int counter = 0;
                int readed = 0;
                while (!feof(fl))
                {
                    char *rr = fgets(buffer, 65536, fl);
                    if (!rr)
                        continue;

                    counter++;

                    readed += strlen(buffer);

                    MyHandler handler(m_table);
                    rapidjson::Reader reader;
                    rapidjson::StringStream ss(buffer);
                    try
                    {
                        ParseResult res = reader.Parse(ss, handler);
                        if (res.IsError())
                        {
                            m_table.finishAppend(false);
                            m_pnode->logger()->error("Error at parse json file:'{}'; line:{} offset:{}", fileName.string(), counter, (int)res.Offset());
                            ++m_ErrorJSON;
                            bOk = false;
                        }
                        else
                        {
                            m_table.finishAppend(true);
                        }
                    }
                    catch (...)
                    {
                        // flush last row
                        m_table.finishAppend(false);
                        m_pnode->logger()->error("Error at parse json file:'{}'; line:{}", fileName.string(), counter);
                        ++m_ErrorJSON;
                        bOk = false;
                    }

                    if (m_blockSize != 0)
                    {
                        //проверка на количество строк происходит внутри dump()
                        dump();
                    }
                }
                fclose(fl);

                // сбрасываем метрику
                if (!m_metrics.empty())
                {
                    m_Rows += counter;
                    m_DataSize += readed;
                }

                // todo сохранять файлы во временную директорию, пока они не обработаются модулем click2db. Потом грохать
                // скорее всего для этого сделать клас с callback, проскольку модулей click2db может быть несколько, а сохранять нужно
                // если гдето произошла ошибка

                if (m_delay == 0 && m_blockSize == 0)
                {
                    // не задан временной интервал и количество строк, сохраням по файлу
                    std::filesystem::path outFileName = std::filesystem::path(m_outputDir) / fileName.filename();

                    try
                    {
                        m_table.save(outFileName.string() + ".cdb");
                    }
                    catch (const std::exception &pExcept)
                    {
                        bOk = false;
                        ++m_ErrorFS;
                        m_pnode->logger()->error(pExcept.what());
                    }

                    m_table.flushTbl();
                }

                if (!m_pnode->moveFile(bOk ? m_backupDir : m_failedDir, fileName))
                    ++m_ErrorFS;
            }
        }
        if (m_delay != 0)
        {
            //проверка на интервал происходит внутри dump()
            dump();
        }
    }

    if (m_delay != 0 || m_blockSize != 0)
    {
        //если в памяти есть данные, сохраняем
        dump(true);
    }

    delete[] buffer;
    m_pnode->logger()->debug("Json2Click::thread exit");
}
