#include <stdio.h>
#include <iostream>
#include <signal.h>
#include <cstdlib>
#include <csignal>
#include <gflags/gflags.h>

#include "project_version.h"

#include "flexetl.h"

#include "stem/spdlog_init.h"
#include "stem/pid_file.h"

DEFINE_string(config_file, "config", "Config file");
DEFINE_uint32(clean_backup, 3, "Maximum number of days to store the backup");
DEFINE_uint32(pid_lock, 1, "Using Pid file for check running. For disable checking set -pid_file 0");
DEFINE_string(pid_file, "/var/run/flexetl.pid", "Pid file for check running.");

int main(int argc, char **argv)
{

    if (argc < 2)
    {
        gflags::ShowUsageWithFlags(argv[0]);
        exit(EXIT_FAILURE);
    }

    gflags::SetVersionString(__PROJECT_VERSION__);
    gflags::SetUsageMessage("flexetl <params>");
    GFLAGS_NAMESPACE::AllowCommandLineReparsing();
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

    std::shared_ptr<spdlog::logger> logger = stem::spdlogInit("flexetl");

    PidFile pidFile(FLAGS_pid_lock, FLAGS_pid_file);

    logger->trace("\n{}", std::string(gflags::GetArgv()));

    FlexEtl flexEtl(logger);

    if (!flexEtl.init(FLAGS_config_file, 24 * FLAGS_clean_backup))
    {
        exit(EXIT_FAILURE);
    }

    flexEtl.start();

    sigset_t sset;
    int sig;
    sigfillset(&sset);
    //sigdelset(&sset, SIGTERM);
    sigprocmask(SIG_SETMASK, &sset, 0);
    while (!sigwait(&sset, &sig))
    {
        logger->error("Signal {} - {}", sig, std::string(sys_siglist[sig]));
        if (sig == SIGTERM || sig == SIGINT)
            break;
        // switch (sig)
        // {
        // case SIGHUP:
        //      flexEtl.reloadConfig();
        //     break;
        // default:
        //     break;
        // }
    }
    logger->error("Exit");
    return 0;
}
