#include "message.hpp"

#include <cstring>

namespace herry
{
namespace message
{

Message::Message(const Message &rhs) : mMsg(new char[rhs.mSize]), mSize(rhs.mSize), mLength(rhs.mLength)
{
    memcpy(mMsg, rhs.mMsg, mLength);
}

Message::Message(Message &&rhs) : mMsg(rhs.mMsg), mSize(rhs.mSize), mLength(rhs.mLength)
{
    rhs.mMsg = nullptr;
    rhs.mSize = 0;
    rhs.mLength = 0;
}

void Message::SetSize(std::size_t n)
{
    if (n <= mSize) {
        return;
    }

    char *new_msg = new char[n];
    memcpy(new_msg, mMsg, mLength);
    delete[] mMsg;
    mMsg = new_msg;
    mSize = n;
}

void Message::SetLength(std::size_t n)
{
    if (n <= mLength || n <= mSize) {
        mLength = n;
        return;
    }

    SetSize(n);
    mLength = n;
}

} // namespace message
} // namespace herry