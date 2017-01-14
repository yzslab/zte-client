# cmake_minimum_required(VERSION 3.6)
project(zte_client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(SOURCE_FILES main.cpp classes/Zte.cpp classes/Zte.h classes/exceptions/ClientException.cpp classes/exceptions/ClientException.h classes/exceptions/SocketInitRequire.cpp classes/exceptions/SocketInitRequire.h includes/exceptionsSet.h classes/exceptions/ExitException.cpp classes/exceptions/ExitException.h classes/exceptions/EapolFailure.cpp classes/exceptions/EapolFailure.h classes/exceptions/EapolStartTimeout.cpp classes/exceptions/EapolStartTimeout.h classes/DhcpClient.cpp classes/DhcpClient.h includes/common.h classes/exceptions/DhcpClientExecException.cpp classes/exceptions/DhcpClientExecException.h classes/WebAuth.cpp classes/WebAuth.h classes/exceptions/WebAuthException.cpp classes/exceptions/WebAuthException.h)
add_executable(zte_client ${SOURCE_FILES})

target_link_libraries (zte_client curl)
target_link_libraries (zte_client lept)
target_link_libraries (zte_client tesseract)
target_link_libraries(zte_client pthread)