#include "select.hpp"
#include "utility.hpp"

namespace herry
{
namespace select
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

int Select::operator()(timeval *timeout)
{
    using utility::NetException;
    using utility::NetExceptionKind;

    int select_result = ::select(mMaxFd, mReadFds, mWriteFds, nullptr, timeout);
    switch (select_result) {
    case 0:
        throw NetException(NetExceptionKind::SELECT_TIMEOUT);
    case -1:
        if (errno != EINTR) {
            throw NetException(NetExceptionKind::SELECT_ERROR);
        } else {
            break;
        }
    default:
        break;
    }

    return select_result;
}

} // namespace select
} // namespace herry