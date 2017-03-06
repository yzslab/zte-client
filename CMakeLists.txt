# cmake_minimum_required(VERSION 3.6)
project(zte_client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(SOURCE_FILES main.cpp classes/Zte.cpp classes/DhcpClient.cpp classes/exceptions/DhcpClientExecException.cpp)
set(SOURCE_FILES ${SOURCE_FILES} classes/exceptions/ClientException.cpp classes/exceptions/ClientException.h classes/exceptions/SocketInitRequire.cpp classes/exceptions/SocketInitRequire.h includes/exceptionsSet.h classes/exceptions/ExitException.cpp classes/exceptions/ExitException.h classes/exceptions/EapolFailure.cpp classes/exceptions/EapolFailure.h classes/exceptions/EapolStartTimeout.cpp classes/exceptions/EapolStartTimeout.h)

if(WEB_AUTH)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWEB_AUTH")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEB_AUTH")
    SET(STATIC FALSE)
    set(SOURCE_FILES ${SOURCE_FILES} classes/WebAuth.cpp classes/WebAuth.h classes/exceptions/WebAuthException.cpp classes/exceptions/WebAuthException.h)
endif(WEB_AUTH)

if(STATIC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
endif(STATIC)

add_executable(zte_client ${SOURCE_FILES})
if(WEB_AUTH)
target_link_libraries (zte_client curl)
target_link_libraries (zte_client lept)
target_link_libraries (zte_client tesseract)
endif(WEB_AUTH)
target_link_libraries(zte_client pthread)
