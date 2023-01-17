set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Werror -fsanitize=address")

#
# Dependencies
#
find_package(Threads)

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

#[[
add_executable(main ${SOURCES})
target_link_libraries(main
        PRIVATE
        myproto
        )]]
#find_package(CURL REQUIRED)

add_executable(main
        main.cpp
        )
target_link_libraries(main
        PRIVATE
        myproto)

add_executable(raftExample
        RaftExample_gRPC_withoutThread.cpp
        )
target_link_libraries(raftExample
        PRIVATE
        myproto)
#[[
add_executable(worker
        worker.cpp
        CurlEasyPtr.cpp
        AzureBlobClient.cpp AzureBlobClient.h)
target_link_libraries(worker PUBLIC CURL::libcurl azure-storage-lite)]]
