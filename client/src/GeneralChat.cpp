#include "GeneralChat.hpp"

#include <ftxui/dom/elements.hpp>

using namespace ftxui;

Element GeneralChat(
    const std::vector<UserItem>& users,
    const std::vector<std::string>& server_messages,
    Element command_input_element
) {
    Elements user_elements;
    for (const auto& user : users) {
        std::string user_line = user.name + " - " + user.status;

        if (user.hasPrivateMessageNotification) {
            user_line += " - ◎";
        }

        user_elements.push_back(
            text(user_line)
        );
    }

    Element users_box = vbox({
        text("Usuarios conectados") | bold | color(Color::Blue),
        separator(),
        vbox(std::move(user_elements)) | color(Color::Blue)
    }) |
    color(Color::Blue) |
    border |
    size(HEIGHT, EQUAL, 7);

    Elements message_elements;
    for (const auto& message : server_messages) {
        message_elements.push_back(
            text(message)
        );
    }

    Element messages_box = vbox({
        text("Mensajes del servidor") | bold | color(Color::Red),
        separator(),
        vbox(std::move(message_elements)) | color(Color::Red) | flex
    }) |
    color(Color::Red) |
    border |
    yflex;

    Element input_box = vbox({
        text("Enviar comando") | bold | color(Color::Yellow),
        separator(),
        command_input_element
    }) |
    color(Color::Yellow) |
    border |
    size(HEIGHT, EQUAL, 6);

    return vbox({
        users_box,
        messages_box,
        input_box
    }) |
    yflex;
}