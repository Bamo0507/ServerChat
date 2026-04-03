#ifndef PRIVATE_CHAT_HPP
#define PRIVATE_CHAT_HPP

#include <string>
#include <vector>
#include <ftxui/dom/elements.hpp>

ftxui::Element PrivateChat(
    const std::string& chatting_with,
    const std::vector<std::string>& private_messages,
    ftxui::Element command_input_element
);

#endif