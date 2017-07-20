cmake_minimum_required(VERSION 3.7)
project(zte_client)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES main.c src/zte.c src/zte.h src/common.h src/exception.c src/exception.h src/webAuth.c src/webAuth.h src/dhcpClient.c src/dhcpClient.h)
add_executable(zte_client ${SOURCE_FILES})

target_link_libraries(zte_client curl)
target_link_libraries(zte_client lept)
target_link_libraries (zte_client tesseract)
target_link_libraries (zte_client pthread)
target_link_libraries (zte_client rt)