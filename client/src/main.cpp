#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "generalChat/GeneralChat.hpp"
#include "ScreenView.hpp"
#include "help/HelpScreen.hpp"

using namespace ftxui;

int main() {
    std::vector<UserItem> users = {
        {"Bryan", "Activo", false},
        {"Adriana", "Ocupado", true},
        {"Brandon", "Inactivo", false}
    };

    std::vector<std::string> server_messages = {
        "/SERVER Bienvenido al chat",
        "/Bryan Hola a todos",
        "/Adriana Estoy trabajando ahorita",
        "/SERVER Bienvenido al chat",
        "/Bryan Hola a todos",
        "/Adriana Estoy trabajando ahorita",
        "/SERVER Bienvenido al chat",
    };

    std::string command_input;
    ScreenView current_view = ScreenView::GeneralChat;
    HelpOrigin help_origin = HelpOrigin::General;

    auto screen = ScreenInteractive::TerminalOutput();

    Component input = Input(&command_input, "");

    Component app = Renderer(input, [&] {
        if (current_view == ScreenView::Help) {
            return HelpScreen(help_origin);
        }

        Element command_line = hbox({
            text("> "),
            input->Render() | flex,
        });

        return GeneralChat(users, server_messages, command_line);
    });

    app = CatchEvent(app, [&](Event event) {
        if (event == Event::Return) {
            if (current_view == ScreenView::Help) {
                current_view = ScreenView::GeneralChat;
                return true;
            }

            if (command_input == "/help") {
                help_origin = HelpOrigin::General;
                current_view = ScreenView::Help;
                command_input.clear();
                return true;
            }

            if (command_input == "/exit") {
                screen.ExitLoopClosure()();
                return true;
            }

            return true;
        }

        return false;
    });

    screen.Loop(app);
    return 0;
}