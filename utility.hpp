#ifndef _UTILITY_HPP_
#define _UTILITY_HPP_

#include <errno.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <fstream>
#include <stdexcept>

#include "message.hpp"

namespace herry
{
namespace utility
{

enum class NetExceptionKind {
    BIND_ERROR,
    LISTEN_ERROR,
    SELECT_ERROR,
    SELECT_TIMEOUT,
    ACCEPT_ERROR,
    ACCEPT_AGAIN,
    RECV_ERROR,
    RECV_AGAIN,
    RECV_SHUTDOWN,
    SEND_ERROR,
    SEND_AGAIN,
    UNKNOW_EXCEPTION
};

class NetException : public std::runtime_error
{
public:
    NetException(NetExceptionKind exception_kind)
        : std::runtime_error(strerror(errno)),
          Pid(getpid()),
          Errno(errno),
          ExceptionKind(exception_kind) {}

    pid_t Pid;
    int Errno;
    NetExceptionKind ExceptionKind = NetExceptionKind::UNKNOW_EXCEPTION;
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

void PrintMessage(char *buf);
void PrintMessage(const message::MessageHeader *msghdr);
void PrintMessage(const message::Message &msg);

} // namespace utility
} // namespace herry

#endif