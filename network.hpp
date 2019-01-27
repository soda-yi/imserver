#ifndef _NETWORK_HPP_
#define _NETWORK_HPP_

#include <netinet/in.h>

#include <string>

namespace herry
{
namespace network
{

struct EndPoint {
    std::string IPAddr;
    unsigned short Port;
};

} // namespace network
} // namespace herry

#endif