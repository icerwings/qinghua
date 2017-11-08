
#ifndef __client_h__
#define __client_h__

#include <sys/types.h>
#include <sys/socket.h>
#include "tcp.h"
using namespace std;

enum class ConnStatus {
    CONNECTING,
    CONNECTED,
};

class TcpClient : public Tcp {
public:
    explicit TcpClient(Epoll * epoll) : Tcp(epoll, -1), m_status(ConnStatus::CONNECTING) {}
    virtual ~TcpClient();

    int         Connect(const struct sockaddr * inaddr);
    bool        IsConnected();

protected:
    virtual int OnIoWrite() override;
private:
    ConnStatus      m_status;
};

#endif
