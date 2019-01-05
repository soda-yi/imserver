#ifndef _IMSERVER_H_
#define _IMSERVER_H_

#include <mysql.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "my_mysql.h"

namespace herry
{

namespace utility
{
class NetException : public std::runtime_error
{
public:
    NetException(const int error)
        : NetException(strerror(error))
    {
    }
    NetException(const int error, const std::string &w)
        : NetException(w + " errno: " + std::to_string(errno) + ", " + strerror(error) + ", PID: " + std::to_string(getpid()))
    {
    }

private:
    using std::runtime_error::runtime_error;
};

std::string GetSystemTime(std::time_t &tt);

class Logger
{
public:
    static void Log(const std::string &str)
    {
        std::time_t tt;
        mFout << GetSystemTime(tt) << '\t' << str << std::endl;
    }

private:
    static std::ofstream mFout;
};
} // namespace utility

namespace fdset
{
class FdSet : public fd_set
{
public:
    FdSet()
    {
        FD_ZERO(this);
    }
    void Clear(int fd)
    {
        FD_CLR(fd, this);
    }
    void Set(int fd)
    {
        FD_SET(fd, this);
    }
    void Zero()
    {
        FD_ZERO(this);
    }
    bool IsSet(int fd) const
    {
        return FD_ISSET(fd, this);
    }
    bool IsEmpty() const;
    int Count() const;
    FdSet &operator&=(const FdSet &fds2);
    FdSet &operator|=(const FdSet &fds2);
};

FdSet operator&(const FdSet &fds1, const FdSet &fds2);
FdSet operator|(const FdSet &fds1, const FdSet &fds2);
} // namespace fdset

namespace imserver
{

using size_t = std::size_t;

namespace
{
constexpr size_t MAX_SELECT = sizeof(fdset::FdSet::fds_bits) * 8;
}

using MsgType = std::vector<char>;
using MsgQueue = std::queue<std::vector<char>>;
using MsgQueueSet = std::array<MsgQueue, MAX_SELECT>;

class IMMySql : public mysql::MySql
{
public:
    IMMySql()
    {
        Connect("localhost", "u1652218", "u1652218", "db1652218", 0);
        SetCharacterSet("gbk");
    }
    bool FindUserNameBySockfd(int sockfd, std::string &username);
    bool FindSockfdByUserName(const std::string &username, int &sockfd);
    bool FindUidByUserName(const std::string &username, unsigned int &uid);
    bool FindUserNameByUid(unsigned int uid, std::string &username);
    bool FindUidBySockfd(int sockfd, unsigned int &uid);
    void SaveMessage(unsigned int rid, unsigned int sid, const std::string &message, std::time_t tt);
    std::string GetOnlineUserName(int sockfd);
    int GetOnlineSockfds(fdset::FdSet &fds);
};

class IMServerSocket final
{
public:
    IMServerSocket() = default;
    ~IMServerSocket();

    void SetServerAddr(const sockaddr_in &addr);
    void Bind();
    void Listen();
    fdset::FdSet Accept();
    fdset::FdSet Recv(fdset::FdSet &readfds, MsgQueueSet &data);
    void Send(const fdset::FdSet &writefds, MsgQueueSet &data);
    fdset::FdSet GetAllFds() const;
    int GetMaxFd() const
    {
        return mMaxfd;
    }
    void CloseSocket(int fd);

private:
    void setNonBlock(const int sockfd);
    void updateMaxfd(int fd);

    int mMaxfd = 0, mListensockfd = -1;
    bool mLsCreated = false;
    fdset::FdSet mAllfds;
    sockaddr_in mServerAddr;
};

class IMServer final
{
public:
    IMServer(const char *ip, uint16_t port);
    ~IMServer();

    void Main();

private:
    unsigned int verifyUserName(int sockfd, const std::string &username);
    unsigned int verifyPassword(int sockfd, const std::string &passwd);
    void chgLoginStatus(unsigned int uid, bool status);
    void clearClosedSock(int sockfd);

    void packIntoMSL(int sockfd, unsigned int head, unsigned int kind, unsigned int length, const char *data = nullptr);
    fdset::FdSet packUnpack(int maxfd, const fdset::FdSet &fds);
    int packReplyVerifyUserName(int sockfd, const std::string &username);
    fdset::FdSet packReplyVerifyPassword(int sockfd, const std::string &password);
    int packReplyOnline(int sockfd);
    fdset::FdSet packRemoteLogin(int sockfd);
    fdset::FdSet packLoginBroadcast(int sockfd);
    fdset::FdSet packForward(int sockfd, unsigned int mhdr_kind, const std::string &content);
    int packReplyForward(int sockfd, unsigned int mhdr_kind);
    int packMessage(int sockfd, unsigned int mhdr_kind, const std::string &message);
    fdset::FdSet packRecvFile(int sockfd, unsigned int mhdr_kind, const std::string &message);
    int packReplyHistory(int sockfd, const std::string &message);

    IMServerSocket mServerSocket;
    IMMySql mMysql;
    MsgQueueSet mRmqs, mSmqs;
};

} // namespace imserver

} // namespace herry

#endif
