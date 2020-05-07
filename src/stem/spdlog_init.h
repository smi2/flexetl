#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace stem
{

DEFINE_string(log_path, "/tmp", "Path to log files");
DEFINE_string(log_level, "info", "Logging level (debug|info|err)");
DEFINE_uint32(log_max_file_size, 1 * 1024 * 1024, "Maximum log file size");
DEFINE_uint32(log_max_files, 10, "Maximum number of log files");

/*
The first call sets the default spdlog.
*/
static std::shared_ptr<spdlog::logger> spdlogInit(const std::string &path = FLAGS_log_path,
                                                  const std::string &level = FLAGS_log_level,
                                                  int maxFiles = FLAGS_log_max_files,
                                                  int maxFileSize = FLAGS_log_max_file_size)
{
    try
    {
        std::vector<spdlog::level::level_enum> logLevels{
            spdlog::level::level_enum::debug,
            spdlog::level::level_enum::info,
            spdlog::level::level_enum::err};

        std::shared_ptr<spdlog::logger> logger;

        spdlog::level::level_enum defaultLevel = spdlog::level::from_str(level);

        std::vector<spdlog::sink_ptr> sinks;

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(defaultLevel);

        sinks.push_back(console_sink);

        for (const auto &logLevel : logLevels)
        {
            if (logLevel >= defaultLevel)
            {
                auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    path + "/" + std::string(spdlog::level::to_string_view(logLevel).data()) + ".log",
                    maxFileSize, maxFiles);
                sink->set_level(logLevel);

                sinks.push_back(sink);
            }
        }

        logger = std::make_shared<spdlog::logger>("flexetl", std::begin(sinks), std::end(sinks));
        logger->set_level(defaultLevel);

        logger->flush_on(spdlog::level::info);

        static bool bDefault = true;
        if (bDefault)
        {
            bDefault = false;
            spdlog::set_default_logger(logger);
        }

        return logger;
    }
    catch (const spdlog::spdlog_ex &e)
    {
        std::fprintf(stderr, "Failed to initialize the logger: %s\n", e.what());
        return nullptr;
    }
}

} // namespace stem