

# Записывает в файл output_file текущую версию в зависимости от состояния git
#  состояние							запись				пример
# 1. Есть тэг, нет изменений            имя тэга			v3.0
# 2. Есть тэг, есть изменения           имя тэга-dirty		v3.0-dirty
# 3. Нет тэга, нет изменений            имя ближайшего по	v3.0-a9bfc
#                                       дате тэга-краткий
#                                       хэш комита
# 4. Нет тэга, есть изменения           имя ближайшего по	v3.0-a9bfc-dirty
#                                       дате тэга-краткий
#                                       хэш комита-dirty

execute_process( COMMAND git tag --points-at HEAD OUTPUT_VARIABLE REVISION OUTPUT_STRIP_TRAILING_WHITESPACE)
if( NOT REVISION )
	execute_process( COMMAND git log -n 1 --format=%h OUTPUT_VARIABLE REVISION_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process( COMMAND git tag -l --merged=HEAD --sort=-creatordate OUTPUT_VARIABLE MERGED_TAGS OUTPUT_STRIP_TRAILING_WHITESPACE )
	
	string( REPLACE "\n" ";" MERGED_TAGS_LIST ${MERGED_TAGS} )

	list(GET MERGED_TAGS_LIST 0 LAST_TAG)
	
	set( REVISION "${LAST_TAG}-${REVISION_HASH}")
endif( NOT REVISION )

execute_process( COMMAND git status --porcelain -uno OUTPUT_VARIABLE DIRTY_STAT )
if( DIRTY_STAT )
	set(REVISION "${REVISION}-dirty")
endif ( DIRTY_STAT )

if( DEFINED VERSION_FILE )
	file( WRITE ${VERSION_FILE} "#pragma once")

	file( WRITE ${VERSION_FILE} "#pragma once\n\n#define __PROJECT_VERSION__ " \" ${REVISION}\")
	message( STATUS "Revision: ${REVISION} written to ${VERSION_FILE}")
else( DEFINED VERSION_FILE )
	message( WARNING "VERSION_FILE not defined")
	message( STATUS "Revision: ${REVISION}.")
endif( DEFINED VERSION_FILE )	