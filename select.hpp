#ifndef _SELECT_HPP_
#define _SELECT_HPP_

#include <sys/select.h>

#include <memory>
#include <set>

#include "socket.hpp"

namespace herry
{
namespace select
{

class FdSet : public fd_set
{
public:
    FdSet()
    {
        FD_ZERO(this);
    }
    void Clear(const socket::Socket &socket)
    {
        Clear(socket.GetSocketFd());
    }
    void Set(const socket::Socket &socket)
    {
        Set(socket.GetSocketFd());
    }
    bool IsSet(const socket::Socket &socket)
    {
        return IsSet(socket.GetSocketFd());
    }
    void Zero()
    {
        FD_ZERO(this);
    }
    bool IsEmpty() const;
    int Count() const;
    FdSet &operator&=(const FdSet &fds2);
    FdSet &operator|=(const FdSet &fds2);

private:
    void Clear(int fd)
    {
        FD_CLR(fd, this);
    }
    void Set(int fd)
    {
        FD_SET(fd, this);
    }
    bool IsSet(int fd) const
    {
        return FD_ISSET(fd, this);
    }
};

FdSet operator&(const FdSet &fds1, const FdSet &fds2);
FdSet operator|(const FdSet &fds1, const FdSet &fds2);

class Select
{
public:
    Select(FdSet *readfds, FdSet *writefds)
        : mReadFds(readfds), mWriteFds(writefds) {}
    int operator()(timeval *timeout);
    void SetReadFds(const FdSet &fds);
    void AddReadFds(const socket::Socket &socket);
    void SetWriteFds(const FdSet &fds);
    void AddWriteFds(const socket::Socket &socket);

private:
    FdSet *mReadFds = nullptr, *mWriteFds = nullptr;
    int mMaxFd = FD_SETSIZE;
};

} // namespace select
} // namespace herry

#endif