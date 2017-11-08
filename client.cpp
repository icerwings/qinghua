
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log.h"
#include "net.h"
#include "client.h"
#include "epoll.h"
#include "buff.h"

TcpClient::~TcpClient() {
    OnIoClose();
}

int TcpClient::Connect(const struct sockaddr * inaddr) {
    socklen_t       addrlen     = 0;
    int             ret         = 0;
    if (inaddr == nullptr) {
        return -1;
    }

    if (inaddr->sa_family == AF_INET) {
        addrlen = sizeof(struct sockaddr_in);
    } else if (inaddr->sa_family == AF_INET6) {
        addrlen = sizeof(struct sockaddr_in6);
    } else {
        ErrorLog(inaddr->sa_family).Flush();
        return -1;
    }

    Close(m_sockfd);
    m_sockfd = TcpSocket(0, true);
    if (m_sockfd == -1) {
        return -1;
    }

    do {
        ret = connect(m_sockfd, inaddr, addrlen);
    } while (ret == -1 && errno == EINTR);

    if (ret == 0) {
        m_status = ConnStatus::CONNECTED;
        return 0;
    } else if (errno == EINPROGRESS) {
        m_epollOpr |= EPOLLOUT;
        m_epoll->Operate(m_sockfd, m_epollOpr, this);
        return 0;
    } else {
        char *ipstr = inet_ntoa(((struct sockaddr_in *)inaddr)->sin_addr);
        ErrorLog(ipstr)(errno).Flush();
        return -1;
    }

    return 0;
}

bool TcpClient::IsConnected() {
    return (m_status == ConnStatus::CONNECTED);
}

int TcpClient::OnIoWrite() {
    if (m_status == ConnStatus::CONNECTING) {
        int             optval      = 0;
        socklen_t       optlen      = 0;
        int ret = getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
        if (ret < 0 || optval != 0) {
            ErrorLog(ret)(optval).Flush();
            return -1;
        }
        m_status = ConnStatus::CONNECTED;
    }    

    if (m_wbuff == nullptr || m_wbuff->Empty()) {
        m_epollOpr &= ~EPOLLOUT;
        m_epoll->Operate(m_sockfd, m_epollOpr, this);
    } else {
        return OnTcpWrite();
    }

    return 0;
}


