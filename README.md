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
    -pid_file (Pid file for check running.) type: string default: "/var/run/flexetl.pid"
    -pid_lock (Using Pid file for check running. For disable checking set -pid_file 0) type: uint32 default: 1

Sample:

    ./bin/flexetl -config_file ./example/test.cfg -log_level debug -server_name=devnode


Sample of config:

    ./example/test.cfg


# Example

1. PHP write to two type json files
2. FlexETL watching directory
3. FlexETL write to ch-cluster1 and ch-cluster2


Example writers:

```php
file_put_contents('/tmp/flexEtlTest/TableViews/20200101.json'.,json_encode(
        [
            'dt'=>time(),
            'type'=>'view',
            'date'=>date('Y-m-d'),
            'count'=>$f,
            'hash'=>md5($f),
            'a'=>'AAA',
            'b'=>'BBBB'
        ]
    ));

```

Create table in clickhouse:

```
CREATE TABLE IF NOT EXISTS db_flexetl.tb_table_views
(
    dt	 DateTime 	 DEFAULT now(),
	type	 LowCardinality(String),
	date	 Date,
	count	 UInt16,
	hash     Nullable(String)
) ENGINE = Log
```

Use FlexETL config for two clickhouse hosts

* clickhouse1-1.host2.net
* clickhouse2-2.host3.net



```
version = "1.0";
application:
{
    metricsHost="graphite.host.net";  // graphite host
    //metricsPort="2003";             // graphite port, default 2003
    //metricsInterval="60";           // agregate time, default 60 sec
nodes:
(
           {
                handler="json2click";                           // name handler
                metrics="flexetl.json.";       // root metrics path
                watch="/tmp/flexEtlTest/TableViews/";                  // [required] path for watching .json files
                failed="/tmp/json2click/input/failed";          // [optional] path for bad input files
                succeeded="/tmp/json2click/input/succeeded";    // [optional] path to backup true files
                output="/tmp/json2click/output";                // [required] path for out files in .cdb format
                delay=60;                                       // [optional] time period in second for save out file
                blockSize=1000;                                 // [optional] max row count for save out file
            },
            {
                handler="pipeset";                              // name handler
                watch="/tmp/json2click/output";                 // [required] path for watching .cdb files
                nodes:
                (
                   {
                        handler="click2db";                             // name handler
                        metrics="flexetl.%server_name%.xad_request.clb.";        // root metrics path
                        tosend="/tmp/json2click/output/1";              // [required] intermidiate dir for sending files
                        failed="/tmp/json2click/output/1/failed";       // [optional] path for files not sended to CH
                        badfiles="/tmp/json2click/output/1/badfiles";   // [optional] path for bad input files
                        host="clickhouse2-2.host3.net";
                        port="9000";
                        user="default";
                        pass="12345678"
                        table="test.xad_request";                       // [required] batabase_name.table_name
                    },
                    {
                        handler="click2db";                             // name handler
                        metrics="flexetl.%server_name%.xad_request.clb.";        // root metrics path
                        tosend="/tmp/json2click/output/2";              // [required] intermidiate dir for sending files
                        failed="/tmp/json2click/output/2/failed";       // [optional] path for files not sended to CH
                        badfiles="/tmp/json2click/output/2/badfiles";   // [optional] path for bad input files
                        host="clickhouse1-1.host2.net";
                        port="9000";
                        user="default";
                        pass="23421232"
                        table="default.xad_request";
                    }
                )
            }
);
}

```

# License

MIT Copyright (c) 2020 SMI2
