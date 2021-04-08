cmake_minimum_required(VERSION 2.8)
project(SimpleTcp)

include_directories(include/)

add_definitions(-g)

add_executable(sender src/SimpleTcp.c src/sender.c)

target_link_libraries(sender pthread)

add_executable(receiver src/receiver.c src/SimpleTcp.c)