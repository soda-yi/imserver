#ifndef _IMSERVER_HPP_
#define _IMSERVER_HPP_

#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "mysql.hpp"
#include "select.hpp"
#include "socket.hpp"

namespace herry
{
namespace imserver
{

using size_t = std::size_t;

class IMMySql : public mysql::MySql
{
public:
    IMMySql()
    {
        Connect("localhost", "dbaim", "dbaim123", "dbim", 0);
        SetCharacterSet("gbk");
    }
    bool FindUserNameBySockfd(int sockfd, std::string &username);
    bool FindSockfdByUserName(const std::string &username, int &sockfd);
    bool FindUidByUserName(const std::string &username, unsigned int &uid);
    bool FindUserNameByUid(unsigned int uid, std::string &username);
    bool FindUidBySockfd(int sockfd, unsigned int &uid);
    void SaveMessage(unsigned int rid, unsigned int sid, const std::string &message, std::time_t tt);
    void DeleteLoginfoBySockfd(int sockfd);
    std::string GetOnlineUserName(int sockfd);
    int GetOnlineSockfds(std::set<int> &fds);
};

class IMServer final
{
public:
    IMServer(const char *ip, uint16_t port);
    ~IMServer();

    void Main();

private:
    using socket_ptr = std::shared_ptr<socket::Socket>;

    void accept();
    void mainLoop();

    unsigned int verifyUserName(const socket_ptr &socket, const std::string &username);
    unsigned int verifyPassword(const socket_ptr &socket, const std::string &passwd);
    void chgLoginStatus(unsigned int uid, bool status);
    void clearClosedSock(const socket_ptr &socket);

    void packIntoMSL(const socket_ptr &socket, unsigned int head, unsigned int kind, unsigned int length, const char *data = nullptr);
    void packUnpack();
    void packReplyVerifyUserName(const socket_ptr &socket, const std::string &username);
    void packReplyVerifyPassword(const socket_ptr &socket, const std::string &password);
    void packReplyOnline(const socket_ptr &socket);
    void packRemoteLogin(const socket_ptr &socket);
    void packLoginBroadcast(const socket_ptr &socket);
    void packForward(const socket_ptr &socket, unsigned int mhdr_kind, const std::string &content);
    void packReplyForward(const socket_ptr &socket, unsigned int mhdr_kind);
    void packMessage(const socket_ptr &socket, unsigned int mhdr_kind, const std::string &message);
    void packRecvFile(const socket_ptr &socket, unsigned int mhdr_kind, const std::string &message);
    void packReplyHistory(const socket_ptr &socket, const std::string &message);

    IMMySql mMysql;
    socket_ptr mListenSocket;
    std::map<int, socket_ptr> mAllSocket;
    std::list<socket_ptr> mSendSocketList, mRecvSocketList;
};

} // namespace imserver
} // namespace herry

#endif
