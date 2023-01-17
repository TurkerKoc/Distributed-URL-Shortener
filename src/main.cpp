#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

#include <raft.grpc.pb.h>


int main(/*int argc, char** argv*/) {

    std::vector<std::string> nodes_info = {
            "127.0.0.1:50051",
            "127.0.0.1:50052",
            "127.0.0.1:50053"
    };

    /*
    RaftNode node(nodes_info[0], nodes_info);
    RaftNode node1(nodes_info[1], nodes_info);
    RaftNode node2(nodes_info[2], nodes_info);

    // Start the Raft node
    node.start();
    node1.start();
    node2.start();
     */

    return 0;
}
