//
// Created by root on 7/20/17.
//

#ifndef ZTE_CLIENT_WEBAUTH_H
#define ZTE_CLIENT_WEBAUTH_H

#define VERIFY_CODE_URL "http://enet.10000.gd.cn:10001/common/image.jsp"
// #define VERIFY_CODE_URL "http://192.168.1.1/verify_code.jpg"
#define AUTH_POST_URL "http://enet.10000.gd.cn:10001/login.do"
#define AUTH_FAIL_URL "http://enet.10000.gd.cn:10001/failed.jsp"
// #define AUTH_POST_URL "http://192.168.1.1/login.do"
#define EDUBAS "113.98.10.136"
#define SUCCESS_RETURN "http://enet.10000.gd.cn:10001/success.jsp"

struct webAuth;
typedef struct webAuth webAuth;
webAuth *createWebAuthClient(const char *username, const char *password, const char *dev);
int startWebAuth(webAuth *client);


#endif //ZTE_CLIENT_WEBAUTH_H
