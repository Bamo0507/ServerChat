#ifndef REGISTER_SCREEN_HPP
#define REGISTER_SCREEN_HPP

#include <string>
#include <ftxui/dom/elements.hpp>

ftxui::Element RegisterScreen(
    const std::string& username_input,
    const std::string& server_ip_input,
    const std::string& server_port_input,
    ftxui::Element active_input_element,
    const std::string& helper_message
);

#endif