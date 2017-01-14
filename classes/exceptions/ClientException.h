//
// Created by zhensheng on 1/11/17.
//

#ifndef ZTE_CLIENT_ZTEEXCEPTION_H
#define ZTE_CLIENT_ZTEEXCEPTION_H

#include <execinfo.h>
#include <exception>
#include <string>

class ClientException : public std::exception {
public:
    enum {
        STACK_TRACE_SIZE = 100,
    };
    ClientException(const char* file, int line, const std::string what) {
        void *array[STACK_TRACE_SIZE];
        int size;
        size = backtrace(array, STACK_TRACE_SIZE);
        int i = 0;
        detail.append("Exception thrown from ");
        detail.append(file);
        detail.append(" on line ");
        detail.append(std::to_string(line));
        detail.append(", stack trace:\n");
        for (char **stacks = backtrace_symbols(array, size); i < size; ++i) {
            detail.append("\t");
            detail.append(stacks[i]);
            detail.append("\n");
        }
        detail.append("Exception detail:\n\t");
        detail.append(what);
        detail.append("\n\n");
    }
    const char* what() {
        return detail.c_str();
    }
private:
    std::string detail;
};


#endif //ZTE_CLIENT_ZTEEXCEPTION_H
