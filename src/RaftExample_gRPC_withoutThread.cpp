// 3/9 DONE IN DS SLIDES RAFT IMPLEMENTATION

/*
 * 1) APPEND ENTRIES grpc olmadan implement             +
 * 2) Receiving msg from client kismi grpc (4/9)
 * 3) Commit log entries for leader implementation      +
 * 4) grpc ve proto hakkinda arastirma                  +
 * 5) deliver message to application yazilacak (disk)
 *
 */
#include <iostream>
#include <vector>
#include <unistd.h>
#include <unordered_map>
#include <algorithm>
#include <ctime>
#include <grpc++/grpc++.h>
#include "raft.grpc.pb.h"
#include <memory>
#include <thread>
#include <sstream>
#include <openssl/evp.h>
#include <chrono>
#include <iomanip>

#define PERIODIC_CHECK_SECONDS 5
#define UNUSED(expr) do { (void)(expr); } while (0)

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Status;
using grpc::Channel;

//grpc set for raft services
using raft::RequestVoteRequest;
using raft::RequestVoteResponse;
using raft::AddLogRequest;
using raft::AddLogResponse;
using raft::pair;
using raft::RaftService;

//grp set for client to raft node services
using raft::WriteRequest;
using raft::WriteResponse;
using raft::ReadRequest;
using raft::ReadResponse;
using raft::ClientService;

enum NodeState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

const std::string NodeStateStr[] = { "FOLLOWER", "CANDIDATE", "LEADER" };


class RaftNode final : public RaftService::Service, public ClientService::Service {
private:
    std::string id; //ip:port
    NodeState currentRole;
    int currentTerm;
    std::string votedFor;
    std::string currentLeader;
    int commitLength;
    time_t last_heartbeat_time;
    time_t last_election_time;
    double election_timout;
    double heartbeat_timout;
    std::unique_ptr<std::thread> periodicCheckThread; //thread that check state for each node periodically

    std::unordered_map <std::string, std::string> urlMap; //short->long or long->short
    std::vector <std::pair<std::string, int>> log; //{{msg, termNum}, ...} -> msg = long url
    std::unordered_map<std::string, int> sentLength;
    std::unordered_map<std::string, int> ackedLength;
    std::unique_ptr <Server> server;
    std::unordered_map<std::string, std::unique_ptr<RaftService::Stub>> nodes; //id: localhost:1234, Stub: to make a gRPC Call
    std::unordered_map<std::string, std::unique_ptr<ClientService::Stub>> nodesClientService; //id: localhost:1234, Stub: to make a gRPC Call on Write and Read

    // Helper function to become a candidate and start a new election
    void startElection();

    // Helper function to update the node's state based on the results of an election
    void updateState();

    bool isElectionTimout();

    bool isLeaderUnresponsive();

    void periodicCheck();

    void replicateLog(std::string leaderId, std::string followerId);

    void commitLogEntries();

    void appendEntries(int prefixLen, int leaderCommit, std::vector<pair> suffix);

    //GRPC'S
    // Service implementation for the AddLog RPC
    Status AddLog(ServerContext *context, const AddLogRequest *req, AddLogResponse *res) override;

    // Service implementation for the RequestVote RPC
    //voting on a new leader
    Status RequestVote(ServerContext *context, const RequestVoteRequest *req, RequestVoteResponse *res) override;

    // Service implementation for the Write RPC
    Status Write(ServerContext *context, const WriteRequest *req, WriteResponse *res) override;

    // Service implementation for the RequestVote RPC
    //voting on a new leader
    Status Read(ServerContext *context, const ReadRequest *req, ReadResponse *res) override;




    //UTIL FUNCTIONS
    double getRandomDouble(double fMin, double fMax);

    std::string generateShortUrl(const std::string longUrl);

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

    election_timout = getRandomDouble(10.0, 15.0); //election will time out after X seconds.
    heartbeat_timout = 10.0; //heartbeat timout to check leader unresponsive

    for (auto &curId: nodes_info) {
        auto channel = grpc::CreateChannel(curId, grpc::InsecureChannelCredentials());
        auto stub = RaftService::NewStub(channel);
        auto stubClientService = ClientService::NewStub(channel);
        nodes.emplace(curId, std::move(stub));
        nodesClientService.emplace(curId, std::move(stubClientService));
    }

    std::cout << "Nodes array of " << this->id << ":" << std::endl;
    for(auto &node : nodes) {
        std::cout << node.first << std::endl;
    }

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(this->id, grpc::InsecureServerCredentials());
    // Register the service.
    builder.RegisterService(static_cast<RaftService::Service*>(this));
    builder.RegisterService(static_cast<ClientService::Service*>(this));


    // Assemble the server.
    server = builder.BuildAndStart();

    std::cout << "Server listening on " << id << std::endl;
}

double RaftNode::getRandomDouble(double fMin, double fMax)
{
    double f = (double)rand() / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

bool RaftNode::isElectionTimout() {
    return (time(nullptr) - last_election_time) > election_timout;
}
bool RaftNode::isLeaderUnresponsive() {
    return (time(nullptr) - last_heartbeat_time) > heartbeat_timout;
}

void RaftNode::periodicCheck() {
    while (true) {
        // Check if the leader is still responsive
        updateState();
        std::this_thread::sleep_for(std::chrono::seconds(PERIODIC_CHECK_SECONDS));
    }
}

void RaftNode::start() {
    // Start the leader election thread
    periodicCheckThread = std::make_unique<std::thread>(&RaftNode::periodicCheck, this);

    // Wait for the server to shutdown.
    server->Wait();
}

void RaftNode::stop() {
    server->Shutdown();
}

void RaftNode::replicateLog(std::string leaderId, std::string followerId) {
    int prefixLen = sentLength[followerId]; //msg's we think followers already has
    std::vector <std::pair<std::string, int>> suffixTemp = {log.begin() + prefixLen,
                                                   log.end()}; //logs after prefix (followers dont have)

    int prefixTerm = 0;
    if (prefixLen > 0) {
        prefixTerm = log[(unsigned long)(prefixLen - 1)].second;
    }

    ClientContext context;
    AddLogRequest req;
    req.set_leader_id(leaderId); //our id
    req.set_term(this->currentTerm);
    req.set_prefix_len(prefixLen);
    req.set_prefix_term(prefixTerm);

    auto suffix = req.mutable_suffix();
    for(auto &p : suffixTemp) {
        pair *p1 = suffix->Add();
        p1->set_first(p.first);
        p1->set_second(p.second);
    }

    req.set_leader_commit(commitLength); //TODO check this is not used

    AddLogResponse res;
//    int nodeIndex = getNodeIndex(followerId);
    //leader receiving acks
    if (!((nodes[followerId]->AddLog(&context, req, &res)).ok())) {
        std::cout << "grpc error" << std::endl;
        return;
    }

    std::string follower = res.follower();
    int term = res.term();
    int ack = res.ack();
    bool success = res.success();

    std::cout << "Client response to Add Log" << std::endl;

    if (term == currentTerm && currentRole == LEADER) {
        if (success && ack >= ackedLength[follower]) { //follower acked the sent log
            sentLength[follower] = ack;
            ackedLength[follower] = ack;
            std::cout << "Client acked log -> comitting!" << std::endl;
            commitLogEntries();
            std::cout << "Comitting done!" << std::endl;
        } else if (sentLength[follower] > 0) { //follower is not sync try smaller log as a leader
            sentLength[follower]--; //decrement by 1 to find sync part with follower
            replicateLog(this->id, followerId);
        }
    } else if (term > currentTerm) { //follower has bigger term this node cant be leader any more
        currentTerm = term;
        currentRole = FOLLOWER;
        votedFor = "";
    }
}

//leader committing log entries
void RaftNode::commitLogEntries() {
    int minAcks = ((int)(nodes.size()) + 1) / 2; //quorum
    int ready = 0; //max acked length log which has quorum
    for (int i = 1; i <= (int)(log.size()); i++) //look acked lengths for each size of log
    {
        int matchedAckedNodeCount = 0;
        for (auto cur: ackedLength) { //traverse acked len of each node
            std::cout << "cur.second: " << std::endl;
            std::cout << "cur.second: " << cur.second << std::endl;
            if (cur.second >= i) {
                matchedAckedNodeCount++;
            }
        }
        if (matchedAckedNodeCount >= minAcks) { //is this length obtained quorum on nodes
            ready = i; //update max ready
        } else { //not possible obtain quorum on bigger lengths stop
            break;
        }
    }



    std::cout << "Log size: " << log.size() << std::endl;
    std::cout << "Ready var: " << ready << std::endl;
    if(ready != 0)
        std::cout << "Ready var in log: " << (log[(unsigned long)(ready - 1)].second) << std::endl;
    std::cout << "if condition: " << (ready != 0 && ready > commitLength && log[(unsigned long)(ready - 1)].second == currentTerm) << std::endl;
    if (ready != 0 && ready > commitLength && log[(unsigned long)(ready - 1)].second == currentTerm) {
//        for(int i = commitLength; i < ready; i++) {
//            //TODO deliver log[i].first (msg) to application
//        }
        commitLength = ready;
    }
}


//start election to become leader
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
    req.set_candidate_log_length((int)(log.size()));
    req.set_candidate_log_term(lastTerm);

    last_election_time = time(nullptr); //starting election timer
    //TODO: observe election timeout behaviour whether code waits in ok()?

    std::cout << "Starting eleection for " << std::endl;
    // Send RequestVote RPCs to all other nodes
    for (auto &node: nodes) {
        if (node.first == this->id) continue;
        ClientContext context;
        RequestVoteResponse res;
        std::cout << "Sending request to " << node.first << std::endl;
        if (((node.second)->RequestVote(&context, req, &res)).ok()) {
            std::cout << "Response from " << node.first << " -> " << "Term: " << res.term() << " Granted: " << res.granted() << std::endl;
            if (currentRole == CANDIDATE && res.term() == currentTerm && res.granted()) {
                votesReceived++;
                // If a majority of nodes vote for this node, become leader
                if (votesReceived > ((int)(nodes.size()) + 1) / 2) {
                    currentRole = LEADER;
                    currentLeader = id;

                    // Initialize sentLength and ackedLength for each node
                    for (auto &node: nodes) {
                        if (node.first == this->id) continue;
                        sentLength[node.first] = (int)(log.size());
                        ackedLength[node.first] = 0;
                        replicateLog(currentLeader, node.first);
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



//periodic check for each node
void RaftNode::updateState() {
    std::cout << "Current State of " << this->id << ": " << NodeStateStr[this->currentRole] << std::endl;
    if (currentRole == FOLLOWER) {
        if (isLeaderUnresponsive()) {
            startElection();
        }
    } else if (currentRole == CANDIDATE) {
        if (isElectionTimout()) { //TODO: implement election state
            startElection();
        }
    } else if (currentRole == LEADER) {
        // Send AppendEntries RPCs to all other nodes
        for (auto &node: nodes) {
            if (node.first == this->id) continue;
            replicateLog(this->id, node.first);
        }
    }
    std::cout << "Current State of " << this->id << ": " << NodeStateStr[this->currentRole] << std::endl;
}

//updating followers logs
void RaftNode::appendEntries(int prefixLen, int leaderCommit, std::vector<pair> suffix) {
    std::cout << "appendEntries start" << std::endl;
    if ((int)(suffix.size()) > 0 && (int)((this->log).size()) > prefixLen) { //you may have some redundant log from prev leader, delete them
        int index = std::min((int)((this->log).size()), prefixLen + (int)(suffix.size())) - 1;
        if (log[(unsigned long)index].second != suffix[(unsigned long)(index - prefixLen)].second()) {
            //TODO delete data from urlMap as well.
            log = {log.begin(), log.begin() + prefixLen - 1}; //truncate log to prefix
        } //truncated and cutted of inconsistent logs
    }
    //do we have new logs to add
    if (prefixLen + (int)(suffix.size()) > (int)(log.size())) {
        //add all suffix to log from back
        for(auto &p: suffix) {
            //TODO add urlMap too
            log.push_back({p.first(), p.second()});
            std::string urlMapping = p.first(); //longUrl,shortUrl
            //Parsing on ','
            std::size_t index = urlMapping.find(','); //index of comma
            std::string longUrl = urlMapping.substr(0,index);
            std::string shortUrl = urlMapping.substr(index+1);

            std::cout << "adding log to urlMap" << std::endl;

            urlMap[longUrl] = shortUrl;
            urlMap[shortUrl] = longUrl;
        }
    }

    if (leaderCommit > commitLength) {
//            for(int i = commitLength; i < leaderCommit; i++) {
//                //TODO commit log[i].first (msg) to disk (client)
//            }
        commitLength = leaderCommit;
    }
}



//followers receiving messages and appending new logs
Status RaftNode::AddLog(ServerContext *context, const AddLogRequest *req,
                        AddLogResponse *res) {

    UNUSED(context);
    std::string leaderId = req->leader_id();
    int term = req->term();
    int prefixLen = req->prefix_len();
    int prefixTerm = req->prefix_term();
    int leaderCommit = req->commit_length();
    std::vector<pair> suffix((req->suffix()).begin(), (req->suffix()).end());



    if (term > currentTerm) {
        currentTerm = term;
        votedFor = "";
        //cancel election timer ?
    }
    if (term == currentTerm) {
        currentRole = FOLLOWER;
        currentLeader = leaderId;
        last_heartbeat_time = time(nullptr); //heartbeat from leader
    }

    //compare log and candidates log
    bool logOk = ((int)(this->log).size() >= prefixLen) && (prefixLen == 0 || log[(unsigned long)(prefixLen - 1)].second == prefixTerm);

    res->set_follower(this->id);
    res->set_term(currentTerm);

    //follower updated log
    if (term == currentTerm && logOk) {
        appendEntries(prefixLen, leaderCommit, suffix);
        int ack = prefixLen + (int)(suffix.size()); //update prefix

        res->set_ack(ack);
        res->set_success(true);
    } else { //follower is not sync with previous entries or leader is no longer current leader
        res->set_ack(0);
        res->set_success(false);
    }

    return Status::OK;
}


//voting on a new leader
Status RaftNode::RequestVote(ServerContext *context, const RequestVoteRequest *req, RequestVoteResponse *res) {
    UNUSED(context);
//    std::cout << req->candidate_id() << " wants vote." << std:endl << "Requested term: " << req->candidate_term() << std::endl();
    if (req->candidate_term() > currentTerm) {
        // Update the node's term and step down
        currentTerm = req->candidate_term();
        currentRole = FOLLOWER;
        votedFor = "";
    }

    int lastTerm = 0;

    if (log.size() > 0) {
        lastTerm = log[log.size() - 1].second;
    }

    //compare log and candidates log
    bool logOk = (req->candidate_log_term() > lastTerm) ||
                 (req->candidate_log_term() == lastTerm && req->candidate_log_length() >= (int)(log.size()));


    std::cout << "Is log Ok: " << logOk << std::endl;
    std::cout << "is terms same: " << logOk << std::endl;
    std::cout << "Is log Ok: " << logOk << std::endl;
    std::cout << "Can Vote: " << (req->candidate_log_term() == currentTerm && logOk && (votedFor == "" || votedFor == req->candidate_id())) << std::endl;

    //not outdated term and we have out dated log and we didn't vote some other node
    if (req->candidate_term() == currentTerm && logOk && (votedFor == "" || votedFor == req->candidate_id())) {
        votedFor = req->candidate_id();
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





std::string RaftNode::generateShortUrl(const std::string longUrl) {
    // Get current timestamp
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // Combine timestamp and longURL
    std::stringstream ss;
    ss << timestamp << longUrl;
    // Hash the combined string using SHA-256
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit(mdctx, EVP_sha256());
    EVP_DigestUpdate(mdctx, ss.str().c_str(), ss.str().size());
    EVP_DigestFinal(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);
    // Convert the hash to a hex string
    std::stringstream hashStream;
    hashStream << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hash_len; i++) {
        hashStream << std::setw(2) << (int)hash[i];
    }
    // Take the first 6 characters of the hex string as the short URL
    return hashStream.str().substr(0, 6);
}




// Service implementation for the Write RPC
Status RaftNode::Write(ServerContext *context, const WriteRequest *req, WriteResponse *res) {
    UNUSED(context);
    if(currentRole == LEADER) { //do write request
        std::string longUrl = req->long_url();
        std::string shortUrl = "";

        if(urlMap.find(longUrl) != urlMap.end()) {
            shortUrl = urlMap[longUrl];
        }
        else {
            shortUrl = generateShortUrl(longUrl);
            urlMap[longUrl] = shortUrl;
            urlMap[shortUrl] = longUrl;

            log.push_back({longUrl+","+shortUrl, currentTerm}); //add curren mappint to log
        }

        res->set_long_url(longUrl);
        res->set_short_url(shortUrl);

        return Status::OK;
    }
    else if(currentRole == FOLLOWER) { //forward request to leader
        WriteRequest reqLeader;
        reqLeader.set_long_url(req->long_url());

        WriteResponse resLeader;
        ClientContext contextLeader;

        Status status = (nodesClientService[currentLeader])->Write(&contextLeader, reqLeader, &resLeader); //leader gRPC call
        if(status.ok()) { //leader returned result
            res->set_long_url(resLeader.long_url());
            res->set_short_url(resLeader.short_url());
            return Status::OK;
        }
        else {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Leader response error!");
        }
    }
    else if(currentRole == CANDIDATE) {
        //TODO leader not known we need to fail write request
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Unknown Leader");
    }
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "Unknown Error");
}

// Service implementation for the RequestVote RPC
//voting on a new leader
Status RaftNode::Read(ServerContext *context, const ReadRequest *req, ReadResponse *res) {
    UNUSED(context);
    std::string url = req->url();

    if(urlMap.find(url) != urlMap.end()) {
        res->set_url(url);
        res->set_result_url(urlMap[url]);
        return Status::OK;
    }

    return grpc::Status(grpc::StatusCode::NOT_FOUND, "URL Not Found!");
}



int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }
    int index = std::stoi(argv[1]);
    std::vector<std::string> nodes_info = {
            "127.0.0.1:50051",
            "127.0.0.1:50052",
            "127.0.0.1:50053",
            "127.0.0.1:50054"
    };


    RaftNode node(nodes_info[(unsigned long)index], nodes_info);
//    RaftNode node2(nodes_info[2], nodes_info);

    // Start the Raft node
    node.start();
//    node2.start();

    return 0;
}


