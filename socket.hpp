#ifndef _SOCKET_HPP_
#define _SOCKET_HPP_

#include "network.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <queue>
#include <vector>

#include "message.hpp"

namespace herry
{
namespace socket
{

using MsgQueue = std::queue<message::Message>;

enum class AddressFamily {
    InterNetwork
};

enum class SocketType {
    Stream
};

enum class ProtocolType {
    Tcp
};

class Socket
{
public:
    explicit Socket(AddressFamily address_family, SocketType socket_type, ProtocolType protocol_type);
    Socket(const Socket &socket) = delete;
    Socket(Socket &&socket) noexcept
        : mIsAvailable(true), mSocket(socket.mSocket), mMyEndPoint(socket.mMyEndPoint),
          mPeerEndPoint(socket.mMyEndPoint), mSendMsgQue(socket.mSendMsgQue), mRecvMsgQue(socket.mRecvMsgQue) { socket.mIsAvailable = false; };
    Socket &operator=(const Socket &socket) = delete;
    Socket &operator=(Socket &&socket) = default;
    virtual ~Socket()
    {
        Close();
    }

    void Bind(const network::EndPoint &endpoint);
    void Listen();
    Socket Accept();
    void Connect(const network::EndPoint &endpoint);
    void Recv();
    void PushBackIntoSendQueue(const message::Message &msg) { mSendMsgQue.push(msg); }
    void PushBackIntoSendQueue(message::Message &&msg) { mSendMsgQue.push(std::move(msg)); }
    void PopOutOfRecvQueue() { mRecvMsgQue.pop(); }
    void Send();
    void Close()
    {
        if (mIsAvailable) {
            close(mSocket);
            mIsAvailable = false;
        }
    }

    bool IsAvailable() const { return mIsAvailable; }
    int GetSocketFd() const { return mSocket; }
    const network::EndPoint &GetPeerEndPoint() const { return mPeerEndPoint; }
    const MsgQueue &GetRecvMsgQueue() const { return mRecvMsgQue; }
    const MsgQueue &GetSendMsgQueue() const { return mSendMsgQue; }

private:
    Socket() {}
    explicit Socket(int socket, const network::EndPoint &local_endpoint, const network::EndPoint &remote_endpoint)
        : mIsAvailable(true), mSocket(socket), mMyEndPoint(local_endpoint), mPeerEndPoint(remote_endpoint) { setNonBlock(); }

    void setNonBlock();
    void recvNewPacket();
    void recvPartialPacket();
    void recvErrorProcess(int recv_size);

    bool mIsAvailable = false;
    int mSocket;
    network::EndPoint mMyEndPoint, mPeerEndPoint;
    MsgQueue mSendMsgQue, mRecvMsgQue;
};

} // namespace socket
} // namespace herry

#endif