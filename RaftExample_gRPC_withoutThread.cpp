// 3/9 DONE IN DS SLIDES RAFT IMPLEMENTATION

/*
 * 1) APPEND ENTRIES grpc olmadan implement
 * 2) Receiving msg from client kismi grpc yaz
 * 3) Commit log entries for leader implementation
 * 4) grpc ve proto hakkinda arastirma
 */
#include <iostream>
#include <vector>
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <unistd.h>
#include <unordered_map>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

enum NodeState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

// Define the protobuf message and service for the AppendEntries RPC
// ...

// Define the protobuf message and service for the RequestVote RPC
// ...

class RaftNode {
private:
    std::string id; //ip:port
    NodeState currentRole;
    int currentTerm;
    std::string votedFor;
    std::string currentLeader;
    int commitLength;
    //TODO add last_heartbeat_time
    //TODO add last_election_time
    //TODO add election_timout
    //TODO add heartbeat_timout
    std::unordered_map <string, string> urlMap; //short URL -> long URL and long to short
    std::vector <std::pair<string, int>> log; //{{msg, termNum}, ...} -> msg = long url
    std::unordered_map<string, int> sentLength;
    std::unordered_map<string, int> ackedLength;
    std::unique_ptr <Server> server;
    std::vector<RaftNode> nodes;

    // Helper function to become a candidate and start a new election
    void startElection();

    // Helper function to update the node's state based on the results of an election
    void updateState();

    // Service implementation for the AppendEntries RPC
    Status AppendEntries(ServerContext *context, const AppendEntriesRequest *req,
                         AppendEntriesResponse *res);

    // Service implementation for the RequestVote RPC
    //voting on a new leader
    Status RequestVote(ServerContext *context, const RequestVoteRequest *req,
                       RequestVoteResponse *res);

public:
    RaftNode(std::string id, std::vector <std::string> nodes_info);

    // Function to start the Raft node
    void start();

    // Function to stop the Raft node
    void stop();
};

RaftNode::RaftNode(std::string id, std::vector <std::string> nodes_info) {
    this->currentTerm = 0;
    this->votedFor = "";
    this->commitLength = 0;
    this->currentRole = FOLLOWER;
    this->currentLeader = "";
    this->id = id;

    for (auto &curId: nodes_info) {
        if (curId == this->id) {
            nodes.emplace_back(*this);
        } else {
            nodes.emplace_back(RaftNode(curId, nodes_info));
        }
    }

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(this->id, grpc::InsecureServerCredentials());
    // Register the service.
    builder.RegisterService(this);
    // Assemble the server.
    server = builder.BuildAndStart();

    std::cout << "Server listening on " << id << std::endl;
}

bool RaftNode::isLeaderResponsive() {
    return !(time(nullptr) - last_heartbeat_time > election_timeout);
}

void RaftNode::checkLeaderElection() {
    while (true) {
        // Check if the leader is still responsive
        updateState();
        std::this_thread::sleep_for(std::chrono::seconds(ELECTION_TIMEOUT));
    }
}

void RaftNode::start() {
    // Start the leader election thread
    leaderElectionThread = std::make_unique<std::thread>(&RaftNode::checkLeaderElection, this);

    // Wait for the server to shutdown.
    server->Wait();
}

void RaftNode::stop() {
    server->Shutdown();
}

void RaftNode::replicateLog(std::string leaderId, std::string followerId) {
    int prefixLen = sentLength[followerId]; //msg's we think followers already has
    std::vector<std::pair<string, int>> suffix = {log.begin() + prefixLen, log.end()}; //logs after prefix (followers dont have)

    int prefixTerm = 0;
    if(prefixLen > 0) {
        prefixTerm = log[prefixLen-1].second;
    }

    AddLogRequest req;
    req.set_leader_id(this->id); //our id
    req.set_term(this->currentTerm);
    req.set_prefix_len(prefixLen);
    req.set_prefix_term(prefixTerm);
    req.set_leader_commit(commitLength);
    req.set_suffix(suffix);

    AddLogResponse res;
    int nodeIndex = getNodeIndex(followerId);
    //leader receiving acks
    if (!nodes[nodeIndex].AddLog(req, &res).ok()) {
        std::cout << "grpc error" << std::endl:
        return;
    }

    std::string follower = res.follower();
    int term = res.term();
    int ack = res.ack();
    bool success = res.success();

    if(term == currentTerm && currentRole == leaderId) {
        if(success && ack >= ackedLength[follower]) { //follower acked the sent log
            sentLength[follower] = ack;
            ackedLength[follower] = ack;
            //TODO call commit log entries
        }
        else if(sentLength[follower] > 0) { //follower is not sync try smaller log as a leader
            sentLength[follower]--; //decrement by 1 to find sync part with follower
            replicateLog(this->id,followerId);
        }
    }
    else if(term > currentTerm) { //follower has bigger term this node cant be leader any more
        currentTerm = term;
        currentRole = follower;
        votedFor = "";
    }
}

int getNodeIndex(std::string nodeId) {
    for (int i = 0 ; i < nodes.size(); i++) {
        if (nodes[i].id = nodeId) {
            return i;
        }
    }
    return -1;
}

//followers receiving messages
Status RaftNode::AddLog(ServerContext *context, const AddLogRequest *req,
                               AddLogResponse *res) {
    std::string leaderId = req->leader_id();
    int term = req->term();
    int prefixLen = req->prefix_len();
    int prefixTerm = req->prefix_term();
    int leaderCommit = req->commit_length();
    std::vector<std::pair<string, int>> suffix = req->suffix();



    if (term > currentTerm) {
        currentTerm = term;
        votedFor = "";
        //cancel election timer ?
    }
    if(term = currentTerm) {
        currentRole = FOLLOWER;
        currentLeader = leaderId;
    }

    //compare log and candidates log
    bool logOk = ((this->log).size() >= prefixLen) && (prefixLen == 0 || log[prefixLen-1].second = prefixTerm);

    res->set_follower(this->id);
    res->set_term(currentTerm);

    //follower updated log
    if(term == currentTerm && logOk) {
        appendEntries(prefixLen, leaderCommit, suffix);
        int ack = prefixLen + suffix.size(); //update prefix

        res->set_ack(ack);
        res->set_success(true);
    }
    else { //follower is not sync with previous entries or leader is no longer current leader
        res->set_ack(0);
        res->set_success(false);
    }

    return Status::OK;
}


void RaftNode::startElection() {
    currentTerm++;
    currentRole = CANDIDATE;
    votedFor = id;

    int votesReceived = 1;
    int lastTerm = 0;


    if (log.size() > 0) {
        lastTerm = log[log.size() - 1].second;
    }

    RequestVoteRequest req;
    req.set_candidate_id(id); //our id
    req.set_candidate_term(currentTerm);
    req.set_candidate_log_length(log.size());
    req.set_candidate_log_term(lastTerm);

    last_election_time = time(nullptr); //starting election timer
    //TODO: observe election timeout behaviour whether code waits in ok()?

    // Send RequestVote RPCs to all other nodes
    for (auto &node: nodes) {
        if (node.id == this->id) continue;

        RequestVoteResponse res;
        if (node.RequestVote(req, &res).ok()) {
            if (currentRole == CANDIDATE && res.term() == currentTerm && res.granted()) {
                votesReceived++;
                // If a majority of nodes vote for this node, become leader
                if (votesReceived > nodes.size() / 2) {
                    currentRole = LEADER;
                    currentLeader = id;

                    // Initialize sentLength and ackedLength for each node
                    for (auto &node: nodes) {
                        if (node.id == this->id) continue;
                        sentLength[node.id] = log.size();
                        ackedLength[node.id] = 0;
                        replicateLog(currentLeader, node.id);
                    }
                    break; //we obtained majority no need to continue (quorum)
                }
            } else if (res.term() > currentTerm) { //some other node has larger term this node cant be a leader
                currentTerm = res.term();
                currentRole = FOLLOWER;
                votedFor = "";
                break; //stop informing other nodes to vote
            }
        }
    }
}




//TODO add grpc method to receive write request and replicate log again on leader




void RaftNode::updateState() {
    if (currentRole == FOLLOWER) {
        if (!isLeaderResponsive()) {
            startElection();
        }
    } else if (currentRole == CANDIDATE) {
        if (time(nullptr) - last_election_time > election_timeout) { //TODO: implement election state
            startElection();
        }
    } else if (currentRole == LEADER) {
        // Send AppendEntries RPCs to all other nodes
        for (auto &node: nodes) {
            if (node.id == this->id) continue;
            replicateLog(this->id, node.id);
//
//            AppendEntriesRequest req;
//            AppendEntriesResponse res;
//            req.set_term(currentTerm);
//            req.set_leader_id(id);
//            // Fill in the req with sentLength, log entries, etc
//            if (node.AppendEntries(req, &res).ok()) {
//                if (res.success()) {
//                    // Update ackedLength and sentLength for this node
//                    ackedLength[node.id] = sentLength[node.id] - 1;
//                    sentLength[node.id] = ackedLength[node.id] + 1;
//                } else {
//                    // Decrement sentLength for this node and try again
//                    sentLength[node.id]--;
//                }
//            }
        }
    }
}

void RaftNode::appendEntries(int prefixLen, int leaderCommit, std::vector<std::pair<string, int>> suffix) {
    //TODO implement without grpc
//    if (req->term() < currentTerm) {
//        // Send an error response, since this node's term is more up-to-date
//        res->set_success(false);
//        res->set_term(currentTerm);
//        return Status::OK;
//    }
//
//    // Update the node's term and leader
//    currentTerm = req->term();
//    currentLeader = req->leader_id();
//    currentRole = FOLLOWER;
//    last_heartbeat_time = time(nullptr);
//
//    // If the leader's log is up-to-date
//    if (req->prev_log_index() <= log.size() &&
//        req->prev_log_term() == log[req->prev_log_index()].term) {
//
//        // Delete any conflicting entries
//        log.erase(log.begin() + req->prev_log_index() + 1, log.end());
//
//        // Append new entries
//        for (int i = 0; i < req->entries_size(); i++) {
//            log.push_back(req->entries(i));
//        }
//
//        // Update the commit index
//        commitIndex = std::min(req->leader_commit(), log.size() - 1);
//        res->set_success(true);
//    } else {
//        // Send an error response, since the leader's log is not up-to-date
//        res->set_success(false);
//    }
//
//    res->set_term(currentTerm);
//    return Status::OK;
}

//voting on a new leader
Status RaftNode::RequestVote(ServerContext *context, const RequestVoteRequest *req,
                             RequestVoteResponse *res) {

    if (req->candidate_term() > currentTerm) {
        // Update the node's term and step down
        currentTerm = req->candidate_term();
        currentRole = FOLLOWER;
        votedFor = -1;
    }

    int lastTerm = 0;

    if (log.size() > 0) {
        lastTerm = log[log.size() - 1].second;
    }

    //compare log and candidates log
    bool logOk = (req->candidate_log_term > lastTerm) ||
                 (req->candidate_log_term == lastTerm && req->candidate_log_length >= log.size());


    //not outdated term and we have out dated log and we didn't vote some other node
    if (req->candidate_log_term == currentTerm && logOk && (votedFor == "" || votedFor == req->candidate_id)) {
        votedFor = req->candidate_id;
        res->set_voter_id(this->id); //voter id
        res->set_term(this->currentTerm);
        res->set_granted(true);
    } else { //current term is larger than candidate term or candidate log not up to date or we already voted for some other node
        res->set_voter_id(this->id); //voter id
        res->set_term(this->currentTerm);
        res->set_granted(false);
    }
    return Status::OK;
}





