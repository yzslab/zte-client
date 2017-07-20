//
// Created by root on 7/19/17.
//
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/if_packet.h>
#include <sys/epoll.h>
#include <pthread.h>
#include "md5.h"
#include "rc4.h"
#include "common.h"
#include "exception.h"
#include "zte.h"

enum status_list {
    TIMEOUT = 1,
    EAPOL_EAPPACKET = 0,
    EAPOL_START = 1,
    EAPOL_LOGOFF = 2,
    EAPOL_KEY = 3,
    EAP_REQUEST = 1,
    EAP_RESPONSE = 2,
    EAP_SUCCESS = 3,
    EAP_FAILURE = 4,
    EAP_TYPE_IDENTITY = 1,
    EAP_TYPE_NOTIFICATION = 2,
    EAP_TYPE_MD5 = 4,
    EAP_KEY_RC4 = 1,
    CLIENT_PREPARED,
    PKT_SIZE = 2048,
};

enum CODE_LIST {
    SOCKET_INIT_REQUIRE = 1,
    ZTE_CLIENT_EAPOL_RESTART,
};

#pragma pack(1)
struct eth
{
    unsigned char dest[6]; //Destination
    unsigned char source[6]; //Source
    unsigned short int proto; //Type
};

struct eapol
{
    unsigned char ver; //Version
    unsigned char type; //Type
    unsigned short int len; //Length
};

struct eap
{
    unsigned char code;//Code
    unsigned char id;//Id
    unsigned short int len; //Length
    unsigned char type; //Type
};

struct md5
{
    unsigned char len;//EAP-MD5 Value-Size
    unsigned char value[16];//EAP-MD5 Value
    unsigned char username[1];//Username
};

struct key
{
    unsigned char keytype;//Key Descriptor Type
    unsigned short int keylen; //Key Length
    unsigned char rc[8];//Replay Counter
    unsigned char keyiv[16];//Key IV
    unsigned char keyindex;//Key Index
    unsigned char keysignature[16];//Key Signature
    unsigned char key[16];//Key
};
#pragma pack()

struct zte {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    char dev[DEV_MAX_LENGTH];
    unsigned char buffer[PKT_SIZE];
    unsigned char srcAddr[6];
    struct eth *eth;
    struct eapol *eapol;
    struct eap *eap;
    unsigned char *last;
    struct key *key;
    struct md5 *md5;
    int socketFD;
    status_list status;

    int *exceptionIndex;
    int epfd;

    jmp_buf zteClientEnv;
};

static const char * salt = "zte142052";
static const unsigned char des_addr[6] = {0x01, 0x80, 0xc2, 0x00, 0x00, 0x03};

static void closeEpfd(zte *zteClient);
static int socketInit(zte *zteClient);
static int sendEth(zte *zteClient, unsigned short int proto, unsigned short int len);
static void sendEapol(zte *zteClient, unsigned char type, unsigned short int len);
static void sendEap(zte *zteClient, unsigned char code, unsigned short int len);
static void eapolStart(zte *zteClient);
static void eapolLogoff(zte *zteClient);
static void eapIdentity(zte *zteClient);
static void eapMd5(zte *zteClient);
static void eapolKeyRc4(zte *zteClient);

zte *createZteClient(const char *username, const char *password, const char *dev, int *exceptionIndex) {
    if (!(strlen(username) < USERNAME_MAX_LENGTH && strlen(password) < PASSWORD_MAX_LENGTH && strlen(dev) < DEV_MAX_LENGTH))
        return NULL;
    zte *ztePtr = malloc(sizeof(struct zte));
    if (!ztePtr)
        return NULL;
    strcpy(ztePtr->username, username);
    strcpy(ztePtr->password, password);
    strcpy(ztePtr->dev, dev);

    ztePtr->eth = (struct eth *)ztePtr->buffer;
    ztePtr->eapol = (struct eapol *)(ztePtr->buffer + sizeof(struct eth));
    ztePtr->eap = (struct eap *)(ztePtr->buffer + sizeof(struct eth) + sizeof(struct eapol));
    ztePtr->last = (unsigned char *)(ztePtr->buffer + sizeof(struct eth) + sizeof(struct eapol) + sizeof(struct eap));
    ztePtr->key = (struct key *)(ztePtr->buffer + sizeof(struct eth) + sizeof(struct eapol));
    ztePtr->md5 = (struct md5 *)(ztePtr->buffer + sizeof(struct eth) + sizeof(struct eapol) + sizeof(struct eap));

    ztePtr->socketFD = -1;
    ztePtr->exceptionIndex = exceptionIndex;
    ztePtr->epfd = -1;

    return ztePtr;
}

static void closeEpfd(zte *zteClient) {
    close(zteClient->epfd);
    zteClient->epfd = -1;
}

static int socketInit(zte *zteClient) {
    printf("Socket init\n");
    if (zteClient->epfd != -1)
        closeEpfd(zteClient);
    if (zteClient->socketFD != -1)
        close(zteClient->socketFD);
    if ((zteClient->socketFD = socket(PF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_P_PAE))) == -1)
        throwException(zteClient->zteClientEnv, __FILE__, __LINE__, SOCKET_INIT_REQUIRE, "socket()", zteClient->exceptionIndex);

    struct ifreq ifr;

    bzero(&ifr, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, zteClient->dev);
    ifr.ifr_flags = IFF_UP;
    if(ioctl(zteClient->socketFD, SIOCSIFFLAGS, &ifr) < 0)
        throwException(zteClient->zteClientEnv, __FILE__, __LINE__,  SOCKET_INIT_REQUIRE, "ioctl()", zteClient->exceptionIndex);

    bzero(&ifr, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, zteClient->dev);
    if(ioctl(zteClient->socketFD, SIOCGIFINDEX, &ifr) < 0)
        throwException(zteClient->zteClientEnv, __FILE__, __LINE__, SOCKET_INIT_REQUIRE, "ioctl()", zteClient->exceptionIndex);

    struct sockaddr_ll addr;
    bzero(&addr, sizeof(struct sockaddr_ll));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(zteClient->socketFD, (struct sockaddr*)&addr, sizeof(struct sockaddr_ll)) < 0)
        throwException(zteClient->zteClientEnv, __FILE__, __LINE__, SOCKET_INIT_REQUIRE, "bind()", zteClient->exceptionIndex);

/*
    struct timeval timeout={TIMEOUT,0};
    if(setsockopt(zteClient->socketFD,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) !=0)
        throwException(zteClient->zteClientEnv, __FILE__, __LINE__, SOCKET_INIT_REQUIRE, "setsockopt()", zteClient->exceptionIndex);
*/

    socklen_t size = sizeof(struct sockaddr_ll);
    if (getsockname(zteClient->socketFD, (struct sockaddr *)&addr, &size) < 0)
        throwException(zteClient->zteClientEnv, __FILE__, __LINE__, SOCKET_INIT_REQUIRE, "setsockopt()", zteClient->exceptionIndex);

    memcpy(zteClient->srcAddr, addr.sll_addr, addr.sll_halen);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = zteClient->socketFD;
    zteClient->epfd = epoll_create(1);
    epoll_ctl(zteClient->epfd, EPOLL_CTL_ADD, zteClient->socketFD, &ev);


    return zteClient->socketFD;
}

int startZteClient(zte *zteClient) {
    struct epoll_event evlist[1];
    int timeout;
    int readBytes;
    exception *ex;
    switch (setjmp(zteClient->zteClientEnv)) {
        case 0:
            socketInit(zteClient);
            break;
        case -1:
            break;
        default:
            ex = getException(*zteClient->exceptionIndex);
            printf("Message: %s:%d %s with code %d\n", ex->fileName, ex->line, ex->message, ex->code);
            destroyException(*zteClient->exceptionIndex);
            perror("error");
    }
    while (1) {
        if (zteClient->status == EAPOL_LOGOFF)
            break;
        switch (setjmp(zteClient->zteClientEnv)) {
            case 0:
                timeout = 3000;
                zteClient->status = EAPOL_START;
                while (recv(zteClient->socketFD, zteClient->buffer, sizeof(zteClient->buffer), 0) > 0);
                eapolStart(zteClient);
                while (1) {
                    switch(epoll_wait(zteClient->epfd, evlist, 1, timeout)) {
                        case -1:
                            longjmp(zteClient->zteClientEnv, SOCKET_INIT_REQUIRE);
                            break;
                        case 0:
                            printf("Heart break timeout, restart zte authentication\n");
                            longjmp(zteClient->zteClientEnv, ZTE_CLIENT_EAPOL_RESTART);
                            break;
                        default:
                            if (evlist[0].events & (EPOLLHUP | EPOLLERR))
                                longjmp(zteClient->zteClientEnv, SOCKET_INIT_REQUIRE);
                            if (!(evlist[0].events & EPOLLIN))
                                break;
                            readBytes = recv(zteClient->socketFD, zteClient->buffer, sizeof(zteClient->buffer), 0);
                            switch (zteClient->eapol->type) {
                                case EAPOL_EAPPACKET:
                                    switch(zteClient->eap->code){
                                        case EAP_REQUEST:
                                            zteClient->status = EAP_REQUEST;
                                            switch(zteClient->eap->type){
                                                case EAP_TYPE_IDENTITY:
                                                    eapIdentity(zteClient);
                                                    break;
                                                case EAP_TYPE_NOTIFICATION:
                                                    printf("EAP Request Notification\n%s\n", zteClient->last);
                                                    break;
                                                case EAP_TYPE_MD5:
                                                    eapMd5(zteClient);
                                                    break;
                                                default:
                                                    printf("Unknow eap type: %d\n", zteClient->eap->type);
                                                    break;
                                            }
                                            break;
                                        case EAP_SUCCESS:
                                            zteClient->status = EAP_SUCCESS;
                                            // count = count_aim = 0;
                                            timeout = 240000;
                                            kill(getpid(), SIGUSR1);
                                            printf("EAP Success\n");
                                            // exit(0);
                                            // newConnectionTag = true;
                                            break;
                                        case EAP_FAILURE:
                                            printf("EAP Failure\n");
                                            longjmp(zteClient->zteClientEnv, ZTE_CLIENT_EAPOL_RESTART);
                                        default:
                                            printf("Unknow eapol type: %d\n", zteClient->eap->code);
                                            break;
                                    }
                                    break;
                                case EAPOL_KEY:
                                    switch(zteClient->eap->code){
                                        case EAP_KEY_RC4:
                                            eapolKeyRc4(zteClient);
                                            break;
                                        default:
                                            printf("Unknow key type: %d\n", zteClient->eap->code);
                                            break;
                                    }
                                    break;
                                default:
                                    printf("Unknow packet type: %d\n", zteClient->eapol->type);
                                    break;
                            }
                            break;
                    }
                }
                break;
            case SOCKET_INIT_REQUIRE:
                socketInit(zteClient);
            case ZTE_CLIENT_EAPOL_RESTART:
            default:
                fflush(stdout);
                fflush(stderr);
                break;
        }
    }
    return 0;
}

void stopZteClient(zte *zteClient) {
    eapolLogoff(zteClient);
}

static int sendEth(zte *zteClient, unsigned short int proto, unsigned short int len) {
    int t = sizeof(struct eth) + len;
    memcpy(zteClient->eth->dest, des_addr, 6);
    memcpy(zteClient->eth->source, zteClient->srcAddr, 6);
    zteClient->eth->proto = htons(proto);
    t = send(zteClient->socketFD, zteClient->buffer, t, 0);
    if(t < 0){
        printf("Send(): %s %d\n", strerror(errno), errno);
        zteClient->status = EAP_FAILURE;
        longjmp(zteClient->zteClientEnv, SOCKET_INIT_REQUIRE);
    }
    return 0;
}

static void sendEapol(zte *zteClient, unsigned char type, unsigned short int len) {
    int t = sizeof(struct eapol) + len;
    zteClient->eapol->ver = 1;
    zteClient->eapol->type = type;
    zteClient->eapol->len = htons(len);
    sendEth(zteClient, ETH_P_PAE, t);
}

static void sendEap(zte *zteClient, unsigned char code, unsigned short int len) {
    int t = sizeof(struct eap) + len;
    zteClient->eap->code = code;
    zteClient->eap->len = htons(t);
    sendEapol(zteClient, EAPOL_EAPPACKET, t);
}

static void eapolStart(zte *zteClient) {
    sendEapol(zteClient, EAPOL_START, 0);
    zteClient->status = EAPOL_START;
    printf("EAPOL Start\n");
}

static void eapolLogoff(zte *zteClient) {
    if (zteClient->status == EAP_SUCCESS)
        sendEapol(zteClient, EAPOL_LOGOFF, 0);
    printf("EAPOL Logoff\n");
/*    if (!continueZteClient) {
        close(socketFD);
        terminatedTag = true;
    }*/
}

static void eapIdentity(zte *zteClient) {
    int t;
    printf("EAP Request Identity\n");
    t = strlen(strcpy((char *) zteClient->last, zteClient->username));
    sendEap(zteClient, EAP_RESPONSE, t);
    printf("EAP Response Identity\n");
}

static void eapMd5(zte *zteClient) {
    int t;
    unsigned char tb[PKT_SIZE];
    MD5_CTX context;
    printf("EAP Request MD5\n");
    t = 0;
    tb[t++] = zteClient->eap->id;
    t += strlen(strcat(strcpy((char *) (tb + t), zteClient->password), salt));
    memcpy(tb + t, zteClient->md5->value, 16);
    t += 16;
    MD5Init(&context);
    MD5Update(&context, tb, t);
    MD5Final(tb + t, &context);
    memcpy(zteClient->md5->value, tb + t, 16);
    t = sizeof(struct md5) + strlen(strcpy((char *) zteClient->md5->username, zteClient->username)) - 1;
    sendEap(zteClient, EAP_RESPONSE, t);
    printf("EAP Response MD5\n");
}

static void eapolKeyRc4(zte *zteClient) {
    int t;
    unsigned char enckey[]={0x02,0x0E,0x05,0x04,0x66,0x40,0x19,0x75,0x06,0x06,0x00,0x16,0xD3,0xF3,0xAC,0x02};
    unsigned char wholekey[20];
    printf("EAPOL Request Key RC4\n");
    t = sizeof(struct key) + ntohs(zteClient->key->keylen) - 16;
    //key
    memcpy(wholekey, zteClient->key->keyiv, 16);
    memcpy(wholekey + 16, zteClient->key->rc + 4, 4);
    rc4_crypt(enckey, ntohs(zteClient->key->keylen), wholekey, 20);
    memcpy(zteClient->key->key, enckey, ntohs(zteClient->key->keylen));
    //hash
    bzero(zteClient->key->keysignature, 16);
    hmac_md5((unsigned char *) zteClient->eapol, sizeof(struct eapol) + t, &zteClient->key->keyindex, 1, wholekey);
    memcpy(zteClient->key->keysignature, wholekey, 16);
    sendEapol(zteClient, EAPOL_KEY, t);
    printf("EAPOL Response Key RC4\n");
}