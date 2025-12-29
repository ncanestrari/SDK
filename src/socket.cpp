#include "socket.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <errno.h>

namespace sdk {

// BaseSocket implementation
BaseSocket::~BaseSocket() {
    close();
}

BaseSocket::BaseSocket(BaseSocket&& other) noexcept
    : fd_(other.fd_), connected_(other.connected_) {
    other.fd_ = -1;
    other.connected_ = false;
}

BaseSocket& BaseSocket::operator=(BaseSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        connected_ = other.connected_;
        other.fd_ = -1;
        other.connected_ = false;
    }
    return *this;
}

bool BaseSocket::createSocket(int domain, int type, int protocol) {
    fd_ = ::socket(domain, type, protocol);
    return fd_ >= 0;
}

SocketResult<size_t> BaseSocket::send(std::span<const uint8_t> data) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::send(fd_, data.data(), data.size(), 0);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

SocketResult<size_t> BaseSocket::receive(std::span<uint8_t> buffer) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::recv(fd_, buffer.data(), buffer.size(), 0);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

void BaseSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        connected_ = false;
    }
}

bool BaseSocket::setTimeout(std::chrono::milliseconds timeout) {
    if (!isValid()) {
        return false;
    }

    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;

    bool sendOk = setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
    bool recvOk = setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;

    return sendOk && recvOk;
}

bool BaseSocket::setNonBlocking(bool nonBlocking) {
    if (!isValid()) {
        return false;
    }

    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }

    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    return fcntl(fd_, F_SETFL, flags) == 0;
}

// UnixStreamSocket implementation
UnixStreamSocket::UnixStreamSocket(UnixAddress addr)
    : address_(std::move(addr)) {
    createSocket(AF_UNIX, SOCK_STREAM, 0);
}

bool UnixStreamSocket::connect() {
    if (!BaseSocket::isValid() || connected_) {
        return false;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address_.path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        return false;
    }

    connected_ = true;
    return true;
}

// UnixStreamServerSocket implementation
UnixStreamServerSocket::UnixStreamServerSocket(UnixAddress addr)
    : address_(std::move(addr)) {
    createSocket(AF_UNIX, SOCK_STREAM, 0);
}

bool UnixStreamServerSocket::bind() {
    if (!BaseSocket::isValid()) {
        return false;
    }

    // Remove existing socket file if it exists
    ::unlink(address_.path.c_str());

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address_.path.c_str(), sizeof(addr.sun_path) - 1);

    return ::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
}

bool UnixStreamServerSocket::listen(int backlog) {
    if (!BaseSocket::isValid()) {
        return false;
    }

    return ::listen(fd_, backlog) == 0;
}

std::unique_ptr<ISocket> UnixStreamServerSocket::accept() {
    if (!BaseSocket::isValid()) {
        return nullptr;
    }

    struct sockaddr_un clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientFd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
    if (clientFd < 0) {
        return nullptr;
    }

    // Create a socket wrapper for the accepted connection
    auto socket = std::make_unique<UnixStreamSocket>(UnixAddress{""});
    static_cast<BaseSocket*>(socket.get())->fd_ = clientFd;
    static_cast<BaseSocket*>(socket.get())->connected_ = true;

    return socket;
}

// UnixDatagramSocket implementation
UnixDatagramSocket::UnixDatagramSocket() {
    createSocket(AF_UNIX, SOCK_DGRAM, 0);
}

UnixDatagramSocket::UnixDatagramSocket(UnixAddress addr)
    : address_(std::move(addr)) {
    createSocket(AF_UNIX, SOCK_DGRAM, 0);
}

SocketResult<size_t> UnixDatagramSocket::sendTo(std::span<const uint8_t> data, const void* addr, socklen_t addrLen) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::sendto(fd_, data.data(), data.size(), 0,
                              static_cast<const struct sockaddr*>(addr), addrLen);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

SocketResult<size_t> UnixDatagramSocket::receiveFrom(std::span<uint8_t> buffer, void* addr, socklen_t* addrLen) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::recvfrom(fd_, buffer.data(), buffer.size(), 0,
                                static_cast<struct sockaddr*>(addr), addrLen);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

bool UnixDatagramSocket::bind() {
    if (!BaseSocket::isValid() || !address_) {
        return false;
    }

    // Remove existing socket file if it exists
    ::unlink(address_->path.c_str());

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address_->path.c_str(), sizeof(addr.sun_path) - 1);

    return ::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
}

SocketResult<size_t> UnixDatagramSocket::sendTo(std::span<const uint8_t> data, const UnixAddress& addr) {
    struct sockaddr_un destAddr;
    std::memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sun_family = AF_UNIX;
    std::strncpy(destAddr.sun_path, addr.path.c_str(), sizeof(destAddr.sun_path) - 1);

    return sendTo(data, &destAddr, sizeof(destAddr));
}

SocketResult<size_t> UnixDatagramSocket::receiveFrom(std::span<uint8_t> buffer, UnixAddress& addr) {
    struct sockaddr_un srcAddr;
    socklen_t addrLen = sizeof(srcAddr);

    auto result = receiveFrom(buffer, &srcAddr, &addrLen);
    if (result) {
        addr.path = srcAddr.sun_path;
    }

    return result;
}

// TcpSocket implementation
TcpSocket::TcpSocket(InetAddress addr, bool useIPv6)
    : address_(std::move(addr)), useIPv6_(useIPv6) {
    createSocket(useIPv6_ ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
}

bool TcpSocket::connect() {
    if (!BaseSocket::isValid() || connected_) {
        return false;
    }

    struct addrinfo hints, *result, *rp;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = useIPv6_ ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(address_.port);
    if (getaddrinfo(address_.host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return false;
    }

    bool success = false;
    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        if (::connect(fd_, rp->ai_addr, rp->ai_addrlen) == 0) {
            success = true;
            break;
        }
    }

    freeaddrinfo(result);

    if (success) {
        connected_ = true;
    }

    return success;
}

// TcpServerSocket implementation
TcpServerSocket::TcpServerSocket(InetAddress addr, bool useIPv6)
    : address_(std::move(addr)), useIPv6_(useIPv6) {
    createSocket(useIPv6_ ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
    setReuseAddr(true);
}

bool TcpServerSocket::bind() {
    if (!BaseSocket::isValid()) {
        return false;
    }

    if (useIPv6_) {
        struct sockaddr_in6 addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(address_.port);
        addr.sin6_addr = in6addr_any;

        return ::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
    } else {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address_.port);
        addr.sin_addr.s_addr = INADDR_ANY;

        return ::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
    }
}

bool TcpServerSocket::listen(int backlog) {
    if (!BaseSocket::isValid()) {
        return false;
    }

    return ::listen(fd_, backlog) == 0;
}

std::unique_ptr<ISocket> TcpServerSocket::accept() {
    if (!BaseSocket::isValid()) {
        return nullptr;
    }

    if (useIPv6_) {
        struct sockaddr_in6 clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            return nullptr;
        }

        auto socket = std::make_unique<TcpSocket>(InetAddress{"", 0}, true);
        socket->fd_ = clientFd;
        socket->connected_ = true;

        return socket;
    } else {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            return nullptr;
        }

        auto socket = std::make_unique<TcpSocket>(InetAddress{"", 0}, false);
        socket->fd_ = clientFd;
        socket->connected_ = true;

        return socket;
    }
}

bool TcpServerSocket::setReuseAddr(bool reuse) {
    if (!BaseSocket::isValid()) {
        return false;
    }

    int opt = reuse ? 1 : 0;
    return setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

// UdpSocket implementation
UdpSocket::UdpSocket(bool useIPv6)
    : useIPv6_(useIPv6) {
    createSocket(useIPv6_ ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
}

UdpSocket::UdpSocket(InetAddress addr, bool useIPv6)
    : address_(std::move(addr)), useIPv6_(useIPv6) {
    createSocket(useIPv6_ ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
}

SocketResult<size_t> UdpSocket::sendTo(std::span<const uint8_t> data, const void* addr, socklen_t addrLen) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::sendto(fd_, data.data(), data.size(), 0,
                              static_cast<const struct sockaddr*>(addr), addrLen);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

SocketResult<size_t> UdpSocket::receiveFrom(std::span<uint8_t> buffer, void* addr, socklen_t* addrLen) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::recvfrom(fd_, buffer.data(), buffer.size(), 0,
                                static_cast<struct sockaddr*>(addr), addrLen);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

bool UdpSocket::bind() {
    if (!BaseSocket::isValid() || !address_) {
        return false;
    }

    if (useIPv6_) {
        struct sockaddr_in6 addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(address_->port);
        addr.sin6_addr = in6addr_any;

        return ::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
    } else {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address_->port);
        addr.sin_addr.s_addr = INADDR_ANY;

        return ::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
    }
}

SocketResult<size_t> UdpSocket::sendTo(std::span<const uint8_t> data, const InetAddress& addr) {
    struct addrinfo hints, *result;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = useIPv6_ ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    std::string portStr = std::to_string(addr.port);
    if (getaddrinfo(addr.host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return std::nullopt;
    }

    auto sendResult = sendTo(data, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);

    return sendResult;
}

SocketResult<size_t> UdpSocket::receiveFrom(std::span<uint8_t> buffer, InetAddress& addr) {
    if (useIPv6_) {
        struct sockaddr_in6 srcAddr;
        socklen_t addrLen = sizeof(srcAddr);

        auto result = receiveFrom(buffer, &srcAddr, &addrLen);
        if (result) {
            char hostStr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &srcAddr.sin6_addr, hostStr, sizeof(hostStr));
            addr.host = hostStr;
            addr.port = ntohs(srcAddr.sin6_port);
        }

        return result;
    } else {
        struct sockaddr_in srcAddr;
        socklen_t addrLen = sizeof(srcAddr);

        auto result = receiveFrom(buffer, &srcAddr, &addrLen);
        if (result) {
            char hostStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &srcAddr.sin_addr, hostStr, sizeof(hostStr));
            addr.host = hostStr;
            addr.port = ntohs(srcAddr.sin_port);
        }

        return result;
    }
}

bool UdpSocket::setBroadcast(bool broadcast) {
    if (!BaseSocket::isValid()) {
        return false;
    }

    int opt = broadcast ? 1 : 0;
    return setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) == 0;
}

// RawSocket implementation
RawSocket::RawSocket(int protocol, bool useIPv6)
    : protocol_(protocol), useIPv6_(useIPv6) {
    createSocket(useIPv6_ ? AF_INET6 : AF_INET, SOCK_RAW, protocol);
}

SocketResult<size_t> RawSocket::sendTo(std::span<const uint8_t> data, const void* addr, socklen_t addrLen) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::sendto(fd_, data.data(), data.size(), 0,
                              static_cast<const struct sockaddr*>(addr), addrLen);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

SocketResult<size_t> RawSocket::receiveFrom(std::span<uint8_t> buffer, void* addr, socklen_t* addrLen) {
    if (!isValid()) {
        return std::nullopt;
    }

    ssize_t result = ::recvfrom(fd_, buffer.data(), buffer.size(), 0,
                                static_cast<struct sockaddr*>(addr), addrLen);
    if (result < 0) {
        return std::nullopt;
    }

    return static_cast<size_t>(result);
}

bool RawSocket::setIpHeaderInclude(bool include) {
    if (!BaseSocket::isValid() || useIPv6_) {
        return false;
    }

    int opt = include ? 1 : 0;
    return setsockopt(fd_, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) == 0;
}

// UnixSeqPacketSocket implementation
UnixSeqPacketSocket::UnixSeqPacketSocket(UnixAddress addr)
    : address_(std::move(addr)) {
    createSocket(AF_UNIX, SOCK_SEQPACKET, 0);
}

bool UnixSeqPacketSocket::connect() {
    if (!BaseSocket::isValid() || connected_) {
        return false;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address_.path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        return false;
    }

    connected_ = true;
    return true;
}

SocketResult<size_t> UnixSeqPacketSocket::sendMessage(std::span<const uint8_t> data) {
    return send(data);
}

SocketResult<size_t> UnixSeqPacketSocket::receiveMessage(std::span<uint8_t> buffer) {
    return receive(buffer);
}

// UnixSeqPacketServerSocket implementation
UnixSeqPacketServerSocket::UnixSeqPacketServerSocket(UnixAddress addr)
    : address_(std::move(addr)) {
    createSocket(AF_UNIX, SOCK_SEQPACKET, 0);
}

bool UnixSeqPacketServerSocket::bind() {
    if (!BaseSocket::isValid()) {
        return false;
    }

    // Remove existing socket file if it exists
    ::unlink(address_.path.c_str());

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address_.path.c_str(), sizeof(addr.sun_path) - 1);

    return ::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
}

bool UnixSeqPacketServerSocket::listen(int backlog) {
    if (!BaseSocket::isValid()) {
        return false;
    }

    return ::listen(fd_, backlog) == 0;
}

std::unique_ptr<ISocket> UnixSeqPacketServerSocket::accept() {
    if (!BaseSocket::isValid()) {
        return nullptr;
    }

    struct sockaddr_un clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientFd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
    if (clientFd < 0) {
        return nullptr;
    }

    auto socket = std::make_unique<UnixSeqPacketSocket>(UnixAddress{""});
    socket->fd_ = clientFd;
    socket->connected_ = true;

    return socket;
}

} // namespace sdk
