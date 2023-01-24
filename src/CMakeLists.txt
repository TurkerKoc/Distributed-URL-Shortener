set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Werror -fsanitize=address")

#
# Dependencies
#
find_package(Threads)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# sqlite3
include_directories(/usr/include)
link_directories(/usr/lib/sqlite3)

#
# Targets
#

# main
add_executable(main
        main.cpp
        )
target_link_libraries(main
        PRIVATE
        myproto)

# raft
add_executable(raft
        raft.cpp
        )
target_link_libraries(raft
        PRIVATE
        myproto
        ${OPENSSL_LIBRARIES})

# client
add_executable(client
        client.cpp
        )
target_link_libraries(client
        PRIVATE
        myproto)

# load balancer
add_executable(loadBalancer
        loadBalancer.cpp
        )
target_link_libraries(loadBalancer
        PRIVATE
        myproto)

# sqlite test
add_executable(SQLiteExample
        SQLiteExample.cpp
        )
target_link_libraries(SQLiteExample
        PRIVATE
        myproto
        sqlite3)
