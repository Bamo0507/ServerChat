#include "RegisterScreen.hpp"

#include <ftxui/dom/elements.hpp>

using namespace ftxui;

Element RegisterScreen(
    Element username_input_element,
    Element server_ip_input_element,
    Element server_port_input_element,
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
        username_input_element
    }) |
    border |
    size(HEIGHT, EQUAL, 5);

    Element server_ip_box = vbox({
        text("IP del servidor") | bold,
        separator(),
        server_ip_input_element
    }) |
    border |
    size(HEIGHT, EQUAL, 5);

    Element server_port_box = vbox({
        text("Puerto") | bold,
        separator(),
        server_port_input_element
    }) |
    border |
    size(HEIGHT, EQUAL, 5);

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
        footer_box
    }) | yflex;
}