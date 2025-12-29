#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <chrono>

namespace sdk {

// Socket result type
template<typename T>
using SocketResult = std::optional<T>;

// Socket address types
struct UnixAddress {
    std::string path;

    explicit UnixAddress(std::string_view p) : path(p) {}
};

struct InetAddress {
    std::string host;
    uint16_t port;

    InetAddress(std::string_view h, uint16_t p) : host(h), port(p) {}
};

// Base socket interface
class ISocket {
public:
    virtual ~ISocket() = default;

    // Send data through the socket
    // Returns number of bytes sent, or std::nullopt on error
    virtual SocketResult<size_t> send(std::span<const uint8_t> data) = 0;

    // Receive data from the socket
    // Returns number of bytes received, or std::nullopt on error
    virtual SocketResult<size_t> receive(std::span<uint8_t> buffer) = 0;

    // Close the socket
    virtual void close() = 0;

    // Check if socket is valid/open
    virtual bool isValid() const = 0;

    // Get the underlying file descriptor (for advanced use)
    virtual int getFd() const = 0;

    // Set socket timeout for send/receive operations
    virtual bool setTimeout(std::chrono::milliseconds timeout) = 0;

    // Set socket to blocking or non-blocking mode
    virtual bool setNonBlocking(bool nonBlocking) = 0;
};

// Connection-oriented socket interface (for sockets that connect to a peer)
class IConnectedSocket {
public:
    virtual ~IConnectedSocket() = default;

    // Connect to a remote endpoint
    virtual bool connect() = 0;

    // Check if socket is connected
    virtual bool isConnected() const = 0;
};

// Server socket interface (for sockets that accept connections)
class IServerSocket {
public:
    virtual ~IServerSocket() = default;

    // Bind the socket to an address
    virtual bool bind() = 0;

    // Listen for incoming connections
    virtual bool listen(int backlog = 5) = 0;

    // Accept an incoming connection
    // Returns a new socket for the accepted connection
    virtual std::unique_ptr<ISocket> accept() = 0;
};

// Connectionless socket interface (for datagram sockets)
class IDatagramSocket {
public:
    virtual ~IDatagramSocket() = default;

    // Send data to a specific address
    virtual SocketResult<size_t> sendTo(std::span<const uint8_t> data, const void* addr, socklen_t addrLen) = 0;

    // Receive data and get sender's address
    virtual SocketResult<size_t> receiveFrom(std::span<uint8_t> buffer, void* addr, socklen_t* addrLen) = 0;

    // Bind to a local address
    virtual bool bind() = 0;
};

// Base implementation with common socket operations
class BaseSocket : public ISocket {
protected:
    int fd_ = -1;
    bool connected_ = false;

public:
    BaseSocket() = default;
    explicit BaseSocket(int fd) : fd_(fd), connected_(fd >= 0) {}
    virtual ~BaseSocket() override;

    // Disable copy, allow move
    BaseSocket(const BaseSocket&) = delete;
    BaseSocket& operator=(const BaseSocket&) = delete;
    BaseSocket(BaseSocket&& other) noexcept;
    BaseSocket& operator=(BaseSocket&& other) noexcept;

    SocketResult<size_t> send(std::span<const uint8_t> data) override;
    SocketResult<size_t> receive(std::span<uint8_t> buffer) override;
    void close() override;
    bool isValid() const override { return fd_ >= 0; }
    int getFd() const override { return fd_; }
    bool setTimeout(std::chrono::milliseconds timeout) override;
    bool setNonBlocking(bool nonBlocking) override;

protected:
    bool createSocket(int domain, int type, int protocol);

    // Allow derived server sockets to set fd and connected state for accepted sockets
    friend class UnixStreamServerSocket;
    friend class TcpServerSocket;
    friend class UnixSeqPacketServerSocket;
};

// Unix Domain Stream Socket (SOCK_STREAM)
class __attribute__((annotate("initialize"))) UnixStreamSocket : public BaseSocket, public IConnectedSocket {
private:
    UnixAddress address_;

public:
    explicit UnixStreamSocket(UnixAddress addr);
    ~UnixStreamSocket() override = default;

    bool connect() override;
    bool isConnected() const override { return connected_; }
};

// Unix Domain Stream Server Socket
class __attribute__((annotate("initialize"))) UnixStreamServerSocket : public BaseSocket, public IServerSocket {
private:
    UnixAddress address_;

public:
    explicit UnixStreamServerSocket(UnixAddress addr);
    ~UnixStreamServerSocket() override = default;

    bool bind() override;
    bool listen(int backlog = 5) override;
    std::unique_ptr<ISocket> accept() override;
};

// Unix Domain Datagram Socket (SOCK_DGRAM)
class __attribute__((annotate("initialize"))) UnixDatagramSocket : public BaseSocket, public IDatagramSocket {
private:
    std::optional<UnixAddress> address_;

public:
    UnixDatagramSocket();
    explicit UnixDatagramSocket(UnixAddress addr);
    ~UnixDatagramSocket() override = default;

    SocketResult<size_t> sendTo(std::span<const uint8_t> data, const void* addr, socklen_t addrLen) override;
    SocketResult<size_t> receiveFrom(std::span<uint8_t> buffer, void* addr, socklen_t* addrLen) override;
    bool bind() override;

    // Helper methods for Unix addresses
    SocketResult<size_t> sendTo(std::span<const uint8_t> data, const UnixAddress& addr);
    SocketResult<size_t> receiveFrom(std::span<uint8_t> buffer, UnixAddress& addr);
};

// TCP Socket (SOCK_STREAM over AF_INET/AF_INET6)
class __attribute__((annotate("initialize"))) TcpSocket : public BaseSocket, public IConnectedSocket {
private:
    InetAddress address_;
    bool useIPv6_;

public:
    explicit TcpSocket(InetAddress addr, bool useIPv6 = false);
    ~TcpSocket() override = default;

    bool connect() override;
    bool isConnected() const override { return connected_; }
};

// TCP Server Socket
class __attribute__((annotate("initialize"))) TcpServerSocket : public BaseSocket, public IServerSocket {
private:
    InetAddress address_;
    bool useIPv6_;

public:
    explicit TcpServerSocket(InetAddress addr, bool useIPv6 = false);
    ~TcpServerSocket() override = default;

    bool bind() override;
    bool listen(int backlog = 5) override;
    std::unique_ptr<ISocket> accept() override;

    // Enable/disable address reuse
    bool setReuseAddr(bool reuse);
};

// UDP Socket (SOCK_DGRAM over AF_INET/AF_INET6)
class __attribute__((annotate("initialize"))) UdpSocket : public BaseSocket, public IDatagramSocket {
private:
    std::optional<InetAddress> address_;
    bool useIPv6_;

public:
    explicit UdpSocket(bool useIPv6 = false);
    explicit UdpSocket(InetAddress addr, bool useIPv6 = false);
    ~UdpSocket() override = default;

    SocketResult<size_t> sendTo(std::span<const uint8_t> data, const void* addr, socklen_t addrLen) override;
    SocketResult<size_t> receiveFrom(std::span<uint8_t> buffer, void* addr, socklen_t* addrLen) override;
    bool bind() override;

    // Helper methods for Internet addresses
    SocketResult<size_t> sendTo(std::span<const uint8_t> data, const InetAddress& addr);
    SocketResult<size_t> receiveFrom(std::span<uint8_t> buffer, InetAddress& addr);

    // Enable/disable broadcast
    bool setBroadcast(bool broadcast);
};

// Raw Socket (SOCK_RAW)
class __attribute__((annotate("initialize"))) RawSocket : public BaseSocket {
private:
    int protocol_;
    bool useIPv6_;

public:
    explicit RawSocket(int protocol, bool useIPv6 = false);
    ~RawSocket() override = default;

    // Raw sockets typically use sendto/recvfrom
    SocketResult<size_t> sendTo(std::span<const uint8_t> data, const void* addr, socklen_t addrLen);
    SocketResult<size_t> receiveFrom(std::span<uint8_t> buffer, void* addr, socklen_t* addrLen);

    // Set IP header included option (for IPv4)
    bool setIpHeaderInclude(bool include);
};

// Sequenced Packet Socket (SOCK_SEQPACKET)
// Connection-oriented with message boundaries preserved
class __attribute__((annotate("initialize"))) UnixSeqPacketSocket : public BaseSocket, public IConnectedSocket {
private:
    UnixAddress address_;

public:
    explicit UnixSeqPacketSocket(UnixAddress addr);
    ~UnixSeqPacketSocket() override = default;

    bool connect() override;
    bool isConnected() const override { return connected_; }

    // Send/receive preserve message boundaries
    SocketResult<size_t> sendMessage(std::span<const uint8_t> data);
    SocketResult<size_t> receiveMessage(std::span<uint8_t> buffer);
};

// Sequenced Packet Server Socket
class __attribute__((annotate("initialize"))) UnixSeqPacketServerSocket : public BaseSocket, public IServerSocket {
private:
    UnixAddress address_;

public:
    explicit UnixSeqPacketServerSocket(UnixAddress addr);
    ~UnixSeqPacketServerSocket() override = default;

    bool bind() override;
    bool listen(int backlog = 5) override;
    std::unique_ptr<ISocket> accept() override;
};

} // namespace sdk
