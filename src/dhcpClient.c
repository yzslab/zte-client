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
    char *dhclientArgv[] = {"dhclient", "-d", client->dev, NULL};
    char *udhcpcArgv[] = {"udhcpc", "-i", client->dev, NULL};
    char **dhcpClient;

    switch (client->clientType) {
        case DHCLIENT:
            dhcpClient = dhclientArgv;
            break;
        case UDHCPC:
            dhcpClient = udhcpcArgv;
            break;
        default:
            printf("Skip DHCP Client\n");
            return 0;
    }

    if (client->pid > 0)
        kill(client->pid, SIGKILL);
    client->pid = -1;
    switch (client->pid = fork()) {
        case 0:
            while (1) {
                execvp(dhcpClient[0], dhcpClient);
                perror("execvp()");
                printf("Switch to ");
                if (dhcpClient == dhclientArgv) {
                    dhcpClient = udhcpcArgv;
                } else {
                    dhcpClient = dhclientArgv;
                }
                printf("%s, retry after 1 second, .\n", dhcpClient[0]);
                sleep(1);
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