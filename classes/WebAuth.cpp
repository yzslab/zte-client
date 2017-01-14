//
// Created by root on 1/12/17.
//

#include "WebAuth.h"

void WebAuth::start() {
    if (!mtx.try_lock())
        return;
    log("Web auth signal received\n");
    while (true) {
        while (!do_web_auth()) {
            log("Web auth failed, retry after 5 seconds\n");
            sleep(5);
            ip = getIp();
        }
        log("Web auth success!\n");
        break;
    }
    mtx.unlock();
}

bool WebAuth::web_auth_inital() {
    int timestamp;
    timestamp = time(NULL); // 生成当前时间的Unix时间戳

    // 生成验证码文件路径
    sprintf(verify_code_file_path, "/tmp/zte_web_auth_verify_code_%d", timestamp);
    log("Verify code file path: %s\n", verify_code_file_path);

    // 生成Cookie文件路径
    sprintf(cookie_file_path, "/tmp/zte_web_auth_cookie_%d", timestamp);
    log("Cookie file path: %s\n", cookie_file_path);

    ip = getIp();

    // 生成验证结果文件路径
    /*
    sprintf(auth_return, "/tmp/zte_web_auth_return_%d", timestamp);
    printf("Auth result path: %s\n", auth_return);
    */

    if ((verify_code_file_fd = open(verify_code_file_path, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH)) < 0) {
        log("Verify code file open: %s", strerror(errno));
        throw new WebAuthException();
        return false;
    }

    /*
    if ((web_auth_return_file_fd = open(auth_return, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH)) < 0) {
        perror("Auth return file open");
        return -1;
    }
    */


    return 0;
}

bool WebAuth::do_web_auth() {
    int verify_code;
    char auth_body[200], buffer[200], success_return[] = SUCCESS_RETURN, *buffer_pointer, success = 0;
    ftruncate(verify_code_file_fd, 0); // 清空验证码文件
    lseek(verify_code_file_fd, SEEK_SET, 0); // 指针移到文件首
    if (get_verify_code()) {
        if ((verify_code = ocr_verify_code()) >= 0) {
            sprintf(auth_body, "userName1=%s&password1=%s&eduuser=%s&edubas=%s&rand=%d", username.c_str(), password.c_str(), ip.c_str(), EDUBAS, verify_code); // 生成表单数据
            log("auth_body: %s\n", auth_body);
            CURL *curl;
            CURLcode res;

            curl = curl_easy_init();
            if(curl) {
                curl_easy_setopt(curl, CURLOPT_URL, AUTH_POST_URL);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, auth_body);
                curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path);
                curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
                curl_easy_setopt(curl, CURLOPT_HEADERDATA, &success);
                curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);


                res = curl_easy_perform(curl);

                if(res != CURLE_OK) {
                    log("curl_easy_perform() failed: %s\n",
                            curl_easy_strerror(res));
                    return false;
                }

                curl_easy_cleanup(curl);
            } else {
                return false;
            }
            if (!success)
                printReason();

            return success;
        } else {
            log("Fail to do ocr\n");
            return false;
        }
    } else {
        log("Fail to get verify code\n");
        return false;
    }
}

bool WebAuth::get_verify_code() {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, VERIFY_CODE_URL);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_path);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &verify_code_file_fd);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            log("curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
            return false;
        }

        curl_easy_cleanup(curl);
    } else {
        return false;
    }
    return true;
}

int WebAuth::ocr_verify_code() {
    TessBaseAPI *handle;
    PIX *img;
    char *text;
    int verify_code;

    if((img = pixRead(verify_code_file_path)) == NULL) {
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



size_t WebAuth::header_callback(char *buffer, size_t size, size_t nitems, void *success) {
    if (!strncmp(buffer, "Lo", 2)) {
        log("Respon header Location found\n");

        if (!strncmp(buffer + 10, SUCCESS_RETURN, strlen(SUCCESS_RETURN))) {
            * (char *) success = 1;
            log("Success location match!\n");
        }
    }

    return size * nitems; // 此处只为了让libcurl认为成功
}

size_t WebAuth::write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    int fd = *((int *) userdata);

    size_t write_size; // 用于保存写入量

    if ((write_size = (size_t) write(fd, ptr, size * nmemb)) <= 0) {
        perror("Verify code write");
        return 0;
    }

    return write_size;
}

size_t WebAuth::log_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    char *buffer = (char *) alloca(size * nmemb + 1);
    buffer[size * nmemb] = '\0';
    strncpy(buffer, ptr, size * nmemb);
    log(buffer);
    return size * nmemb;
}

void WebAuth::printReason() {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, AUTH_FAIL_URL);
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_path);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, log_callback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);


        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            log("curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }
}

string WebAuth::getIp() {
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    /* display result */
    return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}