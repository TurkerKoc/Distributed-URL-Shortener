// read, write methodunu grpc yazicaz
// mainde RaftNode objelerini olusturup thread olarak bunlari calistiricaz
// sonra grpc dinlemeye baslicak ve ne zaman bir istek gelse sirasiyla nodelara yonlendiricez
// write request RafNodeda hali hazirda leadera forward olacagi icin lider bilgisini buranin tutmasina gerek yok
// eger herhangi bir istekte hata alirsak diger nodea istek aticaz



#include <raft.grpc.pb.h>
#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using raft::WriteRequest;
using raft::WriteResponse;
using raft::ReadRequest;
using raft::ReadResponse;
using raft::ClientService;

class Client {
public:
    Client(std::shared_ptr<Channel> channel)
            : stub_(ClientService::NewStub(channel)) {}

    // send message function
    void Write(const std::string& longUrl) {
        WriteRequest request;
        request.set_long_url(longUrl);

        WriteResponse response;
        ClientContext context;
        Status status = stub_->Write(&context, request, &response);

        if (status.ok()) {
            std::cout << "Long URL: " << response.long_url() << std::endl;
            std::cout << "Short URL: " << response.short_url() << std::endl;
        } else {
            std::cout << status.error_message() << std::endl;
            return; //rpc failed
        }
    }
    // send message function
    void Read(const std::string& url) {
        ReadRequest request;
        request.set_url(url);

        ReadResponse response;
        ClientContext context;
        Status status = stub_->Read(&context, request, &response);

        if (status.ok()) {
            std::cout << "URL: " << response.url() << std::endl;
            std::cout << "Result URL: " << response.result_url() << std::endl;
        } else {
            std::cout << status.error_message() << std::endl;
            return; //rpc failed
        }
    }

private:
    std::unique_ptr<ClientService::Stub> stub_;
};

int main(int argc, char** argv) {
    // parse command-line arguments using gflags or boost program_options
    std::string operation;
    std::string data;
//    gflags::ParseCommandLineFlags(&argc, &argv, true);
//    gflags::SetUsageMessage("CLI for your gRPC service.");
//    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
//    gflags::HandleCommandLineHelpFlags();
    if (argc < 3) {
//        gflags::ShowUsageWithFlagsRestrict(argv[0], "ClientService");
        return 1;
    }
    operation = argv[1];
    data = argv[2];

    // create a client and call the service
    Client client(grpc::CreateChannel("localhost:50053", grpc::InsecureChannelCredentials()));

    if(operation == "write") {
        client.Write(data);
    }
    else if(operation == "read") {
        client.Read(data);
    }


    return 0;
}