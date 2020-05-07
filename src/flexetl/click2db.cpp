#include <iostream>

#include <clickhouse-cpp/client.h>
#include <clickhouse-cpp/error_codes.h>
#include <clickhouse-cpp/types/type_parser.h>
#include <clickhouse-cpp/columns/factory.h>

#include "click2db.h"

#define CONNECT_TIMEOUT 60


Click2DB::Click2DB() : m_run(false), m_pQueue(std::make_unique<Queue>(10000))
{
}

Click2DB::~Click2DB()
{
    if (m_run.load(std::memory_order_acquire))
    {
        m_run.store(false, std::memory_order_release);

        if (m_t.joinable())
            m_t.join();
    }
}

bool Click2DB::init(INode *pnode)
{
    m_pnode = pnode;
    if (!pnode->getStrParam("host", m_host))
    {
        m_pnode->logger()->error("Error at get host");
        return false;
    }
    if (!pnode->getStrParam("user", m_user))
    {
        m_pnode->logger()->error("Error at get user");
        return false;
    }
    if (!pnode->getStrParam("pass", m_password))
    {
        m_pnode->logger()->error("Error at get password");
        return false;
    }
    if (!pnode->getStrParam("table", m_tableName))
    {
        m_pnode->logger()->error("Error at get table");
        return false;
    }
    if (!pnode->getStrParam("tosend", m_toSend))
    {
        m_pnode->logger()->error("Error at path for sending files");
        return false;
    }

    pnode->getStrParam("failed", m_sendFailed);
    pnode->getStrParam("badfiles", m_loadFailed);
    pnode->getStrParam("metrics", m_metrics);
    if (!m_metrics.empty())
    {
        m_pnode->metrics()->connect(g_ErrorDB,"ErrorDB",m_metrics);
        m_pnode->metrics()->connect(g_ErrorFS,"ErrorFS",m_metrics);
        m_pnode->metrics()->connect(g_ErrorTypes,"ErrorTypes",m_metrics);
        m_pnode->metrics()->connect(g_InsertedRows,"InsertedRows",m_metrics);
    }

    if (!std::filesystem::exists(m_toSend))
    {
        std::filesystem::create_directories(m_toSend);
    }

    if (!m_sendFailed.empty() && !std::filesystem::exists(m_sendFailed))
    {
        std::filesystem::create_directories(m_sendFailed);
    }

    if (!m_loadFailed.empty() && !std::filesystem::exists(m_loadFailed))
    {
        std::filesystem::create_directories(m_loadFailed);
    }

    // перебираем папку tosend на предмет существующих файлов
    std::filesystem::directory_iterator end_itr;
    for (std::filesystem::directory_iterator itr(m_toSend); itr != end_itr; itr++)
    {
        if (!is_directory(itr->status()))
        {
            std::string fileName = m_toSend + "/" + itr->path().filename().string();
            m_pQueue->enqueue(std::make_pair(0, fileName));
        }
    }

    m_pnode->logger()->info("Clickhouse: {}:{}", m_host, m_tableName);
    return true;
}
bool Click2DB::start()
{
    m_run.store(true, std::memory_order_release);
    m_t = std::thread(&Click2DB::lifeProc, this);
    return true;
}

bool Click2DB::tryConnect()
{
    try
    {
        if (!m_pClickDB)
        {

            m_pClickDB = std::make_unique<clickhouse::Client>(clickhouse::ClientOptions()
                                                                  .SetHost(m_host)
                                                                  //.SetPingBeforeQuery(true)
                                                                  .TcpKeepAlive(true)
                                                                  .SetTcpKeepAliveIdle(30)
                                                                  .SetUser(m_user)
                                                                  .SetPassword(m_password)
                                                                  .SetCompressionMethod(clickhouse::CompressionMethod::LZ4));
            m_pnode->logger()->info("Connect to {} succeeded", m_host);
        }
    }
    catch (const std::exception &e)
    {
        m_pnode->logger()->warn("Connection with {} {}", m_host, e.what());
        m_tblType.clear();
        m_pClickDB.reset();
        return false;
    }
    return true;
}
void Click2DB::lifeProc()
{

    bool updateTblDesc = true;
    int emptyCounter = 0; // подсчет холостых тактов для активации проверки соединения
    while (m_run.load(std::memory_order_acquire))
    {
        if (!m_pClickDB && !tryConnect())
        {
            emptyCounter = 0; // сбрасываем счетчик холостых тактов
            
            for(int i=0;i<CONNECT_TIMEOUT && m_run.load(std::memory_order_acquire);i++)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            
            continue;
        }

        if (emptyCounter / 2 > CONNECT_TIMEOUT)
        {
            // время ожидания очереди 500 мс, поэтому количество
            // попыток прочитать из очереди деленное на 2 равно CONNECT_TIMEOUT
            // и если в течении этого времени не было обмена с базой
            // выполняем проверку соединения
            emptyCounter = 0; // сбрасываем счетчик холостых тактов
            try
            {
                m_pnode->logger()->debug("Ping to {}", m_host);
                m_pClickDB->Ping();
            }
            catch (const std::exception &pExecpt)
            {
                m_pnode->logger()->debug("Ping to {} failed: {}", m_host, pExecpt.what());
                // при проверке соединения произошла ошибка связанная с подключением
                m_pClickDB.reset();
                m_tblType.clear();
            }
        }

        if (updateTblDesc || m_tblType.empty())
        {
            emptyCounter = 0; // сбрасываем счетчик холостых тактов
            try
            {
                descibeTable();
            }
            catch (const std::exception &pExecpt)
            {
                m_pnode->logger()->error("Describe table {}:{} failed: {}", m_host, m_tableName, pExecpt.what());
                ++g_ErrorDB;
                // при попытке получить описание произошла ошибка связанная с подключением
                m_pClickDB.reset();
                m_tblType.clear();
            }
            updateTblDesc = m_tblType.empty();
        }

        if (m_tblType.empty())
        {
            updateTblDesc = true;
            std::this_thread::sleep_for(std::chrono::seconds(CONNECT_TIMEOUT));
            continue;
        }

        std::pair<int, std::filesystem::path> file;
        if (m_pQueue->wait_dequeue_timed(file, std::chrono::milliseconds(500)))
        {
            emptyCounter = 0; // сбрасываем счетчик холостых тактов
            ClickTable table;

            try
            {
                table.load(file.second.string());
            }
            catch (const std::exception &pExecpt)
            {
                ++g_ErrorFS;
                m_pnode->logger()->error("Error at load {}: {}, move file to bad dir", file.second.string(), pExecpt.what());
                if (!m_pnode->moveFile(m_loadFailed, file.second))
                    ++g_ErrorFS;
                continue;
            }

            try
            {
                sendTable(table);
                m_pnode->logger()->debug("Send {} rows from '{}' to {}::{} succeeded", table.rowCount(), file.second.string(), m_host, m_tableName);

                if (!m_pnode->moveFile("", file.second))
                    ++g_ErrorFS;
            }
            catch (const std::exception &pExecpt)
            {
                ++g_ErrorDB;

                file.first++;
                if (file.first < 3)
                {
                    m_pnode->logger()->warn("Send to {}:{} failed, try more'{}': {}", m_host, m_tableName, file.second.string(), pExecpt.what());
                    m_pQueue->enqueue(file);

                    // что то пошло не так, обновим информацию о таблице
                    updateTblDesc = true;
                }
                else
                {
                    m_pnode->logger()->error("Send to {}:{} failed, try count over '{}': {}", m_host, m_tableName, file.second.string(), pExecpt.what());
                    if (!m_pnode->moveFile(m_sendFailed, file.second))
                        ++g_ErrorFS;
                }
            }
        }
        else
        {
            emptyCounter++;
        }
    }
    m_pnode->logger()->debug("Click2DB::thread exit");
}

static void column2ClickColumn(const Value &inColumn, clickhouse::ColumnRef column)
{
    switch (column->Type()->GetCode())
    {
    case clickhouse::Type::Int8:
    {
        auto columnType = column->As<clickhouse::ColumnInt8>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getInt8());
        break;
    }
    case clickhouse::Type::Int16:
    {
        auto columnType = column->As<clickhouse::ColumnInt16>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getInt16());
        break;
    }
    case clickhouse::Type::Int32:
    {
        auto columnType = column->As<clickhouse::ColumnInt32>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getInt32());
        break;
    }
    case clickhouse::Type::Int64:
    {
        auto columnType = column->As<clickhouse::ColumnInt64>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getInt64());
        break;
    }
    case clickhouse::Type::Int128:
    {
        auto columnType = column->As<clickhouse::ColumnInt128>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getInt64());
        break;
    }
    case clickhouse::Type::UInt8:
    {
        auto columnType = column->As<clickhouse::ColumnUInt8>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getUInt8());
        break;
    }
    case clickhouse::Type::UInt16:
    {
        auto columnType = column->As<clickhouse::ColumnUInt16>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getUInt16());
        break;
    }
    case clickhouse::Type::UInt32:
    {
        auto columnType = column->As<clickhouse::ColumnUInt32>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getUInt32());
        break;
    }
    case clickhouse::Type::IPv4:
    {
        auto columnType = column->As<clickhouse::ColumnUInt32>();
        for (auto &el : *inColumn._vector)
        {
            columnType->Append(ntohl(inet_addr(el.getString().c_str())));
        }
        break;
    }
    case clickhouse::Type::UInt64:
    {
        auto columnType = column->As<clickhouse::ColumnUInt64>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getUInt64());
        break;
    }

    case clickhouse::Type::Float32:
    {
        auto columnType = column->As<clickhouse::ColumnFloat32>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getFloat());
        break;
    }
    case clickhouse::Type::Float64:
    {
        auto columnType = column->As<clickhouse::ColumnFloat64>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getDouble());
        break;
    }
    case clickhouse::Type::String:
    {
        auto columnType = column->As<clickhouse::ColumnString>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getString());
        break;
    }
    case clickhouse::Type::FixedString:
    {
        auto columnType = column->As<clickhouse::ColumnFixedString>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getString());
        break;
    }
    case clickhouse::Type::DateTime:
    {
        auto columnType = column->As<clickhouse::ColumnDateTime>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getDateTime());
        break;
    }
    case clickhouse::Type::Date:
    {
        auto columnType = column->As<clickhouse::ColumnDate>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getDate());
        break;
    }
    case clickhouse::Type::Array:
    {

        auto columnName = column->Type()->As<clickhouse::ArrayType>()->GetItemType()->GetName();
        auto columnType = column->As<clickhouse::ColumnArray>();
        for (auto &el : *inColumn._vector)
        {
            auto col = clickhouse::CreateColumnByType(columnName);
            column2ClickColumn(el, col);
            columnType->AppendAsColumn(col);
        }
        break;
    }
    case clickhouse::Type::Nullable:
    {
        auto nullableColumn = column->As<clickhouse::ColumnNullable>();
        auto nestedColunmn = nullableColumn->Nested();
        column2ClickColumn(inColumn, nestedColunmn);

        for (auto &el : *inColumn._vector)
        {
            nullableColumn->Append(el.isNull());
        }
        break;

        break;
    }
    case clickhouse::Type::Enum8:
    {
        auto columnType = column->As<clickhouse::ColumnEnum8>();
        for (auto &el : *inColumn._vector)
        {
            if (el.type() == CT_UINT)
                columnType->Append(el.getUInt64());
            else if (el.type() == CT_INT)
                columnType->Append(el.getInt64());
            else if (el.type() == CT_STR)
                columnType->Append(el.getString());
            else
            {
                throw std::runtime_error("convert to Enum8 failed");
            }
        }
        break;
    }
    case clickhouse::Type::Enum16:
    {
        auto columnType = column->As<clickhouse::ColumnEnum16>();
        for (auto &el : *inColumn._vector)
        {
            if (el.type() == CT_UINT)
                columnType->Append(el.getUInt64());
            else if (el.type() == CT_INT)
                columnType->Append(el.getInt64());
            else if (el.type() == CT_STR)
                columnType->Append(el.getString());
            else
            {
                throw std::runtime_error("convert to Enum16 failed");
            }
        }
        break;
    }
    case clickhouse::Type::Decimal32:
    case clickhouse::Type::Decimal64:
    case clickhouse::Type::Decimal128:
    {
        auto columnType = column->As<clickhouse::ColumnDecimal>();
        for (auto &el : *inColumn._vector)
        {
            if (el.type() == CT_UINT)
                columnType->Append(el.getUInt64());
            else
                columnType->Append(el.getInt64());
        }
        break;
    }
    case clickhouse::Type::IPv6:
    {
        auto columnType = column->As<clickhouse::ColumnFixedString>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getString());
        break;
    }
    //  case clickhouse::Type::Tuple:
    //  {
    //      auto columnType = column->As<clickhouse::ColumnTuple>();
    //      break;
    //  }
    case clickhouse::Type::UUID:
    {
        auto columnType = column->As<clickhouse::ColumnUUID>();
        for (auto &el : *inColumn._vector)
            columnType->Append(el.getUInt128());
        break;
    }
    default:
        throw std::runtime_error("unsupported clickhouse type");
    }
}

void Click2DB::sendTable(const ClickTable &table)
{
    // блок заполнения таблицы
    auto fillTable = [this](const ClickTable &table, clickhouse::Block &block) {
        bool hasUnknownTypes = false;
        for (auto &col : table.m_tbl)
        {
            auto typeItr = m_tblType.find(col.first);
            if (typeItr == m_tblType.end())
            {
                m_pnode->logger()->warn("No column '{}' in {}:{}", col.first, m_host, m_tableName);
                ++g_ErrorTypes;
                hasUnknownTypes = true;
                continue;
            }
            clickhouse::ColumnRef column = clickhouse::CreateColumnByType(typeItr->second);
            if (column)
            {
                try
                {
                    column2ClickColumn(col.second._data, column);
                }

                catch (std::exception &ex)
                {

                    m_pnode->logger()->warn("Error at convert {} to CH type {}: {}", col.first, typeItr->second, ex.what());
                    ++g_ErrorTypes;
                    continue;
                }

                block.AppendColumn(col.first, column);
            }
            else
            {
                m_pnode->logger()->error("create column '{}' aka '{}'  in {}:{} failed", col.first, typeItr->second, m_host, m_tableName);
                ++g_ErrorTypes;
            }
        }
        return hasUnknownTypes;
    };

    clickhouse::Block block;

    if (fillTable(table, block))
    {
        // в таблице есть неизвестные элементы, пробуем обновить описание таблицы
        if (descibeTable())
        {
            // описание поменялось, перезаполняем таблицу
            block = clickhouse::Block();
            fillTable(table, block);
        }
    }

    if (block.GetColumnCount() == 0)
    {
        throw std::runtime_error("no column om table");
    }
    if (block.GetRowCount() == 0)
    {
        throw std::runtime_error("no rows om table");
    }

    auto beginTime = std::chrono::steady_clock::now();
    m_pClickDB->Insert(m_tableName, block);
    auto endTime = std::chrono::steady_clock::now();
    auto queryTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime).count();

    // сбрасываем метрику
    g_InsertedRows+=table.rowCount();
    //m_pnode->metrics()->push(m_metrics, "InsertedTime", queryTime);

    return;
}

bool Click2DB::descibeTable()
{
    if (!m_pClickDB)
    {
        throw std::runtime_error("Describe failed: no connection");
    }

    auto beginTime = std::chrono::steady_clock::now();
    std::map<std::string, std::string> tblType;
    clickhouse::Query query(std::string("INSERT INTO " + m_tableName + " VALUES"));
    m_pClickDB->Execute(query.OnDataCancelable([&tblType](const clickhouse::Block &desc) {
        for (clickhouse::Block::Iterator bi(desc); bi.IsValid(); bi.Next())
        {
            auto name = bi.Name();
            auto typeName = bi.Type()->GetName();
            tblType[name] = typeName;
        }
        return false;
    }));

    m_tblType.swap(tblType);
    bool isChanged = m_tblType != tblType;

    auto endTime = std::chrono::steady_clock::now();
    auto queryTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime).count();

    if (tblType.empty() && !m_tblType.empty())
    {
        m_pnode->logger()->debug("Describe table '{}:{}' succeeded by {} msec", m_host, m_tableName, queryTime);
    }
    else if (isChanged)
    {
        m_pnode->logger()->debug("Table description '{}:{}' is updated  by {} msec", m_host, m_tableName, queryTime);
    }
    else
    {
        m_pnode->logger()->debug("Describe table '{}:{}': no changes by {} msec", m_host, m_tableName, queryTime);
    }
    return isChanged;
}
bool Click2DB::doProc(const std::filesystem::path &inFile)
{
    std::filesystem::path toFile = std::filesystem::path(m_toSend) / inFile.filename();

    std::error_code err;
    std::filesystem::copy_file(inFile, toFile, err);
    if (err)
    {
        m_pnode->logger()->warn("Copy file failed ({}): {} -> {}", err.message(), inFile.string(), toFile.string());
        return false;
    }
    m_pQueue->enqueue(std::make_pair(0, toFile));
    return true;
}
