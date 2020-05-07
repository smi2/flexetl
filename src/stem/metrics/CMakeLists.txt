
#include_directories(${CONTRIB_DIR}/wepoll/include)


add_library(metrics
        metrics.cpp
        )

target_link_libraries( metrics  
        libatomic.a
#        tcp_socket
)

