#include <spdlog/spdlog.h>


class PidFile
{
    int pidFilehandle;

public:
    ~PidFile()
    {
        if (pidFilehandle != 0)
            close(pidFilehandle);
    }
    PidFile(bool use,const std::string &pidfile) : pidFilehandle(0)
    {
        if (!use)
        {
            spdlog::info("Checking pid file is disabled");
            return;
        }

        /* Ensure only one copy */
        pidFilehandle = open(pidfile.c_str(), O_RDWR | O_CREAT, 0600);

        if (pidFilehandle == -1)
        {
            /* Couldn't open lock file */
            spdlog::critical("Could not open PID lock file {}, exiting. For running whithout PID lock set \"-pid_lock 0\"", pidfile);
            exit(EXIT_FAILURE);
        }

        /* Try to lock file */
        if (lockf(pidFilehandle, F_TLOCK, 0) == -1)
        {
            /* Couldn't get lock on lock file */
            spdlog::critical("Could not lock PID file {}, exiting", pidfile);
            exit(EXIT_FAILURE);
        }

        char str[10];
        /* Get and format PID */
        sprintf(str, "%d\n", getpid());

        /* write pid to lockfile */
        if(write(pidFilehandle, str, strlen(str))!=strlen(str))
        {
            /* Couldn't get lock on lock file */
            spdlog::critical("Error at write pid to lock file {}, exiting", pidfile);
            exit(EXIT_FAILURE);
        }
        spdlog::debug("Lock PID file '{}' succeeded",pidfile);
    }
};
