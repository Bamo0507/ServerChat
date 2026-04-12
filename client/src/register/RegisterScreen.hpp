#ifndef REGISTER_SCREEN_HPP
#define REGISTER_SCREEN_HPP

#include <string>
#include <ftxui/dom/elements.hpp>

ftxui::Element RegisterScreen(
    ftxui::Element username_input_element,
    ftxui::Element server_ip_input_element,
    ftxui::Element server_port_input_element,
    const std::string& helper_message
);

#endif