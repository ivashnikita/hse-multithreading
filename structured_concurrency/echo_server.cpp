#include <cstdint>
#include <thread>
#include <string_view>

class Socket {
public:
    Socket(std::string_view address, uint16_t port);

    void Send(std::string_view payload);

    std::string Receive();

    Socket Accept();
};

class ServerContext {};

bool ValidateClientData() {
    //...
}

void ServeClient(ServerContext& ctx, Socket&& clientSocket) {
    std::string data = clientSocket.Receive();

    if (!ValidateClientData()) {
        throw std::runtime_error("Invalid client data");
    }

    //...
}

int main() {
    Socket server("localhost", 8080);
    ServerContext ctx;

    while (true) {
        Socket clientConnection = server.Accept();

        std::thread connectionProcessor(ServeClient, std::ref(ctx), std::move(clientConnection));
    }

    return 0;
}
