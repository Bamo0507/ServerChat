#ifndef GENERAL_CHAT_HPP
#define GENERAL_CHAT_HPP

#include <string>
#include <vector>
#include <ftxui/dom/elements.hpp>

struct UserItem {
    std::string name;
    std::string status;
    bool hasPrivateMessageNotification;
};

ftxui::Element GeneralChat(
    const std::vector<UserItem>& users,
    const std::vector<std::string>& server_messages,
    ftxui::Element command_input_element
);

#endif