//
// Created by root on 7/20/17.
//

#ifndef ZTE_CLIENT_DHCPCLIENT_H
#define ZTE_CLIENT_DHCPCLIENT_H

enum dhcpClientType  {
    DHCLIENT,
    UDHCPC,
    NONE
};
typedef enum dhcpClientType dhcpClientType;

struct dhcpClient;
typedef struct dhcpClient dhcpClient;

dhcpClient *createDhcpClient(const char *dev, dhcpClientType);
int startDhcpClient(dhcpClient *client);

#endif //ZTE_CLIENT_DHCPCLIENT_H
