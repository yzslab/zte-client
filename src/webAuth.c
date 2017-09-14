//
// Created by root on 7/20/17.
//

#include <leptonica/allheaders.h>
#include <tesseract/capi.h>
#include <curl/curl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "base64.h"
#include "common.h"
#include "webAuth.h"

struct webAuth {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    char dev[DEV_MAX_LENGTH];
    char ip[16];
    char verifyCodeFilePath[100], cookieFilePath[100];
    int verifyCodeFileFd;
};

static void getIp(webAuth *webAuthClient);
static int getVerifyCode(webAuth *client);
static int ocrVerifyCode(webAuth *client);
static size_t headerCallback(char *buffer, size_t size, size_t nitems, void *success);
static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
static size_t logCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
static void printReason(webAuth *client);

webAuth *createWebAuthClient(const char *username, const char *password, const char *dev) {
    if (!(strlen(username) < USERNAME_MAX_LENGTH && strlen(password) < PASSWORD_MAX_LENGTH && strlen(dev) < DEV_MAX_LENGTH))
        return NULL;
    int timestamp;
    timestamp = time(NULL); // 生成当前时间的Unix时间戳
    
    webAuth *client = malloc(sizeof(struct webAuth));
    if (!client)
        return NULL;

    char *encodedPassword;
    size_t encodedLength;
    encodedPassword = base64_encode(password, strlen(password), &encodedLength);
    strcpy(client->username, username);
    strncpy(client->password, encodedPassword, encodedLength);
    client->password[encodedLength] = '\0';
    free(encodedPassword);
    base64_cleanup();
    strcpy(client->dev, dev);

    // 生成验证码文件路径
    sprintf(client->verifyCodeFilePath, "/tmp/zte_web_auth_verify_code_%d", timestamp);
    printf("Verify code file path: %s\n", client->verifyCodeFilePath);

    // 生成Cookie文件路径
    sprintf(client->cookieFilePath, "/tmp/zte_web_auth_cookie_%d", timestamp);
    printf("Cookie file path: %s\n", client->cookieFilePath);


    // 生成验证结果文件路径
    /*
    sprintf(auth_return, "/tmp/zte_web_auth_return_%d", timestamp);
    printf("Auth result path: %s\n", auth_return);
    */

    if ((client->verifyCodeFileFd = open(client->verifyCodeFilePath, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH)) < 0) {
        printf("Verify code file open: %s", strerror(errno));
        free(client);
        return NULL;
    }

    return client;
}

static void getIp(webAuth *webAuthClient) {
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, webAuthClient->dev, IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    /* display result */
    char *ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    strncpy(webAuthClient->ip, ip, 15);
}

int startWebAuth(webAuth *client) {
    if (!client->username || !client->password)
        return 1;
    int verify_code;
    char auth_body[200], buffer[200], success_return[] = SUCCESS_RETURN, *buffer_pointer, success = 0;
    ftruncate(client->verifyCodeFileFd, 0); // 清空验证码文件
    lseek(client->verifyCodeFileFd, SEEK_SET, 0); // 指针移到文件首
    getIp(client);
    if (getVerifyCode(client)) {
        if ((verify_code = ocrVerifyCode(client)) >= 0) {
            sprintf(auth_body, "userName1=%s&password1=%s&eduuser=%s&edubas=%s&rand=%d", client->username, client->password, client->ip, EDUBAS, verify_code); // 生成表单数据
            printf("auth_body: %s\n", auth_body);
            CURL *curl;
            CURLcode res;

            curl = curl_easy_init();
            if(curl) {
                curl_easy_setopt(curl, CURLOPT_URL, AUTH_POST_URL);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, auth_body);
                curl_easy_setopt(curl, CURLOPT_COOKIEFILE, client->cookieFilePath);
                curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
                curl_easy_setopt(curl, CURLOPT_HEADERDATA, &success);
                curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);


                res = curl_easy_perform(curl);

                curl_easy_cleanup(curl);

                if(res != CURLE_OK) {
                    printf("curl_easy_perform() failed: %s\n",
                        curl_easy_strerror(res));
                    return 0;
                }

            } else {
                return 0;
            }
            if (!success)
                printReason(client);

            return success;
        } else {
            printf("Fail to do ocr\n");
            return 0;
        }
    } else {
        printf("Fail to get verify code\n");
        return 0;
    }
}

static int getVerifyCode(webAuth *client) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, VERIFY_CODE_URL);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, client->cookieFilePath);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &client->verifyCodeFileFd);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);

      curl_easy_cleanup(curl);

      if(res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
            return 0;
        }

    } else {
        return 0;
    }
    return 1;
}

static int ocrVerifyCode(webAuth *client) {
    TessBaseAPI *handle;
    PIX *img;
    char *text;
    int verify_code;

    if((img = pixRead(client->verifyCodeFilePath)) == NULL) {
        fprintf(stderr, "Error reading image\n");
        return -1;
    }

    handle = TessBaseAPICreate();
    if(TessBaseAPIInit3(handle, NULL, "eng") != 0) {
        fprintf(stderr, "Error initialising tesseract\n");
        return -2;
    }

    TessBaseAPISetImage2(handle, img);
    if(TessBaseAPIRecognize(handle, NULL) != 0) {
        fprintf(stderr, "Error in Tesseract recognition\n");
        return -3;
    }

    if((text = TessBaseAPIGetUTF8Text(handle)) == NULL) {
        fprintf(stderr, "Error getting text\n");
        return -4;
    }

    sscanf(text, "%d", &verify_code); // 把字符类型的OCR结果转换为整数类型

    TessDeleteText(text);
    TessBaseAPIEnd(handle);
    TessBaseAPIDelete(handle);
    pixDestroy(&img);

    return verify_code;
}

static size_t headerCallback(char *buffer, size_t size, size_t nitems, void *success) {
    if (!strncmp(buffer, "Lo", 2)) {
        printf("Respon header Location found\n");

        if (!strncmp(buffer + 10, SUCCESS_RETURN, strlen(SUCCESS_RETURN))) {
            * (char *) success = 1;
            printf("Success location match!\n");
        }
    }

    return size * nitems; // 此处只为了让libcurl认为成功
}

static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    int fd = *((int *) userdata);

    size_t write_size; // 用于保存写入量

    if ((write_size = (size_t) write(fd, ptr, size * nmemb)) <= 0) {
        perror("Verify code write");
        return 0;
    }

    return write_size;
}

static size_t logCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    char *buffer = (char *) alloca(size * nmemb + 1);
    buffer[size * nmemb] = '\0';
    strncpy(buffer, ptr, size * nmemb);
    printf(buffer);
    return size * nmemb;
}

static void printReason(webAuth *client) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, AUTH_FAIL_URL);
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, client->cookieFilePath);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, logCallback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);


        res = curl_easy_perform(curl);

      curl_easy_cleanup(curl);

      if(res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        }

    }
}
