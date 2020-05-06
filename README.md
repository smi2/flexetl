# FlexETL for Clickhouse

Universl flexable json to clickhouse data uploader

FlexETL allows you to load bulk of json files into multiple Clickhouse clusters or servers.

# Features
- Watching .json files in directory and convert to binary
- Use binary Clickhouse protocol
- Watching change tables structure in database
- Supports many servers to send
- Sending metrics to graphite
- Sending collected data by interval
- Graceful shutdown


Program start params:

    -config_file (Config file) type: string default: "config"
    -log_level (Logging level (debug|info|err)) type: string default: "info"
    -log_max_file_size (Maximum log file size) type: uint32 default: 1048576
    -log_max_files (Maximum number of log files) type: uint32 default: 10
    -log_path (Path to log files) type: string default: "/tmp"
    -clean_backup (Maximum number of days to store the backup) type: uint32 default: 3

Sample:

    ./bin/flexetl -config_file ./config/test.cfg -log_level debug -server_name=devnode




Sample of config:

    ./config/test.cfg




# License

MIT Copyright (c) 2020 SMI2