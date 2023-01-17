#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <grpcpp/grpcpp.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Define the protobuf message and service for the AppendEntries RPC
// ...

// Define the protobuf message and service for the RequestVote RPC
// ...

class RaftNode {
private:
    int id;
    NodeState state;
    int currentTerm;
    int votedFor;
    int leaderId;
    std::vector<int> log;
    std::vector<int> nextIndex;
    std::vector<int> matchIndex;
    std::vector <std::thread> workerThreads;
    std::mutex lock;
    std::unique_ptr <Server> server;

    // Helper function to become a candidate and start a new election
    void startElection();

    // Helper function to update the node's state based on the results of an election
    void updateState();

    // Service implementation for the AppendEntries RPC
    Status AppendEntries(ServerContext *context, const AppendEntriesRequest *req,
                         AppendEntriesResponse *res);

    // Service implementation for the RequestVote RPC
    Status RequestVote(ServerContext *context, const RequestVoteRequest *req,
                       RequestVoteResponse *res);

public:
    RaftNode(int id);

    // Function to start the Raft node
    void start();

    // Function to stop the Raft node
    void stop();
};

RaftNode::RaftNode(int id) {
    this->id = id;
    this->state = FOLLOWER;
    this->currentTerm = 0;
    this->votedFor = -1;
    this->leaderId = -1;
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(std::to_string(id), grpc::InsecureServerCredentials());
    // Register the service.
    builder.RegisterService(this);
    // Assemble the server.
    server = builder.BuildAndStart();
}

void RaftNode::start() {
    workerThreads.push_back(std::thread(&RaftNode::leaderElectionTimer, this));
    std::cout << "Server listening on " << id << std::endl;
    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

void RaftNode::stop() {
    server->Shutdown();
    for (std::thread &thread: workerThreads) {
        thread.join();
    }
}

Status RaftNode::AppendEntries(ServerContext *context, const AppendEntriesRequest *req,
                               AppendEntriesResponse *res) {
    std::unique_lock <std::mutex> lock(this->lock);
    if (state == FOLLOWER) {
        if (req->term() > currentTerm) {
            // this node's term is outdated, step down
            currentTerm = req->term();
            state = FOLLOWER;
            votedFor = -1;
        }
        // process the AppendEntries request
        // ...
        if (leaderId != -1 && leaderId != req->leader_id()) {
            // new leader has been elected
            leaderId = req->leader_id();
        }
    } else {
        // send an error response, since this node is not a follower
        res->set_success(false);
        res->set_term(currentTerm);
        return Status::OK;
    }
    res->set_success(true);
    res->set_term(currentTerm);
    lock.unlock();
    return Status::OK;
}

Status RaftNode::RequestVote(ServerContext *context, const RequestVoteRequest *req,
                             RequestVoteResponse *res) {
    std::unique_lock <std::mutex> lock(this->lock);
    if (state == FOLLOWER) {
        if (req->term() > currentTerm) {
            // this node's term is outdated, step down
            currentTerm = req->term();
            state = FOLLOWER;
            votedFor = -1;
        }
        if (req->term() == currentTerm && (votedFor == -1 || votedFor == req->candidate_id())) {
            // grant the vote
            res->set_term(currentTerm);
            res->set_vote_granted(true);
            votedFor = req->candidate_id();
        } else {
            res->set_term(currentTerm);
            res->set_vote_granted(false);
        }
        lock.unlock();
        return Status::OK;
    }
}

