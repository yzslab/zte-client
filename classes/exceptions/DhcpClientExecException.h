//
// Created by root on 1/12/17.
//

#ifndef ZTE_CLIENT_DHCPCLIENTEXECEXCEPTION_H
#define ZTE_CLIENT_DHCPCLIENTEXECEXCEPTION_H

#include "ClientException.h"

class DhcpClientExecException : public ClientException {
public:
    DhcpClientExecException(const char* file, int line, const std::string what) : ClientException(file, line, what) {}
};


#endif //ZTE_CLIENT_DHCPCLIENTEXECEXCEPTION_H
