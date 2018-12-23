#ifndef _MESSAGE_H_
#define _MESSAGE_H_

namespace herry
{
namespace message
{

struct MessageHeader {
    unsigned int head : 8;
    unsigned int kind : 8;
    unsigned int length : 16;

    static constexpr unsigned int NON_KIND = 0x00U;

    static constexpr unsigned int VERIFY_REQ = 0x11U;
    static constexpr unsigned int VERIFY_USERNAME_REQ = 0x00U,
                                  VERIFY_PASSWORD_REQ = 0x01U;

    static constexpr unsigned int MESSAGE_SEND_REQ = 0x12U;
    static constexpr unsigned int MESSAGE_SEND_TEXT_REQ = 0x00U,
                                  MESSAGE_SEND_FILE_REQ = 0x01U,
                                  MESSAGE_SEND_FILE_BEGIN_REQ = 0x02U,
                                  MESSAGE_SEND_FILE_END_REQ = 0x03U;

    static constexpr unsigned int ONLINE_FRIENDS_REQ = 0x13U;

    static constexpr unsigned int HISTORY_REQ = 0x14U;

    static constexpr unsigned int RECV_FILE_REQ = 0x15U;
    static constexpr unsigned int RECV_FILE_YES_REQ = 0x00U,
                                  RECV_FILE_NO_REQ = 0xffU;

    static constexpr unsigned int VERIFY_ACK = 0x71U;
    static constexpr unsigned int VERIFY_USERNAME_CORRECT_ACK = 0x00U,
                                  VERIFY_USERNAME_CORRECT_NO_PASSWORD_ACK = 0x01U,
                                  VERIFY_PASSWORD_CORRECT_ACK = 0x02U,
                                  VERIFY_USERNAME_INCORRECT_ACK = 0xffU,
                                  VERIFY_PASSWORD_INCORRECT_ACK = 0xfeU;

    static constexpr unsigned int MESSAGE_SEND_ACK = 0x72U;
    static constexpr unsigned int MESSAGE_SEND_SUCCESS_ACK = 0x00U,
                                  MESSAGE_SEND_INCORRECT_FORMAT_ACK = 0xffU,
                                  MESSAGE_SEND_USER_NOT_EXIST_ACK = 0xfeU,
                                  MESSAGE_SEND_USER_NOT_ONLINE_ACK = 0xfdU;

    static constexpr unsigned int REMOTE_LOGIN = 0x73U;
    static constexpr unsigned int REMOTE_LOGIN_LOGOUT = 0x00U,
                                  REMOTE_LOGIN_LOGIN = 0x01U;

    static constexpr unsigned int MESSAGE_FORWARD = 0x74U;
    static constexpr unsigned int MESSAGE_FORWARD_TEXT = 0x00U,
                                  MESSAGE_FORWARD_FILE = 0x01U,
                                  MESSAGE_FORWARD_FILE_BEGIN = 0x02U,
                                  MESSAGE_FORWARD_FILE_END = 0x03U,
                                  MESSAGE_FORWARD_HISTORY = 0x04U;

    static constexpr unsigned int ONLINE_FRIENDS_ACK = 0x75U;

    static constexpr unsigned int RECV_FILE_ACK = 0x76U;
    static constexpr unsigned int RECV_FILE_YES_ACK = 0x00U,
                                  RECV_FILE_NO_ACK = 0xffU;
};

}; // namespace message
} // namespace herry

#endif
