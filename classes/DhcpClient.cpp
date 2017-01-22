//
// Created by root on 1/12/17.
//

#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>

#include "DhcpClient.h"
#include "../includes/exceptionsSet.h"

DhcpClient::DhcpClient(dhcpClientTypeList dhcpClientType, std::string dev, std::mutex &mtx) : mtx(mtx) {
    this->dhcpClientType = dhcpClientType;
    this->dev = dev;
}

void DhcpClient::start() {
    log("DHCP start signal received\n");
    while (true) {
        // Stop last dhcp client
        if (forkPid > 0)
            kill(forkPid, SIGKILL);

        forkPid = fork();

        if (forkPid < 0) {
            log("fork(): %s", strerror(errno));
            continue;
        }

        if (forkPid == 0) {
            while (true) {
                try {
                    execDhcpClient();
                    break;
                } catch (DhcpClientExecException e) {
                    log(e.what());
                }
            }
        } else
            break;
    }
}

void DhcpClient::execDhcpClient() {
    char *dhclientArgv[] = {"dhclient", "-d", (char *) dev.c_str(), NULL};
    char *udhcpcArgv[] = {"udhcpc", "-i", (char *) dev.c_str(), NULL};
    switch (dhcpClientType) {
        case DHCLIENT:
            execvp("dhclient", dhclientArgv);
            break;
        case UDHCPC:
            execvp("dhclient", udhcpcArgv);
            break;
        default:
            log("Unknow dhcp client type\n");
    }
    throw DhcpClientExecException(__FILE__, __LINE__, (string("execvp(): ") + strerror(errno)).c_str());
}