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
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
std::string CryptPassword(const std::string &passwd);

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

class IMServerSocket final
{
public:
    IMServerSocket()
    {
    }
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

    template <typename T, typename... Tuple>
    bool findTuple(const char *sql_handle, T &t, Tuple &... tuple);
    template <typename T, typename... Args>
    void readItem(MYSQL_ROW item, T &t, Args &... rest);
    template <typename T>
    void readItem(MYSQL_ROW item, T &t);
    bool findUserNameBySockfd(int sockfd, std::string &username);
    bool findSockfdByUserName(const std::string &username, int &sockfd);
    bool findUidByUserName(const std::string &username, unsigned int &uid);
    bool findUserNameByUid(unsigned int uid, std::string &username);
    bool findUidBySockfd(int sockfd, unsigned int &uid);
    void databaseIDUOperation(const char *sql_handle);
    void saveMessage(unsigned int rid, unsigned int sid, const std::string &message, std::time_t tt);

    std::string getOnlineUserName(int sockfd);
    int getOnlineSockfds(fdset::FdSet &fds);

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
    MYSQL *mMysql;
    MsgQueueSet mRmqs, mSmqs;
};

} // namespace imserver

} // namespace herry

#endif
