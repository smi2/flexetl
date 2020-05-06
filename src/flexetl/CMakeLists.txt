cmake_minimum_required(VERSION 2.4)
set( PROJECT_NAME flexetl)
project( ${PROJECT_NAME} )

set( SPEC_LIB ${SPEC_LIB} 
		rt 
		pthread 
		stdc++fs
		clickhouse-cpp-lib-static
		config++ 
		gflags 
		metrics
		)

set(Source_Files_src 
	backuper.cpp
	clicktbl.cpp
	click2db.cpp
	json2click.cpp
	flexetl.cpp
	pipeline.cpp
	pipeset.cpp
	pipebase.cpp
	main.cpp
	nodes.cpp
	watcher.cpp

)
set(Header_Files_src
	#flexetl.h
)

set(SOURCES 
	${Source_Files_src}
	${Header_Files_src}
)

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME}  ${SPEC_LIB})
