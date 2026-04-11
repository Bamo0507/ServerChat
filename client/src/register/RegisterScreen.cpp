#include "RegisterScreen.hpp"

#include <ftxui/dom/elements.hpp>

using namespace ftxui;

Element RegisterScreen(
    const std::string& username_input,
    const std::string& server_ip_input,
    const std::string& server_port_input,
    Element active_input_element,
    const std::string& helper_message
) {
    Element title_box = vbox({
        filler(),
        text("Registro de cliente") | bold | hcenter,
        text("Ingrese sus datos para conectarse al servidor") | hcenter,
        filler(),
    }) |
    border |
    size(HEIGHT, EQUAL, 5);

    Element username_box = vbox({
        text("Username") | bold,
        separator(),
        text(username_input.empty() ? " " : username_input)
    }) |
    border |
    size(HEIGHT, EQUAL, 4);

    Element server_ip_box = vbox({
        text("IP del servidor") | bold,
        separator(),
        text(server_ip_input.empty() ? " " : server_ip_input)
    }) |
    border |
    size(HEIGHT, EQUAL, 4);

    Element server_port_box = vbox({
        text("Puerto") | bold,
        separator(),
        text(server_port_input.empty() ? " " : server_port_input)
    }) |
    border |
    size(HEIGHT, EQUAL, 4);

    Element active_input_box = vbox({
        text("Campo activo") | bold,
        separator(),
        active_input_element
    }) |
    border |
    size(HEIGHT, EQUAL, 4);

    Element footer_box = vbox({
        filler(),
        text(helper_message) | hcenter,
        filler(),
    }) |
    border |
    size(HEIGHT, EQUAL, 4);

    return vbox({
        title_box,
        username_box,
        server_ip_box,
        server_port_box,
        active_input_box,
        footer_box
    }) | yflex;
}