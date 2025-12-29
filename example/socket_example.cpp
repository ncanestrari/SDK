#include "socket.hpp"
#include <fmt/core.h>
#include <thread>
#include <chrono>
#include <string_view>

using namespace sdk;
using namespace std::chrono_literals;

// Helper to convert string to byte span
std::span<const uint8_t> toBytes(std::string_view str) {
    return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

// Helper to convert byte span to string
std::string toString(std::span<uint8_t> bytes, size_t length) {
    return std::string(reinterpret_cast<char*>(bytes.data()), length);
}

void unixStreamExample() {
    fmt::print("\n=== Unix Domain Stream Socket ===\n");

    const std::string socketPath = "/tmp/sdk_test_stream.sock";

    // Server thread
    std::thread server([&]() {
        UnixStreamServerSocket serverSocket(UnixAddress{socketPath});

        if (!serverSocket.bind()) {
            fmt::print("Server: Failed to bind\n");
            return;
        }

        if (!serverSocket.listen()) {
            fmt::print("Server: Failed to listen\n");
            return;
        }

        fmt::print("Server: Listening on {}\n", socketPath);

        auto client = serverSocket.accept();
        if (!client) {
            fmt::print("Server: Failed to accept connection\n");
            return;
        }

        fmt::print("Server: Client connected\n");

        uint8_t buffer[256];
        auto received = client->receive(std::span<uint8_t>(buffer, sizeof(buffer)));
        if (received) {
            std::string message = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *received);
            fmt::print("Server: Received '{}'\n", message);

            // Echo back
            std::string response = "Echo: " + message;
            client->send(toBytes(response));
        }
    });

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Client
    UnixStreamSocket clientSocket(UnixAddress{socketPath});

    if (!clientSocket.connect()) {
        fmt::print("Client: Failed to connect\n");
        server.join();
        return;
    }

    fmt::print("Client: Connected to server\n");

    std::string message = "Hello from client!";
    clientSocket.send(toBytes(message));
    fmt::print("Client: Sent '{}'\n", message);

    uint8_t buffer[256];
    auto received = clientSocket.receive(std::span<uint8_t>(buffer, sizeof(buffer)));
    if (received) {
        std::string response = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *received);
        fmt::print("Client: Received '{}'\n", response);
    }

    server.join();
    std::remove(socketPath.c_str());
}

void unixDatagramExample() {
    fmt::print("\n=== Unix Domain Datagram Socket ===\n");

    const std::string serverPath = "/tmp/sdk_test_dgram_server.sock";
    const std::string clientPath = "/tmp/sdk_test_dgram_client.sock";

    // Server
    UnixDatagramSocket server(UnixAddress{serverPath});
    if (!server.bind()) {
        fmt::print("Server: Failed to bind\n");
        return;
    }

    fmt::print("Server: Bound to {}\n", serverPath);

    // Client
    UnixDatagramSocket client(UnixAddress{clientPath});
    if (!client.bind()) {
        fmt::print("Client: Failed to bind\n");
        return;
    }

    // Send from client to server
    std::string message = "Datagram message";
    client.sendTo(toBytes(message), UnixAddress{serverPath});
    fmt::print("Client: Sent '{}'\n", message);

    // Receive at server
    uint8_t buffer[256];
    UnixAddress senderAddr{""};
    auto received = server.receiveFrom(std::span<uint8_t>(buffer, sizeof(buffer)), senderAddr);
    if (received) {
        std::string msg = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *received);
        fmt::print("Server: Received '{}' from {}\n", msg, senderAddr.path);
    }

    std::remove(serverPath.c_str());
    std::remove(clientPath.c_str());
}

void tcpExample() {
    fmt::print("\n=== TCP Socket ===\n");

    const uint16_t port = 12345;

    // Server thread
    std::thread server([&]() {
        TcpServerSocket serverSocket(InetAddress{"0.0.0.0", port});

        if (!serverSocket.bind()) {
            fmt::print("Server: Failed to bind\n");
            return;
        }

        if (!serverSocket.listen()) {
            fmt::print("Server: Failed to listen\n");
            return;
        }

        fmt::print("Server: Listening on port {}\n", port);

        auto client = serverSocket.accept();
        if (!client) {
            fmt::print("Server: Failed to accept connection\n");
            return;
        }

        fmt::print("Server: Client connected\n");

        uint8_t buffer[256];
        auto received = client->receive(std::span<uint8_t>(buffer, sizeof(buffer)));
        if (received) {
            std::string message = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *received);
            fmt::print("Server: Received '{}'\n", message);

            // Send response
            std::string response = "TCP Echo: " + message;
            client->send(toBytes(response));
        }
    });

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Client
    TcpSocket clientSocket(InetAddress{"127.0.0.1", port});

    if (!clientSocket.connect()) {
        fmt::print("Client: Failed to connect\n");
        server.join();
        return;
    }

    fmt::print("Client: Connected to server\n");

    std::string message = "Hello TCP!";
    clientSocket.send(toBytes(message));
    fmt::print("Client: Sent '{}'\n", message);

    uint8_t buffer[256];
    auto received = clientSocket.receive(std::span<uint8_t>(buffer, sizeof(buffer)));
    if (received) {
        std::string response = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *received);
        fmt::print("Client: Received '{}'\n", response);
    }

    server.join();
}

void udpExample() {
    fmt::print("\n=== UDP Socket ===\n");

    const uint16_t serverPort = 12346;
    const uint16_t clientPort = 12347;

    // Server
    UdpSocket server(InetAddress{"0.0.0.0", serverPort});
    if (!server.bind()) {
        fmt::print("Server: Failed to bind\n");
        return;
    }

    fmt::print("Server: Bound to port {}\n", serverPort);

    // Client
    UdpSocket client(InetAddress{"0.0.0.0", clientPort});
    if (!client.bind()) {
        fmt::print("Client: Failed to bind\n");
        return;
    }

    // Send from client to server
    std::string message = "UDP packet";
    client.sendTo(toBytes(message), InetAddress{"127.0.0.1", serverPort});
    fmt::print("Client: Sent '{}'\n", message);

    // Receive at server
    uint8_t buffer[256];
    InetAddress senderAddr{"", 0};
    auto received = server.receiveFrom(std::span<uint8_t>(buffer, sizeof(buffer)), senderAddr);
    if (received) {
        std::string msg = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *received);
        fmt::print("Server: Received '{}' from {}:{}\n", msg, senderAddr.host, senderAddr.port);

        // Send response
        std::string response = "UDP ACK";
        server.sendTo(toBytes(response), senderAddr);
    }

    // Receive response at client
    auto clientReceived = client.receiveFrom(std::span<uint8_t>(buffer, sizeof(buffer)), senderAddr);
    if (clientReceived) {
        std::string msg = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *clientReceived);
        fmt::print("Client: Received '{}' from {}:{}\n", msg, senderAddr.host, senderAddr.port);
    }
}

void seqPacketExample() {
    fmt::print("\n=== Unix Sequenced Packet Socket ===\n");

    const std::string socketPath = "/tmp/sdk_test_seqpacket.sock";

    // Server thread
    std::thread server([&]() {
        UnixSeqPacketServerSocket serverSocket(UnixAddress{socketPath});

        if (!serverSocket.bind()) {
            fmt::print("Server: Failed to bind\n");
            return;
        }

        if (!serverSocket.listen()) {
            fmt::print("Server: Failed to listen\n");
            return;
        }

        fmt::print("Server: Listening on {}\n", socketPath);

        auto client = serverSocket.accept();
        if (!client) {
            fmt::print("Server: Failed to accept connection\n");
            return;
        }

        fmt::print("Server: Client connected\n");

        // Receive multiple messages with preserved boundaries
        for (int i = 0; i < 3; i++) {
            uint8_t buffer[256];
            auto received = client->receive(std::span<uint8_t>(buffer, sizeof(buffer)));
            if (received) {
                std::string message = toString(std::span<uint8_t>(buffer, sizeof(buffer)), *received);
                fmt::print("Server: Received message {}: '{}'\n", i + 1, message);
            }
        }
    });

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Client
    UnixSeqPacketSocket clientSocket(UnixAddress{socketPath});

    if (!clientSocket.connect()) {
        fmt::print("Client: Failed to connect\n");
        server.join();
        return;
    }

    fmt::print("Client: Connected to server\n");

    // Send multiple messages - boundaries will be preserved
    clientSocket.sendMessage(toBytes("First message"));
    clientSocket.sendMessage(toBytes("Second message"));
    clientSocket.sendMessage(toBytes("Third message"));
    fmt::print("Client: Sent 3 separate messages (boundaries preserved)\n");

    server.join();
    std::remove(socketPath.c_str());
}

void socketOptionsExample() {
    fmt::print("\n=== Socket Options ===\n");

    // Non-blocking socket
    TcpSocket socket(InetAddress{"127.0.0.1", 8080});

    if (socket.setNonBlocking(true)) {
        fmt::print("Socket set to non-blocking mode\n");
    }

    // Timeout
    if (socket.setTimeout(5000ms)) {
        fmt::print("Socket timeout set to 5000ms\n");
    }

    // UDP broadcast
    UdpSocket udpSocket;
    if (udpSocket.setBroadcast(true)) {
        fmt::print("UDP broadcast enabled\n");
    }

    // TCP server with address reuse
    TcpServerSocket serverSocket(InetAddress{"0.0.0.0", 9090});
    if (serverSocket.setReuseAddr(true)) {
        fmt::print("Address reuse enabled for TCP server\n");
    }
}

int main() {
    fmt::print("Socket System Examples\n");
    fmt::print("======================\n");

    unixStreamExample();
    unixDatagramExample();
    tcpExample();
    udpExample();
    seqPacketExample();
    socketOptionsExample();

    fmt::print("\n=== Summary ===\n");
    fmt::print("Demonstrated:\n");
    fmt::print("1. Unix domain stream sockets (connection-oriented)\n");
    fmt::print("2. Unix domain datagram sockets (connectionless)\n");
    fmt::print("3. TCP sockets (network stream)\n");
    fmt::print("4. UDP sockets (network datagram)\n");
    fmt::print("5. Sequenced packet sockets (message boundaries preserved)\n");
    fmt::print("6. Socket options (non-blocking, timeout, broadcast, reuse)\n");

    return 0;
}
