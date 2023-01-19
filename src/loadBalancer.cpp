// read, write methodunu grpc yazicaz
// mainde RaftNode objelerini olusturup thread olarak bunlari calistiricaz
// sonra grpc dinlemeye baslicak ve ne zaman bir istek gelse sirasiyla nodelara yonlendiricez
// + write request RafNodeda hali hazirda leadera forward olacagi icin lider bilgisini buranin tutmasina gerek yok
// eger herhangi bir istekte hata alirsak diger nodea istek aticaz


//+   Raft icinde redundant logu silme isleminde urlMap ten silmen lazim
// 5) deliver message to application yazilacak (disk) yapilacak -> SQLITE

//Load balancer icinde read/write quorum saglamasini yap


/*
 * - We assume that load balancer will not fail
 * - Load balancer can get Read and Write requests from client -> develop grpc functions
 * - Load balancer can request Read and write to all available RaftNodes
 * - Load balancer need to have quorum on nodes when reading urls. -> R + W > N
 * - On failed read or write try another node.
 * - We are not keeping track of the leader -> send every new request to the next known node. (follower nodes will forward write requests to the leader node)
 *
 *
 * - Later on we can add coordinator feature to load balancer to start every RaftNodes and use them as a separate threads for clients.
 */

#include <raft.grpc.pb.h>
#include <iostream>
#include <string>
#include <vector>
#include <grpcpp/grpcpp.h>

#define UNUSED(expr) do { (void)(expr); } while (0)

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Status;
using grpc::Channel;

//grp set for client to raft node services
using raft::WriteRequest;
using raft::WriteResponse;
using raft::ReadRequest;
using raft::ReadResponse;
using raft::ClientService;

class LoadBalancer : public ClientService::Service {
private:
    std::string id; // ip:port
    std::unique_ptr <Server> server;
    // TODO check here
    //  I changed here to vector for better random access (with index)
    std::vector<std::unique_ptr<ClientService::Stub>> nodesClientService; //id: localhost:1234, Stub: to make a gRPC Call on Write and Read
    int readOrder;
    int writeOrder;
public:
    LoadBalancer(std::string id, std::vector <std::string> nodes_info);

    // Function to start the Raft node
    void start();

    // Function to stop the Raft node
    void stop();

    Status Write(ServerContext *context, const WriteRequest *req, WriteResponse *res) override;

    Status Read(ServerContext *context, const ReadRequest *req, ReadResponse *res) override;
};


LoadBalancer::LoadBalancer(std::string id, std::vector <std::string> nodes_info) {
    this->id = id;
    this->readOrder = 0;
    this->writeOrder = 0;

    for (auto &curId: nodes_info) {
        if(curId == this->id) continue;

        auto channel = grpc::CreateChannel(curId, grpc::InsecureChannelCredentials());
        auto stubClientService = ClientService::NewStub(channel);
        nodesClientService.push_back(std::move(stubClientService));
    }

    ServerBuilder builder;
    builder.AddListeningPort(this->id, grpc::InsecureServerCredentials());
    builder.RegisterService(static_cast<ClientService::Service*>(this));
    server = builder.BuildAndStart();
}

Status LoadBalancer::Write(ServerContext *context, const WriteRequest *req, WriteResponse *res) {
    UNUSED(context);

    std::string longUrl = req->long_url();

    WriteRequest requestToNode;
    requestToNode.set_long_url(longUrl);

    for(int i = 0; i < (int)(nodesClientService.size()); i++) {
        WriteResponse responseFromNode;
        ClientContext clientContext;
        Status status = (nodesClientService[(unsigned long)writeOrder])->Write(&clientContext, requestToNode, &responseFromNode);

        //update node index
        writeOrder++;
        writeOrder = writeOrder % ((int)(nodesClientService.size()));

        if (status.ok()) {
            res->set_long_url(responseFromNode.long_url());
            res->set_short_url(responseFromNode.short_url());
            return Status::OK;
        } else if(status.error_code() == grpc::StatusCode::NOT_FOUND){
            // TODO fix all the status codes and implement here in a good way
            //  - there might be a error from the leader
            //      ---> in that case we should return the result to client
            //  - the node may not know the leader
            //      ---> in that case we should send write request to an another node
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Leader error!");
        }
//        else {
//            // another error code
//            // probably node connection error
//            // so send this write request to another node
//            writeOrder++;
//            writeOrder = writeOrder % ((int)(nodesClientService.size()));
//        }
    }

    return grpc::Status(grpc::StatusCode::NOT_FOUND, "URL Not Found!");
}

Status LoadBalancer::Read(ServerContext *context, const ReadRequest *req, ReadResponse *res) {
    UNUSED(context);

    std::string url = req->url();

    ReadRequest requestToNode;
    requestToNode.set_url(url);

    //TODO cpp integer division round to upper int
    int readQuorum = ((int)(nodesClientService.size())) / 2;


    for(int i = 0; i < readQuorum; i++) {
        ReadResponse responseFromNode;
        ClientContext clientContext;
        Status status = (nodesClientService[(unsigned long)readOrder])->Read(&clientContext, requestToNode, &responseFromNode);
        readOrder++;
        readOrder = readOrder % ((int)(nodesClientService.size()));
        if (status.ok()) {
            res->set_url(responseFromNode.url());
            res->set_result_url(responseFromNode.result_url());
            return Status::OK;
        }
//        else if(status.error_code() == grpc::StatusCode::NOT_FOUND){
//            // no network error between load balancer and a node
//            // but there is the node does not have this url information
//
//            // if we checked the half of the nodes and we did not find any result
//            // then we are sure that we do not commit this url
//            // and we can return an error
//
//
//        }
//        else {
//            // another error code
//            // probably node connection error
//        }


    }

    return grpc::Status(grpc::StatusCode::NOT_FOUND, "URL Not Found!");
}


void LoadBalancer::start() {
    std::cout << "Server listening on " << this->id << std::endl;
    server->Wait();
}

void LoadBalancer::stop() {
    server->Shutdown();
}

int main() {
    std::vector<std::string> nodes_info = {
            "127.0.0.1:50050", // load balancer ip:port
            "127.0.0.1:50051",
            "127.0.0.1:50052",
            "127.0.0.1:50053",
            "127.0.0.1:50054"
    };


    LoadBalancer loadBalancer(nodes_info[(unsigned long)0], nodes_info);
    loadBalancer.start();
    return 0;
}