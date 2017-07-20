//
// Created by root on 7/20/17.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include "common.h"
#include "dhcpClient.h"

struct dhcpClient {
    char dev[DEV_MAX_LENGTH];
    dhcpClientType clientType;
    pid_t pid;
};

dhcpClient *createDhcpClient(const char *dev, dhcpClientType clientType) {
    dhcpClient *client = malloc(sizeof(struct dhcpClient));
    if (!client)
        return NULL;
    if (strlen(dev) >= DEV_MAX_LENGTH)
        return NULL;
    strcpy(client->dev, dev);
    client->clientType = clientType;
    client->pid = -1;
    return client;
}

int startDhcpClient(dhcpClient *client) {
    if (client->clientType == NONE)
        return 0;
    if (client->pid != -1)
        kill(client->pid, SIGKILL);
    client->pid = -1;
    char *dhclientArgv[] = {"dhclient", "-d", client->dev, NULL};
    char *udhcpcArgv[] = {"udhcpc", "-i", client->dev, NULL};
    switch (client->pid = fork()) {
        case 0:
            switch (client->clientType) {
                case DHCLIENT:
                    execvp("dhclient", dhclientArgv);
                    break;
                case UDHCPC:
                    execvp("dhclient", udhcpcArgv);
                    break;
                default:
                    printf("Unknow dhcp client type\n");
            }
            break;
        case -1:
            perror("fork()");
            break;
        default:
            return 0;
    }

    return -1;
}