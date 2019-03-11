#include "imserver.hpp"

#include <arpa/inet.h>
#include <fcntl.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stack>
#include <tuple>

#include "message.hpp"
#include "utility.hpp"

/*
#include <iomanip>
using namespace std;
using std::cout;
using std::endl;
*/

using std::istringstream;
using std::ofstream;
using std::ostringstream;
using std::shared_ptr;
using std::size_t;
using std::string;
using std::time_t;
using std::vector;

using herry::message::MessageHeader;
using herry::select::FdSet;
using herry::select::Select;
using herry::socket::Socket;
using herry::utility::Logger;
using herry::utility::NetException;
using herry::utility::NetExceptionKind;

namespace herry
{
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

void IMMySql::DeleteLoginfoBySockfd(int sockfd)
{
    ostringstream sql_handle;
    sql_handle << "delete from loginfo where sockfd=" << sockfd;
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

int IMMySql::GetOnlineSockfds(std::set<int> &fds)
{
    ostringstream sql_handle;

    sql_handle << "select sockfd from loginfo where status=1 and sockfd<>-1";

    vector<int> sockfds;
    FindTuples(sql_handle.str().c_str(), sockfds);

    fds.insert(sockfds.begin(), sockfds.end());
    if (!fds.empty()) {
        return *fds.rbegin();
    }

    return 0;
}

IMServer::IMServer(const char *ip, uint16_t port)
    : mListenSocket(std::make_shared<Socket>(socket::AddressFamily::InterNetwork, socket::SocketType::Stream, socket::ProtocolType::Tcp))
{
    mMysql.IDUOperation("delete from loginfo");
    mMysql.IDUOperation("insert into loginfo (uid,sockfd,status) values (1,-1,1)");

    mListenSocket->Bind({ip, port});
    mListenSocket->Listen();

    mAllSocket[mListenSocket->GetSocketFd()] = mListenSocket;
}

IMServer::~IMServer()
{
    mMysql.IDUOperation("delete from loginfo");
}

void IMServer::accept()
{
    FdSet readfds;
    readfds.Set(*mListenSocket);

    Select select(&readfds, nullptr);
    select(nullptr);

    if (readfds.IsSet(*mListenSocket)) {
        for (;;) {
            try {
                Socket new_socket(mListenSocket->Accept());
                mAllSocket[new_socket.GetSocketFd()] = std::make_shared<Socket>(std::move(new_socket));
                ostringstream sout;
                sout << "Accept client success, IP: " << new_socket.GetPeerEndPoint().IPAddr;
                Logger::Log(sout.str());
            } catch (NetException &e) {
                if (e.ExceptionKind == NetExceptionKind::ACCEPT_AGAIN) {
                    break;
                }
                throw;
            }
        }
    }
}

void IMServer::mainLoop()
{
    FdSet readfds, writefds;
    for (const auto &socket : mAllSocket) {
        readfds.Set(*socket.second);
    }
    for (const auto &e : mSendSocketList) {
        writefds.Set(*e);
    }

    Select select(&readfds, &writefds);
    select(nullptr);

    for (auto &e : mAllSocket) {
        auto &socket = e.second;
        if (readfds.IsSet(*socket)) {
            if (socket->GetSocketFd() == mListenSocket->GetSocketFd()) {
                accept();
                continue;
            }
            try {
                socket->Recv();
            } catch (NetException &ne) {
                if (ne.ExceptionKind == NetExceptionKind::RECV_ERROR) {
                    throw;
                }
            }
            mRecvSocketList.push_back(socket);
        }
    }
    for (auto it = mSendSocketList.begin(); it != mSendSocketList.end();) {
        auto &socket = *it;
        if (writefds.IsSet(*socket)) {
            try {
                socket->Send();
                if (socket->GetSendMsgQueue().empty()) {
                    mSendSocketList.erase(it++);
                }
                continue;
            } catch (NetException &ne) {
                if (ne.ExceptionKind != NetExceptionKind::SEND_AGAIN) {
                    throw;
                }
            }
            ++it;
        }
    }
}

void IMServer::packUnpack()
{
    for (auto it = mRecvSocketList.begin(); it != mRecvSocketList.end(); mRecvSocketList.erase(it++)) {
        auto &socket = *it;
        if (!socket->IsAvailable()) {
            mMysql.DeleteLoginfoBySockfd(socket->GetSocketFd());
            packLoginBroadcast(socket);
            mAllSocket.erase(socket->GetSocketFd());
            continue;
        }
        const auto &rmq = socket->GetRecvMsgQueue();
        while (!rmq.empty()) {
            const auto &rmsg = rmq.front();
            if (rmsg.GetLength() < sizeof(MessageHeader)) {
                break;
            }
            if (rmsg.GetLength() < rmsg.GetMessageLength()) {
                break;
            }

            string raw_mtext(rmsg.GetMessageContentData(), rmsg.GetMessageContentData() + rmsg.GetMessageLength() - sizeof(MessageHeader));
            string mtext(raw_mtext.c_str());

            switch (rmsg.GetMessageHead()) {
            case MessageHeader::VERIFY_REQ:
                switch (rmsg.GetMessageKind()) {
                case MessageHeader::VERIFY_USERNAME_REQ:
                    packReplyVerifyUserName(socket, mtext);
                    break;
                case MessageHeader::VERIFY_PASSWORD_REQ:
                    packReplyVerifyPassword(socket, mtext);
                    break;
                }
                break;
            case MessageHeader::MESSAGE_SEND_REQ:
                switch (rmsg.GetMessageKind()) {
                case MessageHeader::MESSAGE_SEND_TEXT_REQ:
                    packForward(socket, MessageHeader::MESSAGE_FORWARD_TEXT, mtext);
                    break;
                case MessageHeader::MESSAGE_SEND_FILE_BEGIN_REQ:
                    packForward(socket, MessageHeader::MESSAGE_FORWARD_FILE_BEGIN, raw_mtext);
                    break;
                case MessageHeader::MESSAGE_SEND_FILE_REQ:
                    packForward(socket, MessageHeader::MESSAGE_FORWARD_FILE, raw_mtext);
                    break;
                case MessageHeader::MESSAGE_SEND_FILE_END_REQ:
                    packForward(socket, MessageHeader::MESSAGE_FORWARD_FILE_END, raw_mtext);
                    break;
                }
                break;
            case MessageHeader::ONLINE_FRIENDS_REQ:
                packReplyOnline(socket);
                break;
            case MessageHeader::HISTORY_REQ:
                packReplyHistory(socket, mtext);
                break;
            case MessageHeader::RECV_FILE_REQ:
                switch (rmsg.GetMessageKind()) {
                case MessageHeader::RECV_FILE_YES_REQ:
                    packRecvFile(socket, MessageHeader::RECV_FILE_YES_ACK, mtext);
                    break;
                case MessageHeader::RECV_FILE_NO_REQ:
                    packRecvFile(socket, MessageHeader::RECV_FILE_NO_ACK, mtext);
                    break;
                }
                break;
            }
            socket->PopOutOfRecvQueue();
        }
    }
}

unsigned int IMServer::verifyUserName(const socket_ptr &socket, const string &username)
{
    if (username == "all") {
        Logger::Log("verify username failed, try to login with username = all");
        return MessageHeader::VERIFY_USERNAME_INCORRECT_ACK;
    }

    ostringstream sql_handle;
    unsigned int uid;
    sql_handle << "select id from user where name=\'" << username << '\'';
    if (!mMysql.FindUidByUserName(username, uid)) {
        Logger::Log(username + " verify username failed.");
        return MessageHeader::VERIFY_USERNAME_INCORRECT_ACK;
    };

    sql_handle.str("");
    sql_handle << "insert into loginfo (uid,sockfd) values (" << uid << ',' << socket->GetSocketFd() << ')';
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

unsigned int IMServer::verifyPassword(const socket_ptr &socket, const string &passwd)
{
    ostringstream sql_handle;
    unsigned int uid;
    string username;

    if (!mMysql.FindUserNameBySockfd(socket->GetSocketFd(), username)) {
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

void IMServer::clearClosedSock(const socket_ptr &socket)
{
    ostringstream sout, sql_handle;
    string username;

    mAllSocket.erase(socket->GetSocketFd());

    int sockfd = socket->GetSocketFd();
    sout << "Close Socket: " << sockfd;
    if (mMysql.FindUserNameBySockfd(sockfd, username)) {
        sout << ", " << username << " log out.";
        sql_handle << "delete from loginfo where sockfd=" << sockfd;
        mMysql.IDUOperation(sql_handle.str().c_str());
    }
    Logger::Log(sout.str());
}

void IMServer::packIntoMSL(const socket_ptr &socket, unsigned int head, unsigned int kind, unsigned int length, const char *data)
{
    message::Message msg;
    msg.SetLength(sizeof(MessageHeader) + length);
    msg.SetMessageHead(head);
    msg.SetMessageKind(kind);
    msg.SetMessageLength(sizeof(MessageHeader) + length);
    memcpy(msg.GetMessageContentData(), data, length);

    socket->PushBackIntoSendQueue(std::move(msg));
    auto it = std::find(mSendSocketList.begin(), mSendSocketList.end(), socket);
    if (it == mSendSocketList.end()) {
        mSendSocketList.push_back(socket);
    } else {
        mSendSocketList.splice(mSendSocketList.end(), mSendSocketList, it);
    }
}

void IMServer::packReplyOnline(const socket_ptr &socket)
{
    int sockfd = socket->GetSocketFd();
    string users = mMysql.GetOnlineUserName(sockfd);
    packIntoMSL(socket, MessageHeader::ONLINE_FRIENDS_ACK, MessageHeader::NON_KIND, users.length(), users.c_str());
}

void IMServer::packReplyVerifyUserName(const socket_ptr &socket, const string &text)
{
    packIntoMSL(socket, MessageHeader::VERIFY_ACK, verifyUserName(socket, text), 0);
}

void IMServer::packReplyVerifyPassword(const socket_ptr &socket, const string &text)
{
    FdSet ret;
    unsigned int kind = verifyPassword(socket, text);

    packIntoMSL(socket, MessageHeader::VERIFY_ACK, kind, 0);

    if (kind == MessageHeader::VERIFY_PASSWORD_CORRECT_ACK) {
        packRemoteLogin(socket);
        packLoginBroadcast(socket);
    }
}

void IMServer::packLoginBroadcast(const socket_ptr &socket)
{
    std::set<int> onlinefds;
    mMysql.GetOnlineSockfds(onlinefds);
    onlinefds.erase(socket->GetSocketFd());

    for (auto &s : mAllSocket) {
        if (onlinefds.find(s.second->GetSocketFd()) != onlinefds.end()) {
            packReplyOnline(s.second);
        }
    }
}

void IMServer::packRemoteLogin(const socket_ptr &socket)
{
    ostringstream sql_handle;
    int kickedfd = -1, sockfd = socket->GetSocketFd();

    sql_handle << "select sockfd from loginfo where sockfd<>" << sockfd << " and uid in (select uid from loginfo where sockfd=" << sockfd << ')';
    mMysql.FindTuple(sql_handle.str().c_str(), kickedfd);

    string kicked_username;
    mMysql.FindUserNameBySockfd(kickedfd, kicked_username);

    if (kickedfd != -1) {
        auto &kicked_socket = mAllSocket[kickedfd];
        packIntoMSL(socket, MessageHeader::REMOTE_LOGIN, MessageHeader::REMOTE_LOGIN_LOGIN, 0);

        packIntoMSL(kicked_socket, MessageHeader::REMOTE_LOGIN, MessageHeader::REMOTE_LOGIN_LOGOUT, 0);

        sql_handle.str("");
        sql_handle << "delete from loginfo where sockfd=" << kickedfd;
        mMysql.IDUOperation(sql_handle.str().c_str());

        Logger::Log(kicked_username + " remote login.");
    }
}

void IMServer::packForward(const socket_ptr &socket, unsigned int mhdr_kind, const string &content)
{
    ostringstream serr, mycontent;
    int sockfd = socket->GetSocketFd();

    time_t tt;

    string s_username;
    if (!mMysql.FindUserNameBySockfd(sockfd, s_username)) {
        packReplyForward(socket, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK);
        serr << "Can't mMysql.Find sender's sockfd in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return;
    }

    size_t pos = content.find(':');
    if (pos == string::npos) {
        pos = content.find("£º");
    }
    if (pos == string::npos || content.empty() || content[0] != '@') {
        packReplyForward(socket, MessageHeader::MESSAGE_SEND_INCORRECT_FORMAT_ACK);
        serr << "Incorrect message format. Sender: " << s_username << " Content: " << content;
        Logger::Log(serr.str());
        return;
    }

    unsigned int sid;
    if (!mMysql.FindUidByUserName(s_username, sid)) {
        packReplyForward(socket, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK);
        serr << "Can't mMysql.Find sender's uid in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return;
    }

    unsigned int rid;
    string r_username(content.substr(1, pos - 1));
    if (r_username == "all") {
        std::set<int> onlinefds;
        mMysql.GetOnlineSockfds(onlinefds);
        onlinefds.erase(socket->GetSocketFd());
        string &&tt_str = utility::GetSystemTime(tt);
        for (auto e : mAllSocket) {
            if (onlinefds.find(e.second->GetSocketFd()) != onlinefds.end() && mMysql.FindUidBySockfd(e.second->GetSocketFd(), rid)) {
                mycontent.str("");
                if (mhdr_kind == MessageHeader::MESSAGE_FORWARD_TEXT) {
                    mycontent << tt_str;
                }
                mycontent << s_username << content;
                packMessage(e.second, mhdr_kind, mycontent.str());
            }
        }
        mMysql.SaveMessage(0, sid, s_username + content, tt);
    } else {
        if (!mMysql.FindUidByUserName(r_username, rid)) {
            packReplyForward(socket, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK);
            serr << "Can't mMysql.Find receiver's uid in database. Receiver: " << r_username;
            Logger::Log(serr.str());
            return;
        }
        int rfd;
        if (!mMysql.FindSockfdByUserName(r_username, rfd)) {
            packReplyForward(socket, MessageHeader::MESSAGE_SEND_USER_NOT_ONLINE_ACK);
            serr << "Can't mMysql.Find receiver's sockfd in database. Receiver: " << r_username;
            Logger::Log(serr.str());
            return;
        }
        if (mhdr_kind == MessageHeader::MESSAGE_FORWARD_TEXT) {
            mycontent << utility::GetSystemTime(tt);
            mMysql.SaveMessage(rid, sid, s_username + content, tt);
        }
        mycontent << s_username << content;
        auto &r_socket = mAllSocket[rfd];
        packMessage(r_socket, mhdr_kind, mycontent.str());
    }
    serr << "Forward Message Success. Sender: " << s_username << ", Receiver: " << r_username << ", Content: " << content;
    Logger::Log(serr.str());
    packReplyForward(socket, MessageHeader::MESSAGE_SEND_SUCCESS_ACK);
}

void IMServer::packMessage(const socket_ptr &socket, unsigned int mhdr_kind, const string &message)
{
    packIntoMSL(socket, MessageHeader::MESSAGE_FORWARD, mhdr_kind, message.length(), message.c_str());
}

void IMServer::packReplyForward(const socket_ptr &socket, unsigned int mhdr_kind)
{
    packIntoMSL(socket, MessageHeader::MESSAGE_SEND_ACK, mhdr_kind, 0);
}

void IMServer::packReplyHistory(const socket_ptr &socket, const string &message)
{
    ostringstream sout, sql_handle, content;
    unsigned int msgnum, uid, ouid;
    int sockfd = socket->GetSocketFd();

    sql_handle << "select u.msgnum,u.id from user u inner join loginfo l on u.id=l.uid where l.sockfd=" << sockfd;
    if (!mMysql.FindTuple(sql_handle.str().c_str(), msgnum, uid)) {
        return;
    };

    string r_username(message.substr(1, message.length() - 1));
    if (!mMysql.FindUidByUserName(r_username, ouid)) {
        return;
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
        msg_stack.push(std::get<0>(msg) + std::get<1>(msg));
    }

    if (msg_stack.empty()) {
        packMessage(socket, MessageHeader::MESSAGE_FORWARD_HISTORY, string());
    } else {
        while (!msg_stack.empty()) {
            packMessage(socket, MessageHeader::MESSAGE_FORWARD_HISTORY, msg_stack.top());
            msg_stack.pop();
        }
    }
}

void IMServer::packRecvFile(const socket_ptr &socket, unsigned int mhdr_kind, const string &text)
{
    ostringstream serr;

    string r_username;
    if (!mMysql.FindUserNameBySockfd(socket->GetSocketFd(), r_username)) {
        packReplyForward(socket, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK);
        serr << "Can't mMysql.Find receiver in database. Receiver: " << r_username;
        Logger::Log(serr.str());
        return;
    }

    /*
    size_t pos = text.mMysql.Find(':');
    if (pos == string::npos) {
        pos = text.mMysql.Find("£º");
    }
    if (pos == string::npos || text.empty() || text[0] != '@') {
        ret.Set(packReplyForward(sockfd, MessageHeader::MESSAGE_SEND_INCORRECT_FORMAT_ACK));
        serr << "Incorrect message format. Recvier: " << r_username;
        Logger::Log(serr.str());
        return ret;
    }
    */

    string s_username(text.substr(1));
    int sfd;
    if (!mMysql.FindSockfdByUserName(s_username, sfd)) {
        packReplyForward(socket, MessageHeader::MESSAGE_SEND_USER_NOT_ONLINE_ACK);
        serr << "Can't mMysql.Find sender's sockfd in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return;
    }

    unsigned int sid;
    if (!mMysql.FindUidByUserName(s_username, sid)) {
        packReplyForward(socket, MessageHeader::MESSAGE_SEND_USER_NOT_EXIST_ACK);
        serr << "Can't mMysql.Find sender's uid in database. Sender: " << s_username;
        Logger::Log(serr.str());
        return;
    }

    size_t sz = r_username.length() + text.length();
    auto &s_socket = mAllSocket[sfd];
    packIntoMSL(s_socket, MessageHeader::RECV_FILE_ACK, mhdr_kind, sz, (r_username + text).c_str());

    serr << "Forward File Recv Success. Sender: " << s_username << ", Receiver: " << r_username;
    Logger::Log(serr.str());
    packReplyForward(socket, MessageHeader::MESSAGE_SEND_SUCCESS_ACK);
}

void IMServer::Main()
{
    for (;;) {
        //recv();

        mainLoop();
        packUnpack();
        //mRecvSocketList.clear();

        //send();
        //mSendSocketList.clear();
    }
}

} // namespace imserver
} // namespace herry