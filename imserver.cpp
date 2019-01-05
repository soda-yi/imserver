#include "imserver.h"

#include <arpa/inet.h>
#include <fcntl.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <stack>
#include <tuple>

#include "message_header.h"

#include <iomanip>
using namespace std;
using std::cout;
using std::endl;

using herry::fdset::FdSet;
using herry::message::MessageHeader;
using namespace herry::utility;

using std::istringstream;
using std::ofstream;
using std::ostringstream;
using std::size_t;
using std::string;
using std::time_t;
using std::vector;

namespace herry
{
namespace utility
{
ofstream Logger::mFout("./IMServer.log", ofstream::app);

string GetSystemTime(time_t &tt)
{
    tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return date;
}

void print_message(char *buf)
{
    auto msghdr = reinterpret_cast<MessageHeader *>(buf);
    auto s_size = msghdr->length, m_size = 4U;
    //cout << "size = " << s_size << endl;
    cout << hex << "0x" << msghdr->head << " 0x" << setw(2) << setfill('0') << msghdr->kind << " " << dec << msghdr->length << endl;
    for (auto i = m_size; i != s_size; ++i) {
        cout << buf[i];
    }
    cout << endl;
}
} // namespace utility

namespace fdset
{

bool FdSet::IsEmpty() const
{
    for (auto n : fds_bits) {
        if (n) {
            return false;
        }
    }
    return true;
}

int FdSet::Count() const
{
    int count = 0;
    for (unsigned long n : fds_bits) {
        while (n) {
            count += n & 1;
            n >>= 1;
        }
    }
    return count;
}

FdSet &FdSet::operator&=(const FdSet &fds2)
{
    for (int i = 0; i != sizeof(fds_bits) / sizeof(*fds_bits); ++i) {
        fds_bits[i] &= fds2.fds_bits[i];
    }
    return *this;
}

FdSet &FdSet::operator|=(const FdSet &fds2)
{
    for (int i = 0; i != sizeof(fds_bits) / sizeof(*fds_bits); ++i) {
        fds_bits[i] |= fds2.fds_bits[i];
    }
    return *this;
}

bool operator==(const FdSet &fds1, const FdSet &fds2)
{
    for (int i = 0; i != sizeof(fds1.fds_bits) / sizeof(*fds1.fds_bits); ++i) {
        if (fds1.fds_bits[i] != fds2.fds_bits[i]) {
            return false;
        }
    }
    return true;
}

FdSet operator&(const FdSet &fds1, const FdSet &fds2)
{
    FdSet ret = fds1;
    ret &= fds2;
    return ret;
}

FdSet operator|(const FdSet &fds1, const FdSet &fds2)
{
    FdSet ret = fds1;
    ret |= fds2;
    return ret;
}

} // namespace fdset

namespace imserver
{

bool IMMySql::FindUserNameBySockfd(int sockfd, string &username)
{
    ostringstream sql_handle;
    sql_handle << "select u.name from user u inner join loginfo l on u.id=l.uid where l.sockfd=" << sockfd;
    return FindTuple(sql_handle.str().c_str(), username);
}

bool IMMySql::FindSockfdByUserName(const string &username, int &sockfd)
{
    ostringstream sql_handle;
    sql_handle << "select l.sockfd from user u inner join loginfo l on u.id=l.uid where u.name=\'" << username << '\'';
    return FindTuple(sql_handle.str().c_str(), sockfd);
}

bool IMMySql::FindUidByUserName(const string &username, unsigned int &uid)
{
    ostringstream sql_handle;
    sql_handle << "select id from user where name=\'" << username << '\'';
    return FindTuple(sql_handle.str().c_str(), uid);
}

bool IMMySql::FindUserNameByUid(unsigned int uid, string &username)
{
    ostringstream sql_handle;
    sql_handle << "select name from user where id=" << uid;
    return FindTuple(sql_handle.str().c_str(), username);
}

bool IMMySql::FindUidBySockfd(int sockfd, unsigned int &uid)
{
    ostringstream sql_handle;
    sql_handle << "select uid from loginfo where sockfd=" << sockfd;
    return FindTuple(sql_handle.str().c_str(), uid);
}

void IMMySql::SaveMessage(unsigned int rid, unsigned int sid, const string &message, time_t tt)
{
    ostringstream sql_handle;
    sql_handle << "insert into message (rid,sid,content,sendtime) values (" << rid << ',' << sid << ",\'" << message << "\'," << tt << ')';
    IDUOperation(sql_handle.str().c_str());
}

string IMMySql::GetOnlineUserName(int sockfd)
{
    ostringstream sout, sql_handle;

    sql_handle << "select u.name from user u inner join loginfo l on u.id=l.uid where l.sockfd<>" << sockfd << " order by u.id";

    vector<string> usernames;
    FindTuples(sql_handle.str().c_str(), usernames);
    for (const auto &name : usernames) {
        sout << '@' << name;
    }

    return sout.str();
}

int IMMySql::GetOnlineSockfds(FdSet &fds)
{
    int maxfd = 0;
    ostringstream sql_handle;

    sql_handle << "select sockfd from loginfo where status=1 and sockfd<>-1";

    vector<int> sockfds;
    FindTuples(sql_handle.str().c_str(), sockfds);
    for (auto fd : sockfds) {
        fds.Set(fd);
        maxfd = ((fd + 1) > maxfd ? fd + 1 : maxfd);
    }

    return maxfd;
}

IMServerSocket::~IMServerSocket()
{
    for (int sindex = 0; sindex < mMaxfd; ++sindex) {
        if (mAllfds.IsSet(sindex)) {
            close(sindex);
        }
    }
}

void IMServerSocket::SetServerAddr(const sockaddr_in &ad)
{
    mServerAddr = ad;
    if (mLsCreated) {
        close(mListensockfd);
    } else {
        mLsCreated = true;
    }
    mListensockfd = socket(reinterpret_cast<const sockaddr *>(&ad)->sa_family, SOCK_STREAM, 0);
    mAllfds.Set(mListensockfd);
    updateMaxfd(mListensockfd);
    setNonBlock(mListensockfd);
    int on = 1;
    if (setsockopt(mListensockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        throw NetException(errno, "setsockopt(SO_REUSEADDR) failed!");
    }
}

void IMServerSocket::Bind()
{
    if (bind(mListensockfd, reinterpret_cast<sockaddr *>(&mServerAddr), sizeof(sockaddr)) == -1) {
        throw NetException(errno, "bind failed!");
    }
}

void IMServerSocket::Listen()
{
    int backlog = 10;
    if (listen(mListensockfd, backlog) == -1) {
        throw NetException(errno, "listen failed!");
    }
}

FdSet IMServerSocket::Accept()
{
    FdSet readfds, ret;
    readfds.Set(mListensockfd);
    switch (select(mListensockfd + 1, &readfds, NULL, NULL, NULL)) {
    case 0:
        throw NetException(errno, "select timeout!");
        break;
    case -1:
        if (errno != EINTR) {
            throw NetException(errno, "select error!");
            break;
        } else {
            return FdSet();
        }
    default:
        int sockfd_n;
        if (readfds.IsSet(mListensockfd)) {
            socklen_t sin_len = sizeof(sockaddr_in);
            sockaddr_in client_addr;
            for (;;) {
                if ((sockfd_n = accept(mListensockfd, reinterpret_cast<sockaddr *>(&client_addr), &sin_len)) == -1) {
                    if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                        break;
                        throw NetException(errno, "no client to accept!");
                    } else {
                        break;
                        throw NetException(errno, "accept failed!");
                    }
                } else {
                    //std::cout << "IMServerSocket: " << sockfd_n << " accept success!" << std::endl;
                    setNonBlock(sockfd_n);
                    mAllfds.Set(sockfd_n);
                    ret.Set(sockfd_n);
                    updateMaxfd(sockfd_n);
                    ostringstream sout;
                    sout << "Accept client success, Socket: " << sockfd_n << " IP: " << inet_ntoa(client_addr.sin_addr);
                    Logger::Log(sout.str());
                }
            }
        } else {
            throw NetException(errno, "accept failed(socket not readable)!");
        }
        break;
    }
    return ret;
}

FdSet IMServerSocket::Recv(FdSet &readfds, MsgQueueSet &rmqs)
{
    FdSet bak_readfds = readfds, accfds;
    bak_readfds.Set(mListensockfd);
    auto error_process = [this, &rmqs](int fd, FdSet &fds) {
        fds.Clear(fd);
        CloseSocket(fd);
    };
    int shit;
    //std::cout << "maxfd: " << maxfd << "\t" << flush;
    switch (shit = select(mMaxfd, &bak_readfds, NULL, NULL, NULL)) {
    case 0:
        throw NetException(errno, "select timeout(no socket is readable)!");
        break;
    case -1:
        if (errno == EINTR) {
            return bak_readfds;
        }
        throw NetException(errno, "select error at IMServerSocket::Recv()!");
        break;
    default:
        //std::cout << shit << " has been selected(recv)" << std::endl;
        for (int sindex = 0; sindex < mMaxfd; ++sindex) {
            if (!bak_readfds.IsSet(sindex)) {
                continue;
            }
            if (sindex == mListensockfd) {
                bak_readfds.Clear(sindex);
                accfds = Accept();
                continue;
            }
            FdSet rfds;
            rfds.Set(sindex);
            timeval tt{0, 0};
            while (select(sindex + 1, &rfds, NULL, NULL, &tt) > 0) {
                auto &mq = rmqs[sindex];
                if (mq.empty() || mq.back().size() == reinterpret_cast<MessageHeader *>(mq.back().data())->length) {
                    /* message queue is empty OR last receive packet is complete */
                    MsgType t_msg;
                    // recv message hdr
                    unsigned int buf_sz = sizeof(MessageHeader);
                    t_msg.resize(buf_sz);
                    unsigned int r_sz = recv(sindex, t_msg.data(), buf_sz, 0);
                    if (r_sz <= 0) {
                        error_process(sindex, readfds);
                        break;
                    }
                    t_msg.resize(r_sz);
                    // if hdr recv completely, recv message text
                    if (r_sz == buf_sz) {
                        buf_sz = reinterpret_cast<MessageHeader *>(t_msg.data())->length;
                        if (r_sz < buf_sz) {
                            t_msg.resize(buf_sz);
                            r_sz = recv(sindex, t_msg.data() + sizeof(MessageHeader), buf_sz - sizeof(MessageHeader), 0);
                            if (r_sz <= 0) {
                                error_process(sindex, readfds);
                                break;
                            }
                            t_msg.resize(sizeof(MessageHeader) + r_sz);
                        }
                    }
                    // no matter packet is complete or not, push to message queue
                    mq.push(std::move(t_msg));
                } else {
                    /* last receive packet is not complete */
                    auto &msg = mq.back();
                    unsigned int buf_sz = sizeof(MessageHeader), origin_sz, r_sz;
                    // if last packet's hdr doesn't recv completely, continue to recv hdr
                    origin_sz = msg.size();
                    if (origin_sz < sizeof(MessageHeader)) {
                        msg.resize(buf_sz);
                        r_sz = recv(sindex, msg.data() + origin_sz, buf_sz - origin_sz, 0);
                        if (r_sz <= 0) {
                            error_process(sindex, readfds);
                            break;
                        }
                        msg.resize(r_sz + origin_sz);
                    }
                    // when hdr recv completely, recv message text
                    origin_sz = msg.size();
                    if (origin_sz >= sizeof(MessageHeader)) {
                        buf_sz = reinterpret_cast<MessageHeader *>(msg.data())->length;
                        if (origin_sz < buf_sz) {
                            msg.resize(buf_sz);
                            r_sz = recv(sindex, msg.data() + origin_sz, buf_sz - origin_sz, 0);
                            if (r_sz <= 0) {
                                error_process(sindex, readfds);
                                break;
                            }
                            msg.resize(r_sz + origin_sz);
                        }
                    }
                }
            }
        }
        break;
    }

    return bak_readfds | accfds;
}

void IMServerSocket::Send(const FdSet &wfds, MsgQueueSet &mqs)
{
    int shit;
    FdSet writefds = wfds;
    switch (shit = select(mMaxfd, NULL, &writefds, NULL, NULL)) {
    case -1:
        throw NetException(errno, "select error at IMServerSocket::Send()!");
        break;
    case 0:
        if (errno != EINTR) {
            throw NetException(errno, "select timeout(no socket is writable)!");
            break;
        }
        return;
    default:
        //std::cout << shit << " has been selected(write)" << std::endl;
        for (int sindex = 0; sindex < mMaxfd; ++sindex) {
            if (writefds.IsSet(sindex)) {
                int s_size;
                auto &mq = mqs[sindex];
                while (!mq.empty()) {
                    auto &msg = mq.front();
                    auto temp = msg.data();
                    //print_message(temp);
                    if ((s_size = send(sindex, temp, reinterpret_cast<const MessageHeader *>(temp)->length, 0)) <= 0) {
                        if (s_size == 0) {
                            //std::cerr << "IMServerSocket " << sindex << ": Connection Terminate!" << std::endl;
                            //CloseSocket(sindex);
                        } else if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                            throw NetException(errno, "socket is not writable!");
                        } else {
                            throw NetException(errno, "send failed!");
                        }
                    }
                    mq.pop();
                }
            }
        }
        break;
    }
}

FdSet IMServerSocket::GetAllFds() const
{
    FdSet ret = mAllfds;
    ret.Clear(mListensockfd);
    return ret;
}

void IMServerSocket::CloseSocket(int fd)
{
    //std::cout << "PID:\t" << getpid() << " Close Socket: " << fd << std::endl;
    close(fd);
    mAllfds.Clear(fd);
    updateMaxfd(-1);
}

void IMServerSocket::setNonBlock(const int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        throw NetException(errno, "fcntl(F_GETFL) failed!");
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw NetException(errno, "fcntl(F_SETFL) failed!");
    }
}

void IMServerSocket::updateMaxfd(int fd)
{
    if (fd < mMaxfd) {
        int tmax = -1;
        for (int i = 0; i < mMaxfd; ++i) {
            if (mAllfds.IsSet(i)) {
                tmax = i;
            }
        }
        mMaxfd = (tmax > fd ? tmax : fd) + 1;
    } else {
        mMaxfd = fd + 1;
    }
    //std::cout<<"maxfd update! maxfd: "<<maxfd<<std::endl;
}

IMServer::IMServer(const char *ip, uint16_t port)
{
    mMysql.IDUOperation("delete from loginfo");
    mMysql.IDUOperation("insert into loginfo (uid,sockfd,status) values (1,-1,1)");

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    bzero(&(server_addr.sin_zero), 8);

    mServerSocket.SetServerAddr(server_addr);
    mServerSocket.Bind();
    mServerSocket.Listen();
    mServerSocket.Accept();
}

IMServer::~IMServer()
{
    mMysql.IDUOperation("delete from loginfo");
}

FdSet IMServer::packUnpack(int maxfd, const FdSet &fds)
{
    FdSet ret;

    for (int i = 0; i != maxfd; ++i) {
        if (fds.IsSet(i)) {
            if (mRmqs[i].empty()) {
                clearClosedSock(i);
                ret |= packLoginBroadcast(i);
                continue;
            }
            auto &rmq = mRmqs[i];
            while (!rmq.empty()) {
                auto &rmsg = rmq.front();
                if (rmsg.size() < sizeof(MessageHeader)) {
                    break;
                }
                auto p_rc = rmsg.data();
                auto p_rmhdr = reinterpret_cast<const MessageHeader *>(p_rc);
                if (rmsg.size() < p_rmhdr->length) {
                    break;
                }

                string raw_mtext(p_rc + sizeof(MessageHeader), p_rc + p_rmhdr->length);
                string mtext(raw_mtext.c_str());

                switch (p_rmhdr->head) {
                case MessageHeader::VERIFY_REQ:
                    switch (p_rmhdr->kind) {
                    case MessageHeader::VERIFY_USERNAME_REQ:
                        packReplyVerifyUserName(i, mtext);
                        ret.Set(i);
                        break;
                    case MessageHeader::VERIFY_PASSWORD_REQ:
                        ret |= packReplyVerifyPassword(i, mtext);
                        break;
                    }
                    break;
                case MessageHeader::MESSAGE_SEND_REQ:
                    switch (p_rmhdr->kind) {
                    case MessageHeader::MESSAGE_SEND_TEXT_REQ:
                        ret |= packForward(i, MessageHeader::MESSAGE_FORWARD_TEXT, mtext);
                        break;
                    case MessageHeader::MESSAGE_SEND_FILE_BEGIN_REQ:
                        ret |= packForward(i, MessageHeader::MESSAGE_FORWARD_FILE_BEGIN, raw_mtext);
                        break;
                    case MessageHeader::MESSAGE_SEND_FILE_REQ:
                        ret |= packForward(i, MessageHeader::MESSAGE_FORWARD_FILE, raw_mtext);
                        break;
                    case MessageHeader::MESSAGE_SEND_FILE_END_REQ:
                        ret |= packForward(i, MessageHeader::MESSAGE_FORWARD_FILE_END, raw_mtext);
                        break;
                    }
                    break;
                case MessageHeader::ONLINE_FRIENDS_REQ:
                    ret.Set(packReplyOnline(i));
                    break;
                case MessageHeader::HISTORY_REQ:
                    ret.Set(packReplyHistory(i, mtext));
                    break;
                case MessageHeader::RECV_FILE_REQ:
                    switch (p_rmhdr->kind) {
                    case MessageHeader::RECV_FILE_YES_REQ:
                        ret |= packRecvFile(i, MessageHeader::RECV_FILE_YES_ACK, mtext);
                        break;
                    case MessageHeader::RECV_FILE_NO_REQ:
                        ret |= packRecvFile(i, MessageHeader::RECV_FILE_NO_ACK, mtext);
                        break;
                    }
                    break;
                }
                rmq.pop();
            }
        }
    }

    return ret;
}

unsigned int IMServer::verifyUserName(int sockfd, const string &username)
{
    if (username == "all") {
        Logger::Log("verify username failed, try to login with username = all");
        return MessageHeader::VERIFY_USERNAME_INCORRECT_ACK;
    }

    ostringstream serr, sql_handle;
    unsigned int uid;
    sql_handle << "select id from user where name=\'" << username << '\'';
    if (!mMysql.FindUidByUserName(username, uid)) {
        Logger::Log(username + " verify username failed.");
        return MessageHeader::VERIFY_USERNAME_INCORRECT_ACK;
    };

    sql_handle.str("");
    sql_handle << "insert into loginfo (uid,sockfd) values (" << uid << ',' << sockfd << ')';
    mMysql.IDUOperation(sql_handle.str().c_str());

    sql_handle.str("");
    sql_handle << "select id from user where name=\'" << username << '\'' << " and passwd is null";
    if (!mMysql.FindTuple(sql_handle.str().c_str(), uid)) {
        Logger::Log(username + " verify username success.");
        return MessageHeader::VERIFY_USERNAME_CORRECT_ACK;
    };

    Logger::Log(username + " verify username success and will set password.");
    return MessageHeader::VERIFY_USERNAME_CORRECT_NO_PASSWORD_ACK;
}

unsigned int IMServer::verifyPassword(int sockfd, const string &passwd)
{
    ostringstream serr, sql_handle;
    unsigned int uid;
    string username;

    if (!mMysql.FindUserNameBySockfd(sockfd, username)) {
        Logger::Log("no exist sockfd, verify password failed.");
        return MessageHeader::VERIFY_PASSWORD_INCORRECT_ACK;
    }

    sql_handle << "select id from user where name=\'" << username << "\' and passwd is null";
    if (mMysql.FindTuple(sql_handle.str().c_str(), uid)) {
        sql_handle.str("");
        sql_handle << "update user set passwd=MD5(\'" << passwd << "\') where name=\'" << username << "\' and passwd is null";
        mMysql.IDUOperation(sql_handle.str().c_str());

        chgLoginStatus(uid, 1);
        Logger::Log(username + " set password success and log in.");
        return MessageHeader::VERIFY_PASSWORD_CORRECT_ACK;
    }

    sql_handle.str("");
    sql_handle << "select id from user where name=\'" << username << "\' and passwd=MD5(\'" << passwd << "\')";
    if (mMysql.FindTuple(sql_handle.str().c_str(), uid)) {
        chgLoginStatus(uid, 1);
        Logger::Log(username + " verify password success and log in.");
        return MessageHeader::VERIFY_PASSWORD_CORRECT_ACK;
    }

    Logger::Log(username + " verify password failed.");
    return MessageHeader::VERIFY_PASSWORD_INCORRECT_ACK;
}

void IMServer::chgLoginStatus(unsigned int uid, bool status)
{
    ostringstream sql_handle;
    sql_handle << "update loginfo set status=" << status << " where uid = " << uid;
    mMysql.IDUOperation(sql_handle.str().c_str());
}

void IMServer::clearClosedSock(int sockfd)
{
    ostringstream sout, sql_handle;
    string username;

    while (!mRmqs[sockfd].empty()) {
        mRmqs[sockfd].pop();
    }
    while (!mSmqs[sockfd].empty()) {
        mSmqs[sockfd].pop();
    }

    sout << "Close Socket: " << sockfd;
    if (mMysql.FindUserNameBySockfd(sockfd, username)) {
        sout << ", " << username << " log out.";
        sql_handle << "delete from loginfo where sockfd=" << sockfd;
        mMysql.IDUOperation(sql_handle.str().c_str());
    }
    Logger::Log(sout.str());
}

void IMServer::packIntoMSL(int sockfd, unsigned int head, unsigned int kind, unsigned int length, const char *data)
{
    MsgType msg(length);
    auto p_mhdr = reinterpret_cast<MessageHeader *>(msg.data());
    p_mhdr->head = head;
    p_mhdr->kind = kind;
    p_mhdr->length = length;
    memcpy(msg.data() + sizeof(MessageHeader), data, length - sizeof(MessageHeader));

    mSmqs[sockfd].push(std::move(msg));
}

int IMServer::packReplyOnline(int sockfd)
{
    string users = mMysql.GetOnlineUserName(sockfd);
    size_t sz = users.length() + sizeof(MessageHeader);
    packIntoMSL(sockfd, MessageHeader::ONLINE_FRIENDS_ACK, MessageHeader::NON_KIND, sz, users.c_str());
    return sockfd;
}

int IMServer::packReplyVerifyUserName(int sockfd, const string &text)
{
    packIntoMSL(sockfd, MessageHeader::VERIFY_ACK, verifyUserName(sockfd, text), sizeof(MessageHeader));
    return sockfd;
}

FdSet IMServer::packReplyVerifyPassword(int sockfd, const string &text)
{
    FdSet ret;
    unsigned int kind = verifyPassword(sockfd, text);

    packIntoMSL(sockfd, MessageHeader::VERIFY_ACK, kind, sizeof(MessageHeader));
    ret.Set(sockfd);

    if (kind == MessageHeader::VERIFY_PASSWORD_CORRECT_ACK) {
        ret |= packRemoteLogin(sockfd);
        ret |= packLoginBroadcast(sockfd);
    }

    return ret;
}

FdSet IMServer::packLoginBroadcast(int sockfd)
{
    FdSet onlinefds;
    int mymaxfd = mMysql.GetOnlineSockfds(onlinefds);
    onlinefds.Clear(sockfd);

    for (int j = 0; j != mymaxfd; ++j) {
        if (onlinefds.IsSet(j)) {
            packReplyOnline(j);
        }
    }

    return onlinefds;
}

FdSet IMServer::packRemoteLogin(int sockfd)
{
    ostringstream sql_handle;
    FdSet ret;
    int kickedfd = -1;

    sql_handle << "select sockfd from loginfo where sockfd<>" << sockfd << " and uid in (select uid from loginfo where sockfd=" << sockfd << ')';
    mMysql.FindTuple(sql_handle.str().c_str(), kickedfd);

    string kicked_username;
    mMysql.FindUserNameBySockfd(kickedfd, kicked_username);

    if (kickedfd != -1) {
        packIntoMSL(sockfd, MessageHeader::REMOTE_LOGIN, MessageHeader::REMOTE_LOGIN_LOGIN, sizeof(MessageHeader));
        ret.Set(sockfd);

        packIntoMSL(kickedfd, MessageHeader::REMOTE_LOGIN, MessageHeader::REMOTE_LOGIN_LOGOUT, sizeof(MessageHeader));
        ret.Set(kickedfd);

        sql_handle.str("");
        sql_handle << "delete from loginfo where sockfd=" << kickedfd;
        mMysql.IDUOperation(sql_handle.str().c_str());

        Logger::Log(kicked_username + " remote login.");
    }

    return ret;
}

FdSet IMServer::packForward(int sockfd, unsigned int mhdr_kind, const string &content)
{
    FdSet ret;
    ostringstream serr, mycontent;

    time_t tt;

    string s_username;
    if (!mMysql.FindUserNameBySockfd(sockfd, s_username)) {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK));
        serr << "Can't mMysql.Find sender's sockfd in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return ret;
    }

    size_t pos = content.find(':');
    if (pos == string::npos) {
        pos = content.find("£º");
    }
    if (pos == string::npos || content.empty() || content[0] != '@') {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_INCORRECT_FORMAT_ACK));
        serr << "Incorrect message format. Sender: " << s_username << " Content: " << content;
        Logger::Log(serr.str());
        return ret;
    }

    unsigned int sid;
    if (!mMysql.FindUidByUserName(s_username, sid)) {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK));
        serr << "Can't mMysql.Find sender's uid in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return ret;
    }

    unsigned int rid;
    string r_username(content.substr(1, pos - 1));
    if (r_username == "all") {
        FdSet onlinefds;
        int maxfd = mMysql.GetOnlineSockfds(onlinefds);
        onlinefds.Clear(sockfd);
        GetSystemTime(tt);
        for (int i = 0; i != maxfd; ++i) {
            if (onlinefds.IsSet(i) && mMysql.FindUidBySockfd(i, rid)) {
                mycontent.str("");
                if (mhdr_kind == MessageHeader::MESSAGE_FORWARD_TEXT) {
                    mycontent << tt;
                }
                mycontent << s_username << content;
                ret.Set(packMessage(i, mhdr_kind, mycontent.str()));
            }
        }
        mMysql.SaveMessage(0, sid, s_username + content, tt);
    } else {
        if (!mMysql.FindUidByUserName(r_username, rid)) {
            ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK));
            serr << "Can't mMysql.Find receiver's uid in database. Receiver: " << r_username;
            Logger::Log(serr.str());
            return ret;
        }
        int rfd;
        if (!mMysql.FindSockfdByUserName(r_username, rfd)) {
            ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_USER_NOT_ONLINE_ACK));
            serr << "Can't mMysql.Find receiver's sockfd in database. Receiver: " << r_username;
            Logger::Log(serr.str());
            return ret;
        }
        if (mhdr_kind == MessageHeader::MESSAGE_FORWARD_TEXT) {
            mycontent << GetSystemTime(tt);
            mMysql.SaveMessage(rid, sid, s_username + content, tt);
        }
        mycontent << s_username << content;
        ret.Set(packMessage(rfd, mhdr_kind, mycontent.str()));
    }
    serr << "Forward Message Success. Sender: " << s_username << ", Receiver: " << r_username << ", Content: " << content;
    Logger::Log(serr.str());
    ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_SUCCESS_ACK));

    return ret;
}

int IMServer::packMessage(int sockfd, unsigned int mhdr_kind, const string &message)
{
    size_t sz = sizeof(MessageHeader) + message.length();
    packIntoMSL(sockfd, MessageHeader::MESSAGE_FORWARD, mhdr_kind, sz, message.c_str());
    return sockfd;
}

int IMServer::packReplyForward(int sockfd, unsigned int mhdr_kind)
{
    packIntoMSL(sockfd, MessageHeader::MESSAGE_SEND_ACK, mhdr_kind, sizeof(MessageHeader));
    return sockfd;
}

int IMServer::packReplyHistory(int sockfd, const string &message)
{
    ostringstream sout, sql_handle, content;
    unsigned int msgnum, uid, ouid;

    sql_handle << "select u.msgnum,u.id from user u inner join loginfo l on u.id=l.uid where l.sockfd=" << sockfd;
    if (!mMysql.FindTuple(sql_handle.str().c_str(), msgnum, uid)) {
        return -1;
    };

    string r_username(message.substr(1, message.length() - 1));
    if (!mMysql.FindUidByUserName(r_username, ouid)) {
        return -1;
    }

    std::stack<string> msg_stack;
    sql_handle.str("");
    if (r_username != "all") {
        sql_handle << "select from_unixtime(sendtime),content from message where sid=" << uid << " and rid=" << ouid
                   << " or sid=" << ouid << " and rid=" << uid
                   << " order by sendtime desc limit " << msgnum;
    } else {
        sql_handle << "select from_unixtime(sendtime),content from message where rid=1"
                   << " order by sendtime desc limit " << msgnum;
    }

    vector<std::tuple<string, string>> msgs;
    mMysql.FindTuples(sql_handle.str().c_str(), msgs);
    for (const auto &msg : msgs) {
        msg_stack.push(get<0>(msg) + get<1>(msg));
    }

    if (msg_stack.empty()) {
        packMessage(sockfd, MessageHeader::MESSAGE_FORWARD_HISTORY, string());
    } else {
        while (!msg_stack.empty()) {
            packMessage(sockfd, MessageHeader::MESSAGE_FORWARD_HISTORY, msg_stack.top());
            msg_stack.pop();
        }
    }

    return sockfd;
}

FdSet IMServer::packRecvFile(int sockfd, unsigned int mhdr_kind, const string &text)
{
    ostringstream serr;
    FdSet ret;

    string r_username;
    if (!mMysql.FindUserNameBySockfd(sockfd, r_username)) {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK));
        serr << "Can't mMysql.Find receiver in database. Receiver: " << r_username;
        Logger::Log(serr.str());
        return ret;
    }

    /*size_t pos = text.mMysql.Find(':');
    if (pos == string::npos) {
        pos = text.mMysql.Find("£º");
    }
    if (pos == string::npos || text.empty() || text[0] != '@') {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_INCORRECT_FORMAT_ACK));
        serr << "Incorrect message format. Recvier: " << r_username;
        Logger::Log(serr.str());
        return ret;
    }*/

    //string s_username(text.substr(1, pos - 1));
    string s_username(text.substr(1));
    int sfd;
    if (!mMysql.FindSockfdByUserName(s_username, sfd)) {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_USER_NOT_ONLINE_ACK));
        serr << "Can't mMysql.Find sender's sockfd in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return ret;
    }

    unsigned int sid;
    if (!mMysql.FindUidByUserName(s_username, sid)) {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK));
        serr << "Can't mMysql.Find sender's uid in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return ret;
    }

    size_t sz = sizeof(MessageHeader) + r_username.length() + text.length();
    packIntoMSL(sfd, MessageHeader::RECV_FILE_ACK, mhdr_kind, sz, (r_username + text).c_str());
    ret.Set(sfd);

    serr << "Forward File Recv Success. Sender: " << s_username << ", Receiver: " << r_username;
    Logger::Log(serr.str());
    ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_SUCCESS_ACK));

    return ret;
}

void IMServer::Main()
{
    for (;;) {
        FdSet allfds = mServerSocket.GetAllFds();
        int maxfd = mServerSocket.GetMaxFd();
        //cout << "allfds: ";
        /*for (int i = 0; i < maxfd; ++i) {
            if (allfds.IsSet(i))
                cout << i << ", ";
        }
        cout << endl;*/
        //cout << "----read----" << endl;
        FdSet readfds = mServerSocket.Recv(allfds, mRmqs);
        /*cout << "readfds: ";
        for (int i = 0; i < maxfd; ++i) {
            if (readfds.IsSet(i))
                cout << i << ", ";
        }
        cout << endl;*/
        //cout << "----pack----" << endl;
        FdSet writefds = packUnpack(maxfd, readfds);
        if (!writefds.IsEmpty()) {
            //cout << "----write----" << endl;
            /*cout << "writefds: ";
            for (int i = 0; i < maxfd; ++i) {
                if (writefds.IsSet(i))
                    cout << i << ", ";
            }
            cout << endl;*/
            mServerSocket.Send(writefds, mSmqs);
        }
    }
}

} // namespace imserver
} // namespace herry
