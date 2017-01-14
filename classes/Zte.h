//
// Created by zhensheng on 1/11/17.
//

#ifndef ZTE_CLIENT_ZTE_H
#define ZTE_CLIENT_ZTE_H

using namespace std;

#include <string>
#include <mutex>

#include "../includes/common.h"

class Zte {
public:
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
		PKT_SIZE = 2048,
    };

    Zte(string zteUsername, string ztePassword, string nic): zteUsername(zteUsername), ztePassword(ztePassword), nic(nic) {};

    ~Zte() {
        stop();
    };

    void start();
    void restart();
    void stop();

    bool newConnection();

    status_list getStatus() {
        return status;
    }

    time_t getLastCommunicateTimestamp() {
        return lastCommunicateTimestamp;
    }

    bool terminated() {
        return terminatedTag;
    }

private:
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

    // Login informations
    string zteUsername;
    string ztePassword;
    string nic;


    bool continueZteClient = true;

    const char * salt = "zte142052";

    unsigned char buf[PKT_SIZE], src_addr[6], des_addr[6] = {0x01, 0x80, 0xc2, 0x00, 0x00, 0x03};

    struct eth *eth = (struct eth *)buf;
    struct eapol *eapol = (struct eapol *)(buf + sizeof(struct eth));
    struct eap *eap = (struct eap *)(buf + sizeof(struct eth) + sizeof(struct eapol));
    unsigned char *last = (unsigned char *)(buf + sizeof(struct eth) + sizeof(struct eapol) + sizeof(struct eap));
    struct key *key = (struct key *)(buf + sizeof(struct eth) + sizeof(struct eapol));
    struct md5 *md5 = (struct md5 *)(buf + sizeof(struct eth) + sizeof(struct eapol) + sizeof(struct eap));

    int socketFD;

    bool newConnectionTag = false, terminatedTag = false, reconnectTag = false;

    status_list status;

    time_t lastCommunicateTimestamp = 0;

    int socketInit();

    void sendEth(unsigned short int proto, unsigned short int len);
    void sendEapol(unsigned char type, unsigned short int len);
    void sendEap(unsigned char code, unsigned short int len);
    void eapolStart();
    void eapolLogoff();
    void eapIdentity();
    void eapMd5();
    void eapolKeyRc4();
};


#endif //ZTE_CLIENT_ZTE_H
