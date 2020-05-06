cmake_minimum_required(VERSION 2.4)
set( PROJECT_NAME jsontest)
project( ${PROJECT_NAME} )

set( SPEC_LIB ${SPEC_LIB} 
		rt 
		)

#set(CMAKE_CXX_FLAGS "-O3 -g3 -ggdb -std=c++11")
#set(CMAKE_CXX_FLAGS "-g3 -ggdb -std=c++17")


set(Source_Files_src 
	main.cpp
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
