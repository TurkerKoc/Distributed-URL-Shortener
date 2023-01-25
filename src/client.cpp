#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include <raft.grpc.pb.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using raft::ClientService;
using raft::ReadRequest;
using raft::ReadResponse;
using raft::WriteRequest;
using raft::WriteResponse;

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
   std::string operation; //read/write
   std::string data; //url/short_url
   if (argc < 3) {
      return 1;
   }
   operation = argv[1];
   data = argv[2];

   // create a client and call the service
   //Client client(grpc::CreateChannel("localhost:50050", grpc::InsecureChannelCredentials())); //connect to load balancer
   Client client(grpc::CreateChannel("172.32.0.20:50050", grpc::InsecureChannelCredentials())); //connect to load balancer


   if (operation == "write") {
      client.Write(data);
   } else if (operation == "read") {
      client.Read(data);
   }

   return 0;
}