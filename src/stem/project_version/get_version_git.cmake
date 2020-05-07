
# Макрос add_version_git.
# Создает цель version_target, которая записывает в корневую директорию ROOT_DIRECTORY файл version_info.h
# с версией, сформированной по принципу, описанному в cmd_get_version_git.cmake



set( BS_SCRIPT_PATH ${CMAKE_CURRENT_LIST_DIR})
macro( add_version_git VERSION_INFO_PATH)

include_directories(${VERSION_INFO_PATH})


find_package(Git REQUIRED)
set(VERSION_INFO_FILE ${VERSION_INFO_PATH}/project_version.h)
message( STATUS "Version information: ${VERSION_INFO_FILE}" )

add_custom_target( VersionInfo ALL
	COMMAND ${CMAKE_COMMAND}
	-DVERSION_FILE=${VERSION_INFO_FILE}
	-P ${BS_SCRIPT_PATH}/cmd_get_version_git.cmake )

# Для первоначальной сборки
execute_process(COMMAND ${CMAKE_COMMAND}
	-DVERSION_FILE=${VERSION_INFO_FILE}
	-P ${BS_SCRIPT_PATH}/cmd_get_version_git.cmake )
	
endmacro( add_version_git)
