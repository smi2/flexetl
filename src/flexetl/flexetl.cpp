#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>
#include <iostream>
#include <libconfig.h++>
#include <gflags/gflags.h>

#include "flexetl.h"
#include "nodes.h"

FlexEtl::FlexEtl(std::shared_ptr<spdlog::logger> logger) : m_logger(logger), m_watcher(logger)
{
}
FlexEtl::~FlexEtl()
{
    m_logger->info("Stop");
}

void expandConfig(bool bExpand, libconfig::Setting &settings, std::map<std::string, std::string> &expandValues)
{
    libconfig::Setting::iterator itr = settings.begin();
    for (; itr != settings.end(); itr++)
    {
        if (itr->getType() == libconfig::Setting::TypeString)
        {
            std::vector<std::string> array;
            std::stringstream ss(itr->c_str());
            std::string item;
            while (std::getline(ss, item, '%'))
            {
                array.push_back(item);
            }
            if (array.size() == 1)
                continue;

            if (!bExpand)
            {
                // подготавливаем для парсинга

                for (int i = 0; i < array.size(); i++)
                {
                    if (i & 1)
                    {
                        expandValues.insert({array[i], ""});
                    }
                }
            }
            else
            {
                std::stringstream os;
                for (int i = 0; i < array.size(); i++)
                {
                    if (i & 1)
                    {
                        auto valItr = expandValues.find(array[i]);
                        if (valItr != expandValues.end() && !valItr->second.empty())
                        {
                            os << valItr->second;
                        }
                        else
                        {
                            std::stringstream err;
                            err << "No defined \"" << array[i] << "\" in config file at line: " << itr->getSourceLine();
                            throw std::runtime_error(err.str());
                        }
                    }
                    else
                    {
                        os << array[i];
                    }
                }
                itr->operator=(os.str());
            }
        }
        else if (itr->isList() || itr->isGroup())
        {
            expandConfig(bExpand, *itr, expandValues);
        }
    }
}

bool FlexEtl::init(const std::string &configFile, int cleanBackupDays)
{
    m_logger->info("Load {}", configFile);

    // Прочитать файл. Или выйти с ошибкой
    // Класс в С++ не возвращает ошибку, а кидает исключение
    try
    {
        cfg.readFile(configFile.c_str());
    }
    catch (const libconfig::FileIOException &fioex)
    {
        m_logger->error("I/O error while reading file: {}", fioex.what());
        return false;
    }
    catch (const libconfig::ParseException &pex)
    {
        m_logger->error("Parse error at file: {}; line: {}; cause: {}", pex.getFile(), pex.getLine(), pex.getError());
        return false;
    }

    const libconfig::Setting &root = cfg.getRoot();

    try
    {
        libconfig::Setting &handlersS = root["application"];

        std::map<std::string, std::string> expandValues;
        expandConfig(false, handlersS, expandValues);
        if (!expandValues.empty())
        {
            for (auto &prm : expandValues)
            {
                GFLAGS_NAMESPACE::FlagRegisterer(prm.first.c_str(), "", "", &prm.second, &prm.second);
            }
            GFLAGS_NAMESPACE::ReparseCommandLineNonHelpFlags();
            expandConfig(true, handlersS, expandValues);
        }

        std::string metricsHost;
        std::string metricsPort;
        int metricsInterval = 0;

        if (!handlersS.lookupValue("metricsPort", metricsPort))
        {
            metricsPort = "2003";
        }
        if (!handlersS.lookupValue("metricsInterval", metricsPort))
        {
            metricsInterval = 60;
        }
        metricsInterval = std::clamp(metricsInterval, 1, 3600);

        if (!handlersS.lookupValue("metricsHost", metricsHost))
        {
            metricsHost.clear();
            metricsPort.clear();
            metricsInterval = 0;
        }
        auto metrics = std::make_shared<stem::metrics>(metricsHost, std::stoi(metricsPort), "", m_logger);
        auto backuper = std::make_shared<Backuper>(cleanBackupDays, m_logger);

        auto rootNode = std::make_shared<Node>(m_logger, metrics, backuper);
        m_rootNode = rootNode;
        if (!rootNode->create(handlersS))
        {
            return false;
        }
        if (!rootNode->init(&m_watcher))
        {
            return false;
        }
        if (!rootNode->start())
        {
            return false;
        }
    }
    catch (const libconfig::SettingNotFoundException &nfex)
    {
        // Ignore.
        m_logger->error("FlexEtl::init failed: {}", nfex.what());
        return false;
    }
    catch (const std::exception &ex)
    {
        m_logger->error("FlexEtl::init failed: {}", ex.what());
        return false;
    }
    catch (...)
    {
        m_logger->error("FlexEtl::init failed");
        return false;
    }
    return true;
}

bool FlexEtl::IsRunning() const
{
    return run.load(std::memory_order_acquire);
}

bool FlexEtl::start()
{
    m_logger->info("Start");

    m_watcher.start();
    return true;
}
