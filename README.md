# 城院校园网Linux客户端 v3.1.x
![#f03c15](https://placehold.it/15/f03c15/000000?text=+) **★ 声明：本项目的创建者与一切销售或推广网络设备的人员无任何合作关系，且不销售或推广任何相关设备，以前没有，目前没有，未来也不会有。**

## 简介
本客户端为Linux中兴认证客户端，已适配东莞理工学院城市学院。

参考Dot1x

## 安装
### 方式一：下载预编译ELF文件
访问[Release Page](https://github.com/yzslab/zte-client/releases)，下载适合你的CPU的版本的ELF文件(zte-client-xxx.bz2)以及tesseract的训练数据(eng.traineddata.bz2)，并解压。
libcurl, libleptonica, libtesseract已静态链接，无需再安装此类库。

如mipsel-glibc:
```
mkdir -p /usr/share/tesseract-ocr/tessdata/
bunzip2 -kc eng.traineddata.bz2 > /usr/share/tesseract-ocr/tessdata/eng.traineddata
bunzip2 -kc zte-client-mipsel-glibc.bz2 > /usr/sbin/zte-client
chmod +x /usr/sbin/zte-client

TESSDATA_PREFIX="/usr/share/tesseract-ocr/tessdata/" \
/usr/sbin/zte-client \
--zteuser 201535010200 \
--ztepass 000000 \
--webuser 13800138000 \
--webpass 00000000 \
--webpass_encoder base64 \
--device eth0 \
--dhcpclient dhclient \
--daemon
```

### 方式二：自编译
需要安装以下运行库:
libcurl, libleptonica, tesseract


#### For Debian Jessie
```
apt-get install libcurl4-openssl-dev libleptonica-dev tesseract-ocr-dev tesseract-ocr-eng gcc git make cmake
git clone https://github.com/yzslab/zte-client.git
cd zte-client
cmake CMakeLists.txt
make
cp ./zte_client /usr/sbin/zte-client
```

#### 交叉编译
Debian Jessie可参考此处安装Toolchains: [CrossToolchains#For_jessie_.28Debian_8.29](https://wiki.debian.org/CrossToolchains#For_jessie_.28Debian_8.29) 

OpenWRT/PandoraBox可到官网下载Toolchains

以Debian 9 Stretch，目标架构mipsel为例

下载源码: 
```
apt-get install build-essential crossbuild-essential-mipsel autoconf libtool pkg-config git

MAKE_JOBS=32
PREFIX="/usr/local/"
SRC_DIR="/usr/src/"
HOST="mipsel"
export CC="/usr/bin/mipsel-linux-gnu-gcc"
export CXX="/usr/bin/mipsel-linux-gnu-g++"

cd ${SRC_DIR}

CURL_VERSION="7.54.1"
LEPT_VERSION="1.74.4"
TESSERACT_VERSION="3.05.01"
LIBJPEG_VERSION="1.5.2"
wget -O- https://curl.haxx.se/download/curl-${CURL_VERSION}.tar.bz2 | tar -xjvf-
wget -O- http://www.leptonica.com/source/leptonica-${LEPT_VERSION}.tar.gz | tar -zxvf-
wget -O- https://github.com/tesseract-ocr/tesseract/archive/${TESSERACT_VERSION}.tar.gz | tar -zxvf-
wget -O- https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${LIBJPEG_VERSION}.tar.gz | tar -zxvf-
```
编译curl: 
```
cd ${SRC_DIR}curl-${CURL_VERSION}
./configure \
--host=${HOST} \
--prefix=${PREFIX}curl-${HOST} \
--disable-debug \
--disable-ares \
--disable-rt \
--disable-largefile \
--disable-libtool-lock \
--disable-ftp \
--disable-ldap \
--disable-ldaps \
--disable-rtsp \
--disable-proxy \
--disable-dict \
--disable-telnet \
--disable-tftp \
--disable-pop3 \
--disable-imap \
--disable-smb \
--disable-smtp \
--disable-gopher \
--disable-manual \
--disable-ipv6 \
--disable-sspi \
--disable-crypto-auth \
--disable-ntlm-wb \
--disable-tls-srp \
--disable-unix-sockets \
--disable-soname-bump \
--without-zlib \
--without-winssl \
--without-darwinssl \
--without-ssl \
--without-gnutls \
--without-polarssl \
--without-mbedtls \
--without-cyassl \
--without-nss \
--without-axtls \
--without-libpsl \
--without-libmetalink \
--without-libssh2 \
--without-librtmp \
--without-winidn \
--without-libidn2 \
--without-nghttp2
make -j${MAKE_JOBS} && make -j${MAKE_JOBS} install
```
编译libjpeg:
```
cd ${SRC_DIR}libjpeg-turbo-${LIBJPEG_VERSION}
autoreconf -fiv
./configure \
--host=${HOST} \
--prefix=${PREFIX}libjpeg-${HOST}
make -j${MAKE_JOBS} && make -j${MAKE_JOBS} install
```
编译leptonica:
```
cd ${SRC_DIR}leptonica-${LEPT_VERSION}
CFLAGS="-I${PREFIX}libjpeg-${HOST}/include/ -L${PREFIX}libjpeg-${HOST}/lib/" \
./configure \
--host=${HOST} \
--prefix=${PREFIX}leptonica-${HOST} \
--without-zlib \
--without-libpng \
--without-giflib \
--without-libtiff \
--without-libwebp \
--without-libopenjpeg
make -j${MAKE_JOBS} && make -j${MAKE_JOBS} install
```
编译tesseract:
```
cd ${SRC_DIR}tesseract-${TESSERACT_VERSION}
./autogen.sh
PKG_CONFIG_PATH="${PREFIX}leptonica-${HOST}/lib/pkgconfig/" \
./configure \
--host=${HOST} \
--disable-largefile \
--disable-graphics \
--prefix=${PREFIX}tesseract-${HOST}
make -j${MAKE_JOBS} && make -j${MAKE_JOBS} install
```
最后参考以下命令编译客户端，输出ELF文件zte-client
```
cd ${SRC_DIR}
git clone https://github.com/yzslab/zte-client.git
cd zte-client
mkdir -p ${HOST}-build
cd ${HOST}-build
${CC} -I${PREFIX}tesseract-${HOST}/include -I${PREFIX}curl-${HOST}/include/ -I${PREFIX}leptonica-${HOST}/include/ ../main.c ../src/zte.c ../src/dhcpClient.c ../src/exception.c ../src/webAuth.c ../src/base64.c -c
${CXX} main.o zte.o dhcpClient.o exception.o webAuth.o base64.o ${PREFIX}curl-${HOST}/lib/libcurl.a ${PREFIX}leptonica-${HOST}/lib/liblept.a ${PREFIX}tesseract-${HOST}/lib/libtesseract.a ${PREFIX}libjpeg-${HOST}/lib/libjpeg.a -lpthread -static-libstdc++ -lrt -ldl -o zte-client
```
查看文件信息
```
bash - # file zte-client
./zte-client: ELF 32-bit LSB shared object, MIPS, MIPS32 rel2 version 1 (SYSV), dynamically linked, interpreter /lib/ld.so.1, for GNU/Linux 3.2.0, BuildID[sha1]=d1ff072bf2f621498368883f7e4c5cbdaae75449, not stripped

bash - # mipsel-linux-gnu-readelf -d ./zte-client 

Dynamic section at offset 0x23c contains 32 entries:
  Tag        Type                         Name/Value
 0x00000001 (NEEDED)                     Shared library: [libpthread.so.0]
 0x00000001 (NEEDED)                     Shared library: [librt.so.1]
 0x00000001 (NEEDED)                     Shared library: [libm.so.6]
 0x00000001 (NEEDED)                     Shared library: [libc.so.6]
 0x00000001 (NEEDED)                     Shared library: [ld.so.1]
 0x0000000c (INIT)                       0x870fc
 0x0000000d (FINI)                       0x604670
 0x00000019 (INIT_ARRAY)                 0x71c640
 0x0000001b (INIT_ARRAYSZ)               564 (bytes)
 0x00000004 (HASH)                       0x36c
 0x00000005 (STRTAB)                     0x24a2c
 0x00000006 (SYMTAB)                     0xab3c
 0x0000000a (STRSZ)                      269369 (bytes)
 0x0000000b (SYMENT)                     16 (bytes)
 0x70000035 (MIPS_RLD_MAP_REL)           0x724264
 0x00000015 (DEBUG)                      0x0
 0x00000003 (PLTGOT)                     0x724520
 0x00000011 (REL)                        0x69b64
 0x00000012 (RELSZ)                      32720 (bytes)
 0x00000013 (RELENT)                     8 (bytes)
 0x70000001 (MIPS_RLD_VERSION)           1
 0x70000005 (MIPS_FLAGS)                 NOTPOT
 0x70000006 (MIPS_BASE_ADDRESS)          0x0
 0x7000000a (MIPS_LOCAL_GOTNO)           6788
 0x70000011 (MIPS_SYMTABNO)              6639
 0x70000012 (MIPS_UNREFEXTNO)            43
 0x70000013 (MIPS_GOTSYM)                0x18cb
 0x6ffffffb (FLAGS_1)                    Flags: PIE
 0x6ffffffe (VERNEED)                    0x69a44
 0x6fffffff (VERNEEDNUM)                 4
 0x6ffffff0 (VERSYM)                     0x66666
 0x00000000 (NULL)                       0x0
```
##### 交叉编译 F.A.Q
###### 1. version `GLIBC_x.xx' not found
通过nm可查看何函数使用了此版本的glibc：
```
nm zte-client | grep "GLIBC_x.xx"
```
如：
```
U fmemopen@GLIBC_2.22
```
libleptonica使用了fmemopen()，可通过汇编的.symver指定glibc版本。
查看目标计算机上的glibc版本，以mipsel为例，注意修改libc.so.6的路径：
```
bash - # /lib/mipsel-linux-gnu/libc.so.6
GNU C Library (Debian EGLIBC 2.13-38+deb7u11) stable release version 2.13, by Roland McGrath et al.
Copyright (C) 2011 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.
There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.
Compiled by GNU CC version 4.7.2.
Compiled on a Linux 3.2.60 system on 2016-05-30.
Available extensions:
	crypt add-on version 2.1 by Michael Glad and others
	GNU Libidn by Simon Josefsson
	Native POSIX Threads Library by Ulrich Drepper et al
	Support for some architectures added on, not maintained in glibc core.
	BIND-8.2.3-T5B
libc ABIs: MIPS_PLT UNIQUE
For bug reporting instructions, please see:
<http://www.debian.org/Bugs/>.
```
可看出glibc版本为2.13。
修改libleptonica源码目录下的文件src/allheaders.h，在
```#define  LEPTONICA_ALLHEADERS_H```
后加入
```asm (".symver fmemopen, fmemopen@GLIBC_2.13");```
然后重新编译libleptonica。

## 使用说明
```
必要参数(使用-l, -r参数不要求以下参数):

	-u, --zteuser		中兴认证用户名
	-p, --ztepass		中兴认证密码
	-d, --device		指定网卡设备，默认为第一个可用网卡设备


可选参数:

	-w, --webuser		天翼认证用户名
	-k, --webpass		天翼认证密码
	-f, --pidfile		pid文件路径，默认为/tmp/zte-client.pid
	-m, --logfile       日志文件路径，前台运行模式下默认输出到标准输出，后台运行模式下默认重定向到/dev/null
	-i, --dhcp		指定DHCP客户端，可以选择dhclient, udhcpc或none(代表不启用DHCP客户端)，默认为dhclient
	-b, --daemon		以守护进程模式运行
	-r, --reconnect		重新连接
	-l, --logoff		注销
	-h, --help		显示帮助信息
```
## 示例
进行中兴认证与天翼认证，并以守护进程模式运行（注意参数--webpass_encoder base64，现在天翼认证密码需要base64编码）:
```
/usr/sbin/zte-client --zteuser username --ztepass password --webuser webusername --webpass webpassword --webpass_encoder base64 --device eth0 --daemon
```
仅进行天翼认证，并以守护进程模式运行：
```
/usr/sbin/zte-client --webuser webusername --webpass webpassword --webpass_encoder base64 --device eth0 --daemon
# 如果没有启用中兴认证，程序是不会自动开始天翼认证的，因此需要发送信号USR1告知程序可以开始天翼认证
killall -USR1 zte-client
```

注销:
```
/usr/sbin/zte-client -l
```

重新进行认证:
```
/usr/sbin/zte-client -r
```

## 参考资料
Dot1x

802.11x协议信息

## 链接
[MIT License](https://opensource.org/licenses/MIT)

[Zhensheng Yuan's weblog: https://zhensheng.im](https://zhensheng.im)

## 协议
**The MIT License (MIT)**

Copyright (c) 2016 Zhensheng Yuan

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
