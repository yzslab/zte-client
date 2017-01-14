//
// Created by zhensheng on 1/11/17.
//

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
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
#include <cstring>
#include <cstdarg>
#include <iostream>

#include "../includes/exceptionsSet.h"
#include "Zte.h"
#include "../includes/md5.h"
#include "../includes/rc4.h"

void Zte::sendEth(unsigned short int proto, unsigned short int len){
    int t = sizeof(struct eth) + len;
    memcpy(eth->dest, des_addr, 6);
    memcpy(eth->source, src_addr, 6);
    eth->proto = htons(proto);
    t = send(socketFD, buf, t, 0);
    if(t < 0){
        log("Send(): %s\n", strerror(errno), errno);
        status = EAP_FAILURE;
        throw SocketInitRequire();
        // socketInit();
    }
}

void Zte::sendEapol(unsigned char type, unsigned short int len){
    int t = sizeof(struct eapol) + len;
    eapol->ver = 1;
    eapol->type = type;
    eapol->len = htons(len);
    sendEth(ETH_P_PAE, t);
}

void Zte::sendEap(unsigned char code, unsigned short int len){
    int t = sizeof(struct eap) + len;
    eap->code = code;
    eap->len = htons(t);
    sendEapol(EAPOL_EAPPACKET, t);
}

void Zte::start() {
    ssize_t bytesRead;
    while (true) {
        try {
            // Inital Socket
            while (true) {
                try {
                    socketInit();
                    break;
                } catch (ClientException e) {
                    log(e.what());
                }
                sleep(1);
            }

            try {
                eapolStart();
            } catch (SocketInitRequire e) {
                log("Reinit the socket\n");
                continue;
            }

            while (true) {
                if (!continueZteClient)
                    throw ExitException();
                try {
                    if (reconnectTag) {
                        eapolLogoff();
                        eapolStart();
                        reconnectTag = false;
                    }
                    bytesRead = recv(socketFD, buf, sizeof(buf), 0);
                    if(bytesRead > 0) {
                        if(eth->proto == htons(ETH_P_PAE) && !memcmp(eth->dest, src_addr, 6)){
                            time(&lastCommunicateTimestamp);
                            switch(eapol->type){
                                case EAPOL_EAPPACKET:
                                    switch(eap->code){
                                        case EAP_REQUEST:
                                            status = EAP_REQUEST;
                                            switch(eap->type){
                                                case EAP_TYPE_IDENTITY:
                                                    eapIdentity();
                                                    break;
                                                case EAP_TYPE_NOTIFICATION:
                                                    log("EAP Request Notification\n%s\n", last);
                                                    break;
                                                case EAP_TYPE_MD5:
                                                    eapMd5();
                                                    break;
                                                default:
                                                    log("Unknow eap type: %d\n", eap->type);
                                                    break;
                                            }
                                            break;
                                        case EAP_SUCCESS:
                                            status = EAP_SUCCESS;
                                            // count = count_aim = 0;
                                            log("EAP Success\n");
                                            newConnectionTag = true;
                                            break;
                                        case EAP_FAILURE:
                                            throw EapolFailure();
                                        default:
                                            log("Unknow eapol type: %d\n", eap->code);
                                            break;
                                    }
                                    break;
                                case EAPOL_KEY:
                                    switch(eap->code){
                                        case EAP_KEY_RC4:
                                            eapolKeyRc4();
                                            break;
                                        default:
                                            log("Unknow key type: %d\n", eap->code);
                                            break;
                                    }
                                    break;
                                default:
                                    log("Unknow packet type: %d\n", eapol->type);
                                    break;
                            }
                        }
                    } else {
                        if(status != EAP_SUCCESS) {
                            log("Timeout, restart\n");
                            eapolStart();
                        }
                    }
                } catch (SocketInitRequire e) {
                    log("Reinit the socket\n");
                    break;
                } catch (EapolFailure e) {
                    status = EAP_FAILURE;
                    log("EAP Failure\n");
                    eapolStart();
                }
            }
        } catch (ExitException e) {
            eapolLogoff();
            log("Logoff\n");
            break;
        }
    }
}

bool Zte::newConnection() {
    if (newConnectionTag) {
        newConnectionTag = false;
        return true;
    }
    return false;
}

void Zte::restart() {
    reconnectTag = true;
}

void Zte::stop() {
    continueZteClient = false;
}


int Zte::socketInit() {
    log("Socket Inital\n");
    if ((socketFD = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_PAE))) == -1)
        throw ::ClientException(__FILE__, __LINE__, string("socket(): ") + strerror(errno));

    struct ifreq ifr;

    bzero(&ifr, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, this->nic.c_str());
    ifr.ifr_flags = IFF_UP;
    if(ioctl(socketFD, SIOCSIFFLAGS, &ifr) < 0)
        throw ::ClientException(__FILE__, __LINE__,  string("Set NIC up, ioctl(): ") + strerror(errno));

    bzero(&ifr, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, this->nic.c_str());
    if(ioctl(socketFD, SIOCGIFINDEX, &ifr) < 0)
        throw ::ClientException(__FILE__, __LINE__,  string("ioctl(): ") + strerror(errno));

    struct sockaddr_ll addr;
    bzero(&addr, sizeof(struct sockaddr_ll));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(socketFD, (struct sockaddr*)&addr, sizeof(struct sockaddr_ll)) < 0)
        throw ::ClientException(__FILE__, __LINE__, string("bind(): ") + strerror(errno));

    struct timeval timeout={TIMEOUT,0};
    if(setsockopt(socketFD,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) !=0)
        throw ::ClientException(__FILE__, __LINE__, string("setsockopt(): ") + strerror(errno));

    socklen_t size = sizeof(struct sockaddr_ll);
    if (getsockname(socketFD, (struct sockaddr *)&addr, &size) < 0)
        throw ::ClientException(__FILE__, __LINE__, string("getsockname(): ") + strerror(errno));

    memcpy(src_addr, addr.sll_addr, addr.sll_halen);

    return socketFD;
}

void Zte::eapolStart(){
    sendEapol(EAPOL_START, 0);
    status = EAPOL_START;
    log("EAPOL Start\n");
}

void Zte::eapolLogoff(){
    if (status == EAP_SUCCESS)
         sendEapol(EAPOL_LOGOFF, 0);
    log("EAPOL Logoff\n");
    if (!continueZteClient) {
        close(socketFD);
        terminatedTag = true;
    }
}

void Zte::eapIdentity(){
    int t;
    log("EAP Request Identity\n");
    t = strlen(strcpy((char *) last, zteUsername.c_str()));
    sendEap(EAP_RESPONSE, t);
    log("EAP Response Identity\n");
}

void Zte::eapMd5(){
    int t;
    unsigned char tb[PKT_SIZE];
    MD5_CTX context;
    log("EAP Request MD5\n");
    t = 0;
    tb[t++] = eap->id;
    t += strlen(strcat(strcpy((char *) (tb + t), ztePassword.c_str()), salt));
    memcpy(tb + t, md5->value, 16);
    t += 16;
    MD5Init(&context);
    MD5Update(&context, tb, t);
    MD5Final(tb + t, &context);
    memcpy(md5->value, tb + t, 16);
    t = sizeof(struct md5) + strlen(strcpy((char *) md5->username, zteUsername.c_str())) - 1;
    sendEap(EAP_RESPONSE, t);
    log("EAP Response MD5\n");
}

void Zte::eapolKeyRc4(){
    int t;
    unsigned char enckey[]={0x02,0x0E,0x05,0x04,0x66,0x40,0x19,0x75,0x06,0x06,0x00,0x16,0xD3,0xF3,0xAC,0x02};
    unsigned char wholekey[20];
    log("EAPOL Request Key RC4\n");
    t = sizeof(struct key) + ntohs(key->keylen) - 16;
    //key
    memcpy(wholekey, key->keyiv, 16);
    memcpy(wholekey + 16, key->rc + 4, 4);
    rc4_crypt(enckey, ntohs(key->keylen), wholekey, 20);
    memcpy(key->key, enckey, ntohs(key->keylen));
    //hash
    bzero(key->keysignature, 16);
    hmac_md5((unsigned char *)eapol, sizeof(struct eapol) + t, &key->keyindex, 1, wholekey);
    memcpy(key->keysignature, wholekey, 16);
    sendEapol(EAPOL_KEY, t);
    log("EAPOL Response Key RC4\n");
}