
```
CREATE TABLE IF NOT EXISTS db_flexetl.tb_table_views
(
    dt	 DateTime 	 DEFAULT now(),
	type	 LowCardinality(String),
	date	 Date,
	count	 UInt16,
	hash     Nullable(String)
) ENGINE = Log


CREATE TABLE IF NOT EXISTS db_flexetl.tb_table_clicks
(
    dt	 DateTime 	 DEFAULT now(),
	type	 LowCardinality(String),
	date	 Date,
	count	 UInt16,
	source   Nullable(String)
) ENGINE = Log

```