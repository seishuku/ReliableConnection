cmake_minimum_required (VERSION 3.8)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

project("ReliableConnection" LANGUAGES C)

add_executable(${CMAKE_PROJECT_NAME} "ReliableConnection.c" "network.c" "transport.c")

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ws2_32.lib)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC)
endif()
