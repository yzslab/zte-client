//
// Created by root on 1/12/17.
//

#ifndef ZTE_CLIENT_DHCP_H
#define ZTE_CLIENT_DHCP_H

#include <string>
#include <mutex>
#include "Zte.h"


class DhcpClient {
public:
    enum dhcpClientTypeList {
        DHCLIENT,
        UDHCPC
    };

    DhcpClient(dhcpClientTypeList dhcpClientType, std::string dev, std::mutex &mtx);

    void start();

private:
    dhcpClientTypeList dhcpClientType;
    std::string dev;
    std::mutex &mtx;

    pid_t forkPid = -1;

    void execDhcpClient();
};


#endif //ZTE_CLIENT_DHCP_H
