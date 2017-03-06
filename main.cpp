#include <iostream>
#include <thread>
#include <mutex>
#include <getopt.h>
#include <cstring>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include "includes/common.h"

#include "classes/Zte.h"
#include "classes/DhcpClient.h"

#ifdef WEB_AUTH
#include "classes/WebAuth.h"
#endif

#define DEFAULT_PID_FILE_PATH "/tmp/zte-client.pid"

using namespace std;

void show_usage(void);
bool lock_pid_file(void);
bool unlock_pid_file(void);
static pid_t get_file_lock();
void write_pid(void);
static void reconnect();
static void logoff();

bool daemond = false;

static Zte *zte;
static int pid_fd;
static FILE *pid_fp; // pid文件的指针
static pid_t pid;

int main(int argc, char *argv[]) {

    if (argc == 1) {
        show_usage();
        exit(EXIT_FAILURE);
    }

    openlog("zte-client", LOG_CONS | LOG_NDELAY | LOG_PID, LOG_DAEMON);

    char *dev = nullptr; // 网卡设备名称
    char *username = nullptr; // 用户名
    char *password = nullptr; // 密码
    char const *pid_file_path = nullptr;
    int reconnect_tag = 0, logoff_tag = 0;

    bool asDaemon = false, noDhcpClient = false;

    // 以下两个Vars在web_auth.c中声明
    char *web_auth_username = nullptr, *web_auth_password  = nullptr;

    DhcpClient::dhcpClientTypeList dhcpClientType = DhcpClient::DHCLIENT;

    int ch; // 保存get_long的返回值，亦即短参数，!!!不能使用char，部分架构char类型默认为无符号类型!!!

    // 长参数的定义
    struct option long_options[] = {
            {
                    "help",
                    no_argument,
                    NULL,
                    'h'
            },
            {
                    "daemon",
                    no_argument,
                    NULL,
                    'b'
            },
            {
                    "device",
                    required_argument,
                    NULL,
                    'd'
            },
            {
                    "zteuser",
                    required_argument,
                    NULL,
                    'u'
            },
            {
                    "ztepass",
                    required_argument,
                    NULL,
                    'p'
            },
            {
                    "webuser",
                    required_argument,
                    NULL,
                    'w'
            },
            {
                    "webpass",
                    required_argument,
                    NULL,
                    'k'
            },
            {
                    "DhcpClient",
                    required_argument,
                    NULL,
                    'i'
            },
            {
                    "pidfile",
                    required_argument,
                    NULL,
                    'f'
            },
            {
                    "logfile",
                    required_argument,
                    NULL,
                    'm',
            },
            {
                    "reconnect",
                    no_argument,
                    NULL,
                    'r'
            },
            {
                    "logoff",
                    no_argument,
                    NULL,
                    'l'
            },
            NULL,
    };

    while ((ch = getopt_long(argc, argv, "bhrld:u:p:f:m:i:", long_options, NULL)) != -1) {
        switch (ch) {
            case 0:
                break;
            case 'b':
                asDaemon = true;
                break;
            case 'd':
                dev = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 'p':
                password = optarg;
                break;
            case 'w':
                web_auth_username = optarg;
                break;
            case 'k':
                web_auth_password = optarg;
                break;
            case 'r':
                reconnect_tag = 1;
                break;
            case 'l':
                logoff_tag = 1;
                break;
            case 'f':
                pid_file_path = optarg;
                break;
            case 'i':
                if (!strcmp(optarg, "dhclient"))
                    dhcpClientType = DhcpClient::DHCLIENT;
                else if (!strcmp(optarg, "udhcpc"))
                    dhcpClientType = DhcpClient::UDHCPC;
                else if (!strcmp(optarg, "none"))
                    noDhcpClient = true;
                else {
                    fprintf(stderr, "Unknow DhcpClient client: %s, use dhclien instead.\n", optarg);
                }
                break;
            case 'h':
                show_usage();
                exit(EXIT_SUCCESS);
            case '?':
                exit(EXIT_FAILURE);
            default:
                fprintf(stderr, "Unrecognize option -%c\n", optopt);
        }
    }

    // 如果未设置PID文件与日志文件的路径，则使用默认值
    if (pid_file_path == NULL)
        pid_file_path = DEFAULT_PID_FILE_PATH;

    pid_fd = open (pid_file_path, O_RDWR | O_CREAT | O_SYNC , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // 打开pid文件，若pid文件不存在，则创建

    // pid文件打开失败时的处理
    if (pid_fd < 0){
        perror ("Pid file open failed");
        exit(EXIT_FAILURE);
    }

    if ((pid_fp = fdopen(pid_fd, "r+")) == NULL) {
        perror("fd to fp falied");
        exit(EXIT_FAILURE);
    }

    if (reconnect_tag) {
        reconnect();
        exit(EXIT_SUCCESS);
    }

    if (logoff_tag) {
        logoff();
        exit(EXIT_SUCCESS);
    }

    if (username == nullptr || password == nullptr || dev == nullptr) {
        fprintf(stderr, "--zteuser, --ztepass, --device are vital.\n");
        exit(EXIT_FAILURE);
    }

    if (!lock_pid_file()) {
        fprintf(stderr, "Program is running, pid: %ld, execute %s -l to terminate it.\n", (long) get_file_lock(), argv[0]);
        exit(EXIT_FAILURE);
    }

    zte = new Zte(username, password, dev);

#ifdef WEB_AUTH
    WebAuth *webAuthObj = nullptr;
    if (web_auth_username != nullptr)
        webAuthObj = new WebAuth(web_auth_username, web_auth_password, dev);
#endif

    if (asDaemon) {
        if (unlock_pid_file()) {
            daemon(0, 0);
            daemond = true;
        } else
            exit(EXIT_FAILURE);
        if (!lock_pid_file())
            exit(EXIT_FAILURE);
    }

    write_pid();

    sigset_t global_sig_set, main_thread_sig_set; // 信号集，第一个用于屏蔽所有信号，第二个用于注销
    // 全局屏蔽所有信号(EXCEPT SIGINT)
    sigemptyset(&global_sig_set);
    sigfillset(&global_sig_set);
    sigdelset(&global_sig_set, SIGINT);
    if (sigprocmask(SIG_BLOCK, &global_sig_set, NULL) < 0) {
        perror("Inital signal mask failed");
        exit(EXIT_FAILURE);
    }

    // 接收SIGTERM, SIGHUP信号
    sigemptyset(&main_thread_sig_set);
    sigaddset(&main_thread_sig_set, SIGTERM);
    sigaddset(&main_thread_sig_set, SIGHUP);
    sigaddset(&main_thread_sig_set, SIGCHLD);
    pthread_sigmask(SIG_UNBLOCK, &main_thread_sig_set, NULL);

    signal(SIGCHLD, SIG_IGN); // 让子进程自动退出

    signal(SIGINT, [] (int signo) { zte->stop();});
    signal(SIGTERM, [] (int signo) { zte->stop();});
    signal(SIGHUP, [] (int signo) { zte->restart(); });

    mutex zteClientMtx, dhcpClientMtx
#ifdef WEB_AUTH
    , webAuthMtx
#endif
    ;
    zteClientMtx.lock();
    dhcpClientMtx.lock();
#ifdef WEB_AUTH
    webAuthMtx.lock();
#endif
    thread zteClient([=] () { zte->start();}), *webAuth = nullptr;
    zteClient.detach();

    DhcpClient *dhcpClientObj = NULL;
    if (!noDhcpClient)
        dhcpClientObj = new DhcpClient(dhcpClientType, dev, dhcpClientMtx);

    string ip;

    int times = 0;
    while (true) {
        if (zte->terminated())
            exit(EXIT_SUCCESS);
        if (zte->newConnection()) {
            if (!noDhcpClient)
                dhcpClientObj->start();
#ifdef WEB_AUTH
            if (web_auth_username != nullptr) {
                if (webAuth != nullptr)
                    delete webAuth;
                (webAuth = new thread([&] () {
                    webAuthObj->start();
                }))->detach();
            }
#endif
        }
        sleep(1);
        if (times == 240) {
            if (time(NULL) - zte->getLastCommunicateTimestamp() >= 240) {
                log("Zte authentication timeout");
                zte->restart();
            }
#ifdef WEB_AUTH
            else if (webAuthObj != nullptr) {
                if (webAuth != nullptr) {
                    log("Another thread exists, delete it\n");
                    delete webAuth;
                }
                (webAuth = new thread([&]() {
                    webAuthObj->start();
                }))->detach();
            }
#endif
            times = 0;
        } else
            ++times;
    }
    return 0;
}

void show_usage(void) {
    printf(
            "ZTE Authentication Linux Client 2.1.\n\n"
                    "Based on Dot1x client, re-develop by Zhensheng Yuan @ CCDGUT, 2017-01\n"
                    "Zhensheng Yuan's weblog: http://zhensheng.im\n"
                    "\n"
                    "Usage:\n\n"
                    "Required arguments:\n\n"
                    "\t-u, --zteuser\t\tYour username.\n"
                    "\t-p, --ztepass\t\tYour password.\n\n"
                    "\t-d, --device\t\tSpecify which device to use.\n"
                    "\n"
                    "Optional arguments:\n\n"
#ifdef WEB_AUTH
                    "\t-w, --webuser\t\tWeb auth username\n"
                    "\t-k, --webpass\t\tWeb auth password\n"
#endif
                    "\t-f, --pidfile\t\tPid file path, default is " DEFAULT_PID_FILE_PATH "\n"
                    "\t-i, --DhcpClient\t\tSelect DhcpClient client, only support dhclient and udhcpc, or none for no dhcp client, default is dhclient\n"
                    "\t-b, --daemon\t\tRun as daemon\n"
                    "\t-r, --reconnect\t\tReconnect\n"
                    "\t-l, --logoff\t\tLogoff.\n"
                    "\t-h, --help\t\tShow this help.\n\n"
                    "\n"
                    "About zte-client:\n\n"
                    "\tzte-client is a program developed individually and release under MIT\n"
                    "\tlicense as free software, with NO any relaiontship with ZTE company.\n\n\n"
                    "Build on " __DATE__ " " __TIME__ "\n"
    );
}

void log(const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    va_end(argp);
    if (daemond)
        vsyslog(LOG_INFO, format, argp);
    else
        vfprintf(stdout, format, argp);
}

bool lock_pid_file(void) {
    pid_t pid_lock;
    struct flock flockstr;

    flockstr.l_start = 0;
    flockstr.l_len = 0;
    flockstr.l_whence = SEEK_SET;
    flockstr.l_type = F_WRLCK;

    // 尝试对pid文件加锁，加锁失败后尝试获取锁定文件的进程ID
    if (fcntl(pid_fd, F_SETLK, &flockstr) == -1) {
        // Print PID INFO
        if ((pid_lock = get_file_lock()) > 0) {
            log("Pid %ld has the lock,\n", (long) pid_lock);
        } else {
            log("Lock pid file failed: %s", strerror(errno));
        }
        return false;
    }

    return true;
}

bool unlock_pid_file(void) {
    struct flock flockstr;

    flockstr.l_start = 0;
    flockstr.l_len = 0;
    flockstr.l_whence = SEEK_SET;
    flockstr.l_type = F_UNLCK;

    // 尝试对pid文件unlock
    if (fcntl(pid_fd, F_SETLK, &flockstr) == -1) {
        log("Lock pid file failed: %s", strerror(errno));
        return false;
    }

    return true;
}

void write_pid(void) {
    pid = getpid();
    ftruncate(pid_fd, 0); // 清空pid文件的内容
    if (fprintf(pid_fp, "%ld", (long) pid) < 0) {
        log("write pid file failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fflush(pid_fp); // 把缓存同步到硬盘上，以防pid未写入文件
}

static pid_t get_file_lock() {
    struct flock flockstr;

    flockstr.l_start = 0;
    flockstr.l_len = 0;
    flockstr.l_whence = SEEK_SET;
    flockstr.l_type = F_WRLCK;

    // 尝试获取pid文件的文件锁信息
    if (fcntl(pid_fd, F_GETLK, &flockstr) < 0) {
        log("Get file lock failed: %s", strerror(errno)); // 获取失败时退出
        exit(EXIT_FAILURE);
    } else {
        if (flockstr.l_pid > 0)
            return flockstr.l_pid;
        else
            return 0;
    }
}

static void reconnect() {
    pid_t pid_lock = get_file_lock();
    if (!lock_pid_file() && pid_lock > 0) {
        log("Signal send to %ld.\n", (long) pid_lock);
        kill(pid_lock, SIGHUP);
    } else {
        log("Program is not running\n");
    }
}

static void logoff() {
    pid_t pid_lock = get_file_lock();
    if (!lock_pid_file() && pid_lock > 0) {
        log("Signal send to %ld.\n", (long) pid_lock);
        kill(pid_lock, SIGTERM);
    } else {
        log("Program is not running\n");
    }
}