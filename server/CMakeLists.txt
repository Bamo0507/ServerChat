cmake_minimum_required(VERSION 3.16)

project(chat_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(chat_server
    main.cpp
    core/ChatServer.cpp
    protocol/MessageParser.cpp
    protocol/MessageSerializer.cpp
)