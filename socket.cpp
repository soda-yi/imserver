#include "socket.hpp"

#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/select.h>

#include <cstring>

#include "select.hpp"
#include "utility.hpp"

using herry::message::MessageHeader;
using herry::utility::NetException;
using herry::utility::NetExceptionKind;

namespace herry
{
namespace socket
{

Socket::Socket(AddressFamily address_family, SocketType socket_type, ProtocolType protocol_type)
    : mIsAvailable(true)
{
    int af = 0, st = 0, pt = 0;

    switch (address_family) {
    case AddressFamily::InterNetwork:
        af = AF_INET;
        break;
    }

    switch (socket_type) {
    case SocketType::Stream:
        st = SOCK_STREAM;
        break;
    }

    switch (protocol_type) {
    case ProtocolType::Tcp:
        pt = IPPROTO_TCP;
        break;
    default:
        pt = IPPROTO_IP;
        break;
    }

    mSocket = ::socket(af, st, pt);
    setNonBlock();
    int on = 1;
    setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
}

void Socket::setNonBlock()
{
    int flags = fcntl(mSocket, F_GETFL, 0);
    if (flags < 0) {
        throw NetException(NetExceptionKind::UNKNOW_EXCEPTION);
    }

    if (fcntl(mSocket, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw NetException(NetExceptionKind::UNKNOW_EXCEPTION);
    }
}

void Socket::Bind(const network::EndPoint &endpoint)
{
    sockaddr_in sock_addr;

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(endpoint.IPAddr.c_str());
    sock_addr.sin_port = htons(endpoint.Port);
    bzero(&(sock_addr.sin_zero), 8);

    if (bind(mSocket, reinterpret_cast<sockaddr *>(&sock_addr), sizeof(sockaddr)) == -1) {
        throw NetException(NetExceptionKind::BIND_ERROR);
    }

    mMyEndPoint = endpoint;
}

void Socket::Listen()
{
    constexpr int backlog = 10;
    if (listen(mSocket, backlog) == -1) {
        throw NetException(NetExceptionKind::LISTEN_ERROR);
    }
}

Socket Socket::Accept()
{
    Socket ret;

    int sockfd_n;
    socklen_t sin_len = sizeof(sockaddr_in);
    sockaddr_in client_addr;
    if ((sockfd_n = accept(mSocket, reinterpret_cast<sockaddr *>(&client_addr), &sin_len)) == -1) {
        if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
            throw NetException(NetExceptionKind::ACCEPT_AGAIN);
        } else {
            throw NetException(NetExceptionKind::ACCEPT_ERROR);
        }
    } else {
        return Socket(sockfd_n, mMyEndPoint, {inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)});
    }

    return ret;
}

void Socket::Recv()
{
    if (mRecvMsgQue.empty()) {
        recvNewPacket();
    }
    for (;;) {
        auto &msg = mRecvMsgQue.back();
        if (msg.GetLength() < sizeof(MessageHeader) || msg.GetLength() != msg.GetMessageLength()) {
            recvPartialPacket();
        } else {
            recvNewPacket();
        }
    }
}

void Socket::recvErrorProcess(int r_sz)
{
    if (r_sz > 0) {
        return;
    } else if (r_sz < 0) {
        if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
            throw NetException(NetExceptionKind::RECV_AGAIN);
        } else {
            Close();
            throw NetException(NetExceptionKind::RECV_ERROR);
        }
    } else {
        Close();
        throw NetException(NetExceptionKind::RECV_SHUTDOWN);
    }
}

void Socket::recvNewPacket()
{
    /* message queue is empty OR last receive packet is complete */
    message::Message t_msg;
    /* recv message hdr */
    unsigned int buf_sz = sizeof(MessageHeader);
    t_msg.SetSize(buf_sz);
    unsigned int r_sz = recv(mSocket, t_msg.GetData(), buf_sz, 0);
    recvErrorProcess(r_sz);
    t_msg.SetLength(r_sz);
    /* if hdr recv completely, recv message text */
    if (r_sz == buf_sz) {
        buf_sz = t_msg.GetMessageLength();
        if (r_sz < buf_sz) {
            t_msg.SetSize(buf_sz);
            r_sz = recv(mSocket, t_msg.GetData() + sizeof(MessageHeader), buf_sz - sizeof(MessageHeader), 0);
            recvErrorProcess(r_sz);
            t_msg.SetLength(sizeof(MessageHeader) + r_sz);
        } // else if (r_sz == buf_sz) {}
    }
    /* no matter packet is complete or not, push to message queue */
    mRecvMsgQue.push(std::move(t_msg));
}

void Socket::recvPartialPacket()
{
    /* last receive packet is not complete */
    auto &msg = mRecvMsgQue.back();
    unsigned int buf_sz = sizeof(MessageHeader), origin_sz, r_sz;
    /* if last packet's hdr doesn't recv completely, continue to recv hdr */
    origin_sz = msg.GetLength();
    if (origin_sz < sizeof(MessageHeader)) {
        msg.SetSize(buf_sz);
        r_sz = recv(mSocket, msg.GetData() + origin_sz, buf_sz - origin_sz, 0);
        recvErrorProcess(r_sz);
        msg.SetLength(r_sz + origin_sz);
    }
    /* when hdr recv completely, recv message text */
    origin_sz = msg.GetLength();
    if (origin_sz >= sizeof(MessageHeader)) {
        buf_sz = msg.GetMessageLength();
        if (origin_sz < buf_sz) {
            msg.SetSize(buf_sz);
            r_sz = recv(mSocket, msg.GetData() + origin_sz, buf_sz - origin_sz, 0);
            recvErrorProcess(r_sz);
            msg.SetLength(r_sz + origin_sz);
        }
    }
}

void Socket::Send()
{
    if (mIsAvailable) {
        int s_size;
        while (!mSendMsgQue.empty()) {
            auto &msg = mSendMsgQue.front();
            auto temp = msg.GetData();
            //print_message(temp);
            if ((s_size = send(mSocket, temp, msg.GetLength(), 0)) <= 0) {
                if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                    throw NetException(NetExceptionKind::SEND_AGAIN);
                } else {
                    throw NetException(NetExceptionKind::SEND_ERROR);
                }
            }
            if (static_cast<size_t>(s_size) != msg.GetLength()) {
                auto rest_sz = msg.GetLength() - s_size;
                memmove(temp, temp + s_size, rest_sz);
                msg.SetLength(rest_sz);
                break;
            } else {
                mSendMsgQue.pop();
            }
        }
    }
}

} // namespace socket
} // namespace herry