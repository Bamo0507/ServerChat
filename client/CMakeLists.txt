cmake_minimum_required(VERSION 3.16)
project(chat_cli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG main
)

FetchContent_MakeAvailable(ftxui)

add_executable(chat_cli
  src/main.cpp
  src/GeneralChat.cpp
)

target_link_libraries(chat_cli PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)