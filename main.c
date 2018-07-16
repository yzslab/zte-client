#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdarg.h>
#include "src/common.h"
#include "src/zte.h"
#include "src/dhcpClient.h"
#include "src/webAuth.h"
#include "src/base64.h"

#define DEFAULT_PID_FILE_PATH "/tmp/zte-client.pid"


jmp_buf mainEnv;

static void sighandler(int sig);
static void *startZteClientAdapter(void *ptr);
static void *startDhcpClientAdapter(void *ptr);
static void *startWebAuthClientAdapter(void *ptr);
int lock_pid_file(void);
int unlock_pid_file(void);
static pid_t get_file_lock();
void write_pid(void);
static void reconnect();
static void logoff();
static void openLogFile(char *logFilePath);
void showUsage();

typedef enum encoder {
    NONE_ENCODER,
    BASE64_ENCODER
} encoder;

int asDaemond = 0;
static int pid_fd;
static FILE *pid_fp; // pid文件的指针
static pid_t pid;

static void stdoutLogger(char *format, va_list ap);
static void syslogLogger(char *format, va_list ap);

static void (*loggerPointer)(char *, va_list) = stdoutLogger;

int main(int argc, char *argv[]) {
    int exceptionIndex, sig;
    sigset_t sigset; // 信号集，第一个用于屏蔽所有信号，第二个用于注销
    initException();
    exception *ex;
    pthread_t zteClientThread, dhcpClientThread, webAuthClientThread;


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
                    "dhcpclient",
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
                    "syslog",
                    no_argument,
                    NULL,
                    's'
            },
            {
                    "webpass_encoder",
                    required_argument,
                    NULL,
                    'e'
            }
            ,
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

    int ch;
    int asDaemon = 0;
    encoder web_auth_password_enocod = NONE_ENCODER;
    char *dev = NULL, *username = NULL, *password = NULL, *web_auth_username = NULL, *web_auth_password = NULL, reconnect_tag = 0, logoff_tag = 0, *pid_file_path = NULL, *log_file_path = NULL, noDhcpClient = 0, dhcpClientType = DHCLIENT;
    while ((ch = getopt_long(argc, argv, "bhrld:e:u:p:f:m:i:", long_options, NULL)) != -1) {
        switch (ch) {
            case 0:
                break;
            case 'b':
                asDaemon = 1;
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
            case 'm':
                log_file_path = optarg;
                break;
            case 'i':
                if (!strcmp(optarg, "dhclient"))
                    dhcpClientType = DHCLIENT;
                else if (!strcmp(optarg, "udhcpc"))
                    dhcpClientType = UDHCPC;
                else if (!strcmp(optarg, "none"))
                    dhcpClientType = NONE;
                else {
                    fprintf(stderr, "Unknow DhcpClient client: %s, use dhclien instead.\n", optarg);
                }
                break;
            case 'e':
                if (!strcmp(optarg, "base64"))
                    web_auth_password_enocod = BASE64_ENCODER;
                break;
            case 's':
                openlog("zte-client", LOG_PID, LOG_DAEMON);
                loggerPointer = syslogLogger;
                break;
            case 'h':
                showUsage();
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
        perror("Pid file open failed");
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

    if (dev == NULL) {
        fprintf(stderr, "The value of --device can not be empty.\n");
        exit(EXIT_FAILURE);
    }

    if (username == NULL || password == NULL) {
        fprintf(stderr, "--zteuser or --ztepass not set, disable zte authentication.\n");
    }

    if (!lock_pid_file()) {
        fprintf(stderr, "Program is running, pid: %ld, execute %s -l to terminate it.\n", (long) get_file_lock(), argv[0]);
        exit(EXIT_FAILURE);
    }

    
    if (asDaemon) {
        if (unlock_pid_file()) {
            daemon(0, 0);
            asDaemond = 1;
        } else
            exit(EXIT_FAILURE);
        if (!lock_pid_file())
            exit(EXIT_FAILURE);
    }

    if (log_file_path)
        openLogFile(log_file_path);

    write_pid();


    zte *zteClient = NULL;
    if (!(username == NULL || password == NULL)) {
        zteClient = createZteClient(username, password, dev, &exceptionIndex);
    }
    dhcpClient *dhcpClient1 = NULL;
    if (dhcpClientType != NONE)
        dhcpClient1 = createDhcpClient(dev, dhcpClientType);
    webAuth *webAuth1 = NULL;
    char *encodedPassword;
    size_t encodedLength, passwordLength, bufferLength;
    if (web_auth_username && web_auth_password) {
        switch (web_auth_password_enocod) {
            case BASE64_ENCODER:
                passwordLength = strlen(web_auth_password);
                encodedPassword = alloca(bufferLength = (encoded_output_length(passwordLength) + 1));
                if (base64_encode(web_auth_password, passwordLength, &encodedLength, encodedPassword, bufferLength) == NULL) {
                    zteLog("Encode error, skip web auth.\n");
                    goto AFTER_WEB_AUTH;
                }
                encodedPassword[encodedLength] = '\0';
                web_auth_password = encodedPassword;
                break;
        }
        webAuth1 = createWebAuthClient(web_auth_username, web_auth_password, dev);
    }
    AFTER_WEB_AUTH:
    while (1) {
        switch (setjmp(mainEnv)) {
            case 0:
            START_CLIENT:
                // 全局屏蔽所有信号(EXCEPT SIGINT)
                sigemptyset(&sigset);
                sigfillset(&sigset);
                if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
                    perror("Inital signal mask failed");
                    exit(EXIT_FAILURE);
                }

                if (dhcpClient1) {
                    pthread_create(&dhcpClientThread, NULL, startDhcpClientAdapter, dhcpClient1);
                    pthread_detach(dhcpClientThread);
                }
                if (webAuth1) {
                    pthread_create(&webAuthClientThread, NULL, startWebAuthClientAdapter, webAuth1);
                    pthread_detach(webAuthClientThread);
                }
                if (zteClient) {
                    pthread_create(&zteClientThread, NULL, startZteClientAdapter, zteClient);
                    pthread_detach(zteClientThread);
                }

                sigemptyset(&sigset);
                sigaddset(&sigset, SIGUSR1);
                sigaddset(&sigset, SIGHUP);
                sigaddset(&sigset, SIGINT);
                sigaddset(&sigset, SIGTERM);
                sigaddset(&sigset, SIGQUIT);
                // pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

                while (1) {
                    if (!sigwait(&sigset, &sig)) {
                        switch (sig) {
                            case SIGUSR1:
                                if (dhcpClient1)
                                    pthread_kill(dhcpClientThread, SIGUSR2);
                                if (webAuth1)
                                    pthread_kill(webAuthClientThread, SIGUSR2);
                                break;
                            case SIGHUP:
                                if (zteClient)
                                    pthread_cancel(zteClientThread);
                                if (dhcpClient1)
                                    pthread_cancel(dhcpClientThread);
                                if (webAuth1)
                                    pthread_cancel(webAuthClientThread);
                                if (zteClient)
                                    stopZteClient(zteClient);
                                goto START_CLIENT;
                            case SIGINT:
                            case SIGTERM:
                            case SIGQUIT:
                                pthread_cancel(zteClientThread);
                                if (dhcpClient1)
                                    pthread_cancel(dhcpClientThread);
                                if (webAuth1)
                                    pthread_cancel(webAuthClientThread);
                                if (zteClient)
                                    stopZteClient(zteClient);
                                goto END_PROGRAM;
                                break;
                            default:
                                break;
                        }
                    }
                }
                break;
            case -1:
                zteLog("Error\n");
                break;
            case 1:
                ex = getException(exceptionIndex);
                zteLog("Message: %s:%d %s with code %d\n", ex->fileName, ex->line, ex->message, ex->code);
                destroyException(exceptionIndex);
                break;
            default:
                zteLog("Unknow error\n");
                break;
        }
    }
    END_PROGRAM:
    return 0;
}

void zteLog(char *format, ...) {
    va_list argp;
    va_start(argp, format);
    loggerPointer(format, argp);
    va_end(argp);
}

static void stdoutLogger(char *format, va_list ap) {
    vprintf(format, ap);
}

static void syslogLogger(char *format, va_list ap) {
    vsyslog(LOG_INFO, format, ap);
}

static void *startZteClientAdapter(void *ptr) {
    zteLog("Zte Client thread start\n");
    startZteClient(ptr);
    return NULL;
}

static void sighandler(int sig) {
    return;
}

static void *startDhcpClientAdapter(void *ptr) {
    zteLog("DHCP Client thread start\n");
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR2);
    sigaddset(&sigset, SIGCHLD);
    signal(SIGCHLD, SIG_IGN);
    // pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
    int sig;
    while (1) {
        sigwait(&sigset, &sig);
        while (startDhcpClient(ptr));
    }
    return NULL;
}

static void *startWebAuthClientAdapter(void *ptr) {
    zteLog("Web Auth Client thread start\n");
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR2);

    // Unblock SIGUSR2, so as to interrupt sleep()
    pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGUSR2, sighandler);

    int sig;
    sigwait(&sigset, &sig);
    while(1) {
        while (1) {
            if (startWebAuth(ptr))
                break;
            sleep(5);
        }
        sleep(300);
    }
    return NULL;
}

int lock_pid_file(void) {
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
            printf("Pid %ld has the lock,\n", (long) pid_lock);
        } else {
            printf("Lock pid file failed: %s", strerror(errno));
        }
        return 0;
    }

    return 1;
}

int unlock_pid_file(void) {
    struct flock flockstr;

    flockstr.l_start = 0;
    flockstr.l_len = 0;
    flockstr.l_whence = SEEK_SET;
    flockstr.l_type = F_UNLCK;

    // 尝试对pid文件unlock
    if (fcntl(pid_fd, F_SETLK, &flockstr) == -1) {
        printf("Lock pid file failed: %s", strerror(errno));
        return 0;
    }

    return 1;
}

void write_pid(void) {
    pid = getpid();
    ftruncate(pid_fd, 0); // 清空pid文件的内容
    if (fprintf(pid_fp, "%ld", (long) pid) < 0) {
        printf("write pid file failed: %s\n", strerror(errno));
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
        printf("Get file lock failed: %s", strerror(errno)); // 获取失败时退出
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
        printf("Signal send to %ld.\n", (long) pid_lock);
        kill(pid_lock, SIGHUP);
    } else {
        printf("Program is not running\n");
    }
}

static void logoff() {
    pid_t pid_lock = get_file_lock();
    if (!lock_pid_file() && pid_lock > 0) {
        printf("Signal send to %ld.\n", (long) pid_lock);
        kill(pid_lock, SIGTERM);
    } else {
        printf("Program is not running\n");
    }
}

static void openLogFile(char *logFilePath) {
    int fd = open(logFilePath, O_WRONLY | O_APPEND | O_CREAT | O_SYNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open()");
        return;
    }
    if (fd != STDOUT_FILENO)
        dup2(fd, STDOUT_FILENO);
    if (fd != STDERR_FILENO)
        dup2(fd, STDERR_FILENO);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
}

void showUsage(void) {
    printf(
            "ZTE Authentication Linux Client 3.1.\n\n"
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
            "\t-w, --webuser\t\tWeb auth username\n"
                    "\t-k, --webpass\t\tWeb auth password\n"
                    "\t-f, --pidfile\t\tPid file path, default is " DEFAULT_PID_FILE_PATH "\n"
                    "\t-m, --logfile\t\tLog file path, default foreground to stdout, daemon to /dev/null\n"
                    "\t-i, --dhcpclient\t\tSelect DhcpClient client, only support dhclient and udhcpc, or none for no dhcp client, default is dhclient\n"
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
