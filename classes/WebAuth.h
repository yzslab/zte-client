//
// Created by root on 1/12/17.
//

#ifndef ZTE_CLIENT_WEBAUTH_H
#define ZTE_CLIENT_WEBAUTH_H

#include <string>
#include <leptonica/allheaders.h>
#include <tesseract/capi.h>
#include <curl/curl.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "../includes/common.h"
#include "../includes/exceptionsSet.h"

#define VERIFY_CODE_URL "http://enet.10000.gd.cn:10001/common/image.jsp"
// #define VERIFY_CODE_URL "http://192.168.1.1/verify_code.jpg"
#define AUTH_POST_URL "http://enet.10000.gd.cn:10001/login.do"
#define AUTH_FAIL_URL "http://enet.10000.gd.cn:10001/failed.jsp"
// #define AUTH_POST_URL "http://192.168.1.1/login.do"
#define EDUBAS "113.98.10.136"
#define SUCCESS_RETURN "http://enet.10000.gd.cn:10001/success.jsp"

using namespace std;

class WebAuth {
public:
    WebAuth(string username, string password, string dev) : username(username), password(password), dev(dev) {
        web_auth_inital();
    }

    void start();
    bool get_verify_code();
    int ocr_verify_code();
    bool do_web_auth();
    bool web_auth_inital();


private:
    string username;
    string password;
    string dev;
    string ip;

    int verify_code_file_fd; // 验证码文件的文件描述符

    char verify_code_file_path[100], cookie_file_path[100], auth_return[100], client_ip[16];

    static size_t header_callback(char *buffer, size_t size, size_t nitems, void *success);
    static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t log_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
    string getIp();

    void printReason();

    mutex mtx;
};


#endif //ZTE_CLIENT_WEBAUTH_H
