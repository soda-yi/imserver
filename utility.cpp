#include "utility.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>

namespace herry
{
namespace utility
{
std::ofstream Logger::mFout("./IMServer.log", std::ofstream::app);

std::string GetSystemTime(std::time_t &tt)
{
    tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return date;
}

void PrintMessage(char *buf)
{
    auto msghdr = reinterpret_cast<message::MessageHeader *>(buf);
    auto s_size = msghdr->length, m_size = 4U;
    //cout << "size = " << s_size << endl;
    std::cout << std::hex << "0x" << msghdr->head << " 0x" << std::setw(2) << std::setfill('0') << msghdr->kind << " " << std::dec << msghdr->length << std::endl;
    for (auto i = m_size; i != s_size; ++i) {
        std::cout << buf[i];
    }
    std::cout << std::endl;
}

void PrintMessage(const message::MessageHeader *msghdr)
{
    std::cout << std::hex << "0x" << msghdr->head << " 0x" << std::setw(2) << std::setfill('0') << msghdr->kind << " " << std::dec << msghdr->length << std::endl;
    std::cout << std::endl;
}

void PrintMessage(const message::Message &msg)
{
    auto m_size = 4U;
    //cout << "size = " << s_size << endl;
    std::cout << "报文总长度：" << msg.GetMessageLength() << ", 已接收长度：" << msg.GetLength() << std::endl;
    std::cout << std::hex << "0x" << msg.GetMessageHead()
              << " 0x" << std::setw(2) << std::setfill('0') << msg.GetMessageKind()
              << " " << std::dec << msg.GetMessageLength() << std::endl;
    for (auto i = m_size; i != msg.GetLength(); ++i) {
        std::cout << msg.GetData()[i];
    }
    std::cout << std::endl;
}

} // namespace utility
} // namespace herry