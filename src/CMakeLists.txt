set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Werror -fsanitize=address")

#
# Dependencies
#
find_package(Threads)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
#
# Sources
#
set(SOURCES
        ./main.cpp
        )

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

#
# Target
#

add_executable(main
        main.cpp
        )
target_link_libraries(main
        PRIVATE
        myproto)

add_executable(raft
        raft.cpp
        )
target_link_libraries(raft
        PRIVATE
        myproto
        ${OPENSSL_LIBRARIES})

add_executable(client
        client.cpp
        )
target_link_libraries(client
        PRIVATE
        myproto)

add_executable(loadBalancer
        loadBalancer.cpp
        )
target_link_libraries(loadBalancer
        PRIVATE
        myproto)
