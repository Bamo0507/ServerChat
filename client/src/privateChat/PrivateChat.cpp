#include "PrivateChat.hpp"

#include <ftxui/dom/elements.hpp>

using namespace ftxui;

Element PrivateChat(
    const std::string& chatting_with,
    const std::vector<std::string>& private_messages,
    Element command_input_element
) {
    Elements message_elements;
    for (const auto& message : private_messages) {
        message_elements.push_back(text(message));
    }

    Element messages_box = vbox({
        text("Chat privado con " + chatting_with) | bold | color(Color::Green),
        separator(),
        vbox(std::move(message_elements)) | color(Color::Green) | flex
    }) |
    color(Color::Green) |
    border |
    yflex;

    Element input_box = vbox({
        text("Enviar comando") | bold | color(Color::Yellow),
        separator(),
        command_input_element | color(Color::Yellow)
    }) |
    color(Color::Yellow) |
    border |
    size(HEIGHT, EQUAL, 6);

    return vbox({
        messages_box,
        input_box
    }) |
    yflex;
}