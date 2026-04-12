#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "generalChat/GeneralChat.hpp"
#include "network/ChatClient.hpp"
#include "register/RegisterScreen.hpp"
#include "ScreenView.hpp"
#include "help/HelpScreen.hpp"
#include "privateChat/PrivateChat.hpp"

using namespace ftxui;

enum class RegisterField {
    Username,
    ServerIp,
    ServerPort
};

int main() {
    std::vector<UserItem> users = {
        {"Bryan", "Activo", false},
        {"Adriana", "Ocupado", true},
        {"Brandon", "Inactivo", false}
    };

    std::vector<std::string> server_messages = {
        "/SERVER Bienvenido al chat"
    };

    std::vector<std::string> private_messages = {
        "/Adriana Hola Bryan",
        "/Bryan Hola Adriana, ¿cómo estás?",
        "/Adriana Todo bien"
    };

    std::string private_chat_user = "Adriana";

    bool is_registering = true;
    RegisterField active_register_field = RegisterField::Username;
    std::string username_input;
    std::string server_ip_input;
    std::string server_port_input;
    std::string register_helper_message =
        "Escriba sus datos. Enter avanza entre campos y registra en el último.";

    std::string current_username;
    std::string current_server_ip;
    int current_server_port = 0;

    // Durante el registro reutilizamos command_input como buffer del campo activo,
    // igual que en el chat. Así dejamos que FTXUI renderice y capture el texto.
    std::string command_input;

    ScreenView current_view = ScreenView::GeneralChat;
    HelpOrigin help_origin = HelpOrigin::General;

    auto screen = ScreenInteractive::TerminalOutput();

    auto send_server_request = [](const std::string& server_ip,
                                  int server_port,
                                  const std::string& request_message) -> std::string {
        int client_socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

        if (client_socket_file_descriptor < 0) {
            return "ERROR|CLIENT|SOCKET_CREATION_FAILED";
        }

        sockaddr_in server_address{};
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(server_port);

        if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
            close(client_socket_file_descriptor);
            return "ERROR|CLIENT|INVALID_SERVER_IP";
        }

        if (connect(
                client_socket_file_descriptor,
                reinterpret_cast<sockaddr*>(&server_address),
                sizeof(server_address)
            ) < 0) {
            close(client_socket_file_descriptor);
            return "ERROR|CLIENT|CONNECTION_FAILED";
        }

        ssize_t sent_byte_count = send(
            client_socket_file_descriptor,
            request_message.c_str(),
            request_message.size(),
            0
        );

        if (sent_byte_count < 0) {
            close(client_socket_file_descriptor);
            return "ERROR|CLIENT|SEND_FAILED";
        }

        char received_data_buffer[4096];
        std::memset(received_data_buffer, 0, sizeof(received_data_buffer));

        ssize_t received_byte_count = recv(
            client_socket_file_descriptor,
            received_data_buffer,
            sizeof(received_data_buffer) - 1,
            0
        );

        if (received_byte_count < 0) {
            close(client_socket_file_descriptor);
            return "ERROR|CLIENT|RECEIVE_FAILED";
        }

        std::string server_response(received_data_buffer, received_byte_count);
        close(client_socket_file_descriptor);
        return server_response;
    };

    auto refresh_public_messages = [&]() {
        if (current_username.empty() || current_server_ip.empty() || current_server_port <= 0) {
            return;
        }

        std::string server_response = send_server_request(
            current_server_ip,
            current_server_port,
            "GETALL|" + current_username
        );

        if (server_response.rfind("ERROR|", 0) == 0) {
            server_messages = {
                "/SERVER No se pudieron refrescar los mensajes: " + server_response
            };
            return;
        }

        std::vector<std::string> refreshed_messages;
        std::istringstream response_stream(server_response);
        std::string response_line;

        while (std::getline(response_stream, response_line)) {
            if (response_line.empty()) {
                continue;
            }

            const std::string public_message_prefix = "PUBLIC_MSG|";
            if (response_line.rfind(public_message_prefix, 0) != 0) {
                continue;
            }

            std::size_t sender_separator_index = response_line.find('|', public_message_prefix.size());
            if (sender_separator_index == std::string::npos) {
                continue;
            }

            std::string sender = response_line.substr(
                public_message_prefix.size(),
                sender_separator_index - public_message_prefix.size()
            );
            std::string content = response_line.substr(sender_separator_index + 1);
            refreshed_messages.push_back("/" + sender + " " + content);
        }

        if (refreshed_messages.empty()) {
            refreshed_messages.push_back("/SERVER No hay mensajes públicos todavía");
        }

        server_messages = refreshed_messages;
    };

    auto send_public_chat_message = [&](const std::string& message_content) {
        if (current_username.empty() || current_server_ip.empty() || current_server_port <= 0) {
            server_messages.push_back("/SERVER No hay una sesión activa para enviar mensajes.");
            return;
        }

        if (message_content.empty()) {
            server_messages.push_back("/SERVER No se puede enviar un mensaje vacío.");
            return;
        }

        std::string server_response = send_server_request(
            current_server_ip,
            current_server_port,
            "CHAT|" + current_username + "|" + message_content
        );

        if (server_response.rfind("ERROR|", 0) == 0) {
            server_messages.push_back(
                "/SERVER No se pudo enviar el mensaje: " + server_response
            );
            return;
        }

        refresh_public_messages();
    };

    Component input = Input(&command_input, "");

    Component app = Renderer(input, [&] {
        if (is_registering) {
            Element username_input_element;
            Element server_ip_input_element;
            Element server_port_input_element;

            if (active_register_field == RegisterField::Username) {
                username_input_element = hbox({
                    text("> "),
                    input->Render() | flex,
                });
            } else {
                username_input_element = text(
                    username_input.empty() ? "> " : "> " + username_input
                );
            }

            if (active_register_field == RegisterField::ServerIp) {
                server_ip_input_element = hbox({
                    text("> "),
                    input->Render() | flex,
                });
            } else {
                server_ip_input_element = text(
                    server_ip_input.empty() ? "> " : "> " + server_ip_input
                );
            }

            if (active_register_field == RegisterField::ServerPort) {
                server_port_input_element = hbox({
                    text("> "),
                    input->Render() | flex,
                });
            } else {
                server_port_input_element = text(
                    server_port_input.empty() ? "> " : "> " + server_port_input
                );
            }

            return RegisterScreen(
                username_input_element,
                server_ip_input_element,
                server_port_input_element,
                register_helper_message
            );
        }

        if (current_view == ScreenView::Help) {
            return HelpScreen(help_origin);
        }

        Element command_line = hbox({
            text("> "),
            input->Render() | flex,
        });

        if (current_view == ScreenView::PrivateChat) {
            return PrivateChat(private_chat_user, private_messages, command_line);
        }

        return GeneralChat(users, server_messages, command_line);
    });

    app = CatchEvent(app, [&](Event event) {
        if (is_registering) {
            auto save_active_register_field = [&]() {
                if (active_register_field == RegisterField::Username) {
                    username_input = command_input;
                } else if (active_register_field == RegisterField::ServerIp) {
                    server_ip_input = command_input;
                } else {
                    server_port_input = command_input;
                }
            };

            auto load_active_register_field = [&]() {
                if (active_register_field == RegisterField::Username) {
                    command_input = username_input;
                } else if (active_register_field == RegisterField::ServerIp) {
                    command_input = server_ip_input;
                } else {
                    command_input = server_port_input;
                }
            };

            if (event == Event::Backspace && command_input.empty()) {
                if (active_register_field == RegisterField::ServerIp) {
                    active_register_field = RegisterField::Username;
                    load_active_register_field();
                    return true;
                }

                if (active_register_field == RegisterField::ServerPort) {
                    active_register_field = RegisterField::ServerIp;
                    load_active_register_field();
                    return true;
                }
            }

            if (event == Event::Tab) {
                save_active_register_field();

                if (active_register_field == RegisterField::Username) {
                    active_register_field = RegisterField::ServerIp;
                } else if (active_register_field == RegisterField::ServerIp) {
                    active_register_field = RegisterField::ServerPort;
                } else {
                    active_register_field = RegisterField::Username;
                }

                load_active_register_field();
                return true;
            }

            if (event == Event::Return) {
                save_active_register_field();

                if (active_register_field == RegisterField::Username) {
                    active_register_field = RegisterField::ServerIp;
                    load_active_register_field();
                    return true;
                }

                if (active_register_field == RegisterField::ServerIp) {
                    active_register_field = RegisterField::ServerPort;
                    load_active_register_field();
                    return true;
                }

                if (username_input.empty() || server_ip_input.empty() || server_port_input.empty()) {
                    register_helper_message = "Complete username, IP y puerto antes de registrarse.";
                    return true;
                }

                int parsed_server_port = std::atoi(server_port_input.c_str());
                if (parsed_server_port <= 0) {
                    register_helper_message = "El puerto ingresado no es válido.";
                    return true;
                }

                ChatClient register_client(server_ip_input, parsed_server_port);
                std::string register_response = register_client.registerUser(username_input);
                register_helper_message = register_response;

                if (register_response.rfind("OK|REGISTER", 0) == 0) {
                    current_username = username_input;
                    current_server_ip = server_ip_input;
                    current_server_port = parsed_server_port;

                    is_registering = false;
                    command_input.clear();
                    server_messages.push_back("/SERVER Resultado registro: " + register_response);
                }
                return true;
            }
        }

        if (event == Event::Return) {
            if (current_view == ScreenView::Help) {
                if (help_origin == HelpOrigin::General) {
                    current_view = ScreenView::GeneralChat;
                } else {
                    current_view = ScreenView::PrivateChat;
                }
                return true;
            }

            if (current_view == ScreenView::GeneralChat && command_input == "/help") {
                help_origin = HelpOrigin::General;
                current_view = ScreenView::Help;
                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::PrivateChat && command_input == "/help") {
                help_origin = HelpOrigin::Private;
                current_view = ScreenView::Help;
                command_input.clear();
                return true;
            }

            // TODO: Manejar logica con /private <nombreUsuario> esto solo es para ver UI de momento
            if (current_view == ScreenView::GeneralChat && command_input == "/private") {
                current_view = ScreenView::PrivateChat;
                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::PrivateChat && command_input == "/return") {
                current_view = ScreenView::GeneralChat;
                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::GeneralChat && command_input.rfind("/chat ", 0) == 0) {
                std::string message_content = command_input.substr(6);
                send_public_chat_message(message_content);
                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::GeneralChat && command_input == "/refresh") {
                refresh_public_messages();
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