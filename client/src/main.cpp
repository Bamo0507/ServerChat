#include <atomic>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
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
        {"Sin usuarios conectados", "-", false}
    };

    std::vector<std::string> server_messages = {
        "/SERVER Bienvenido al chat"
    };

    std::vector<std::string> private_messages;
    std::string private_chat_user;

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
    std::unique_ptr<ChatClient> active_chat_client;

    std::mutex shared_state_mutex;
    std::atomic<bool> keep_listening = false;
    std::thread server_listener_thread;
    std::atomic<bool> awaiting_public_messages_refresh = false;
    std::atomic<bool> awaiting_users_refresh = false;
    std::atomic<bool> awaiting_private_refresh = false;
    std::string last_requested_display_status;

    std::string command_input;

    // Indica que hay un intento de registro en curso en un hilo de fondo.
    // Mientras sea true, el evento de Enter en el formulario se ignora.
    std::atomic<bool> registration_in_progress = false;

    ScreenView current_view = ScreenView::GeneralChat;
    HelpOrigin help_origin = HelpOrigin::General;

    auto screen = ScreenInteractive::TerminalOutput();

    auto request_server_response = [&](const std::function<bool()>& send_request_action) -> std::string {
        if (!active_chat_client) {
            return "ERROR|CLIENT|NOT_CONNECTED";
        }

        if (!send_request_action()) {
            return "ERROR|CLIENT|SEND_FAILED";
        }

        return active_chat_client->receiveMessage();
    };

    auto map_server_status_to_display_status = [](const std::string& server_status) -> std::string {
        if (server_status == "online") {
            return "ACTIVO";
        }

        if (server_status == "away") {
            return "OCUPADO";
        }

        if (server_status == "offline") {
            return "INACTIVO";
        }

        return server_status;
    };

    auto map_display_status_to_server_status = [](const std::string& display_status) -> std::string {
        if (display_status == "ACTIVO") {
            return "online";
        }

        if (display_status == "OCUPADO") {
            return "away";
        }

        if (display_status == "INACTIVO") {
            return "offline";
        }

        return "";
    };

    auto refresh_public_messages = [&]() {
        if (current_username.empty() || !active_chat_client) {
            return;
        }

        awaiting_public_messages_refresh = true;

        if (!active_chat_client->sendGetAllRequest(current_username)) {
            awaiting_public_messages_refresh = false;
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back("/SERVER No se pudieron refrescar los mensajes.");
            return;
        }
    };

    auto refresh_connected_users = [&]() {
        if (current_username.empty() || !active_chat_client) {
            return;
        }

        awaiting_users_refresh = true;

        if (!active_chat_client->sendGetUsersRequest(current_username)) {
            awaiting_users_refresh = false;
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            users = {
                {"Error cargando usuarios", "-", false}
            };
            return;
        }
    };

    auto refresh_initial_data = [&]() {
        refresh_public_messages();
        refresh_connected_users();
    };

    auto handle_server_response = [&](const std::string& server_response) {
        if (server_response.rfind("ERROR|CLIENT|", 0) == 0) {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back("/SERVER " + server_response);
            return;
        }

        bool response_is_empty = true;
        for (char current_character : server_response) {
            if (current_character != ' ' &&
                current_character != '\n' &&
                current_character != '\r' &&
                current_character != '\t') {
                response_is_empty = false;
                break;
            }
        }

        if (response_is_empty) {
            return;
        }

        std::vector<std::string> refreshed_public_messages;
        std::vector<UserItem> refreshed_users;
        std::vector<std::string> refreshed_private_messages;
        std::vector<std::string> server_notifications;
        bool contains_public_messages = false;
        bool contains_user_info = false;
        bool contains_private_messages = false;
        bool should_refresh_users_after_status_update = false;

        std::istringstream response_stream(server_response);
        std::string response_line;

        while (std::getline(response_stream, response_line)) {
            if (response_line.empty()) {
                continue;
            }

            const std::string public_message_prefix = "PUBLIC_MSG|";
            const std::string user_info_prefix = "USER_INFO|";
            const std::string server_warning_prefix = "SERVER_WARN|";
            const std::string ok_prefix = "OK|";
            const std::string error_prefix = "ERROR|";
            const std::string info_prefix = "INFO|";

            if (response_line.rfind(public_message_prefix, 0) == 0) {
                std::size_t sender_separator_index = response_line.find('|', public_message_prefix.size());
                if (sender_separator_index == std::string::npos) {
                    continue;
                }

                std::string sender = response_line.substr(
                    public_message_prefix.size(),
                    sender_separator_index - public_message_prefix.size()
                );
                std::string content = response_line.substr(sender_separator_index + 1);

                if (sender.empty()) {
                    sender = "SERVER";
                }

                if (content.empty()) {
                    content = "(sin contenido)";
                }

                refreshed_public_messages.push_back("/" + sender + " " + content);
                contains_public_messages = true;
                continue;
            }

            if (response_line.rfind(user_info_prefix, 0) == 0) {
                std::vector<std::string> parts;
                std::stringstream line_stream(response_line);
                std::string current_part;

                while (std::getline(line_stream, current_part, '|')) {
                    parts.push_back(current_part);
                }

                if (parts.size() < 4) {
                    continue;
                }

                std::string username = parts[1];
                std::string status = map_server_status_to_display_status(parts[2]);
                bool has_private_chat = (parts[3] == "true" || parts[3] == "1");

                if (username.empty()) {
                    username = "Usuario";
                }

                if (status.empty()) {
                    status = "-";
                }

                refreshed_users.push_back({username, status, has_private_chat});
                contains_user_info = true;
                continue;
            }

            if (response_line.rfind(server_warning_prefix, 0) == 0) {
                server_notifications.push_back(
                    "/SERVER " + response_line.substr(server_warning_prefix.size())
                );
                continue;
            }

            const std::string private_msg_prefix = "PRIVATE_MSG|";
            if (response_line.rfind(private_msg_prefix, 0) == 0) {
                std::vector<std::string> parts;
                std::stringstream line_stream(response_line);
                std::string current_part;

                while (std::getline(line_stream, current_part, '|')) {
                    parts.push_back(current_part);
                }

                // PRIVATE_MSG|sender|target|content
                if (parts.size() >= 4) {
                    std::string sender  = parts[1];
                    std::string content = parts[3];

                    // Solo agregar si viene del otro usuario (el propio ya se añadió optimistamente)
                    if (sender != current_username) {
                        refreshed_private_messages.push_back("/" + sender + " " + content);
                        contains_private_messages = true;
                    }
                }
                continue;
            }

            if (response_line.rfind(ok_prefix, 0) == 0 ||
                response_line.rfind(error_prefix, 0) == 0 ||
                response_line.rfind(info_prefix, 0) == 0) {

                if (response_line == "OK|STATUS") {
                    std::string status_label = last_requested_display_status.empty()
                        ? "desconocido"
                        : last_requested_display_status;
                    server_notifications.push_back(
                        "/SERVER " + current_username + " cambió su estado a " + status_label
                    );
                    last_requested_display_status.clear();
                    should_refresh_users_after_status_update = true;
                } else {
                    server_notifications.push_back("/SERVER " + response_line);
                }
                continue;
            }
        }

        std::lock_guard<std::mutex> lock(shared_state_mutex);

        if (contains_public_messages) {
            if (awaiting_public_messages_refresh.exchange(false)) {
                if (refreshed_public_messages.empty()) {
                    server_messages = {
                        "/SERVER No hay mensajes públicos todavía"
                    };
                } else {
                    server_messages = refreshed_public_messages;
                }
            } else {
                for (const std::string& refreshed_message : refreshed_public_messages) {
                    server_messages.push_back(refreshed_message);
                }
            }
        }

        if (contains_user_info && awaiting_users_refresh.exchange(false)) {
            if (refreshed_users.empty()) {
                users = {
                    {"Sin usuarios conectados", "-", false}
                };
            } else {
                users = refreshed_users;
            }
        }

        if (contains_private_messages) {
            if (awaiting_private_refresh.exchange(false)) {
                // Respuesta a GETPRIVATE: reemplazar historial completo
                private_messages = refreshed_private_messages;
            } else {
                // Mensaje nuevo entrante: agregar al final
                for (const std::string& pm : refreshed_private_messages) {
                    private_messages.push_back(pm);
                }
            }
        }

        for (const std::string& notification_message : server_notifications) {
            server_messages.push_back(notification_message);
        }

        if (should_refresh_users_after_status_update) {
            refresh_connected_users();
        }
    };

    auto start_server_listener = [&]() {
        if (!active_chat_client || keep_listening) {
            return;
        }

        // Activar timeout de recv() solo ahora que el registro ya terminó.
        // El listener usará este timeout para hacer auto-refresh periódico.
        active_chat_client->enableReceiveTimeout(5);

        keep_listening = true;
        server_listener_thread = std::thread([&]() {
            while (keep_listening && active_chat_client && active_chat_client->isConnected()) {
                std::string server_response = active_chat_client->receiveMessage();

                if (!keep_listening) {
                    break;
                }

                // El recv() tiene un timeout de 3s. Cuando vence sin datos,
                // aprovechamos para pedir el estado actualizado del servidor.
                if (server_response == "TIMEOUT") {
                    refresh_public_messages();
                    refresh_connected_users();
                    continue;
                }

                if (server_response == "ERROR|CLIENT|CONNECTION_CLOSED") {
                    std::lock_guard<std::mutex> lock(shared_state_mutex);
                    server_messages.push_back("/SERVER Conexión cerrada con el servidor.");
                    break;
                }

                handle_server_response(server_response);
                screen.PostEvent(Event::Custom);
            }

            keep_listening = false;
        });
    };

    auto stop_server_listener = [&]() {
        keep_listening = false;

        if (active_chat_client) {
            active_chat_client->disconnect();
        }

        if (server_listener_thread.joinable()) {
            server_listener_thread.join();
        }
    };

    auto send_public_chat_message = [&](const std::string& message_content) {
        if (current_username.empty() || !active_chat_client) {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back("/SERVER No hay una sesión activa para enviar mensajes.");
            return;
        }

        std::string trimmed_message_content = message_content;

        while (!trimmed_message_content.empty() && trimmed_message_content.front() == ' ') {
            trimmed_message_content.erase(trimmed_message_content.begin());
        }

        while (!trimmed_message_content.empty() && trimmed_message_content.back() == ' ') {
            trimmed_message_content.pop_back();
        }

        if (trimmed_message_content.empty()) {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back("/SERVER No se puede enviar un mensaje vacío.");
            return;
        }

        if (!active_chat_client->sendPublicMessageRequest(current_username, trimmed_message_content)) {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back("/SERVER No se pudo enviar el mensaje.");
            return;
        }
    };

    auto send_status_update = [&](const std::string& display_status) {
        if (current_username.empty() || !active_chat_client) {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back("/SERVER No hay una sesión activa para actualizar el estado.");
            return;
        }

        std::string server_status = map_display_status_to_server_status(display_status);

        if (server_status.empty()) {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back(
                "/SERVER Estado inválido. Use /status ACTIVO, /status OCUPADO o /status INACTIVO."
            );
            return;
        }

        last_requested_display_status = display_status;

        if (!active_chat_client->sendStatusRequest(current_username, server_status)) {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            server_messages.push_back("/SERVER No se pudo enviar la actualización de estado.");
            last_requested_display_status.clear();
            return;
        }
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

        std::vector<UserItem> users_snapshot;
        std::vector<std::string> server_messages_snapshot;
        std::vector<std::string> private_messages_snapshot;

        {
            std::lock_guard<std::mutex> lock(shared_state_mutex);
            users_snapshot = users;
            server_messages_snapshot = server_messages;
            private_messages_snapshot = private_messages;
        }

        Element command_line = hbox({
            text("> "),
            input->Render() | flex,
        });

        if (current_view == ScreenView::PrivateChat) {
            return PrivateChat(private_chat_user, private_messages_snapshot, command_line);
        }

        return GeneralChat(users_snapshot, server_messages_snapshot, command_line);
    });

    app = CatchEvent(app, [&](Event event) {
        if (event == Event::Custom) {
            return true;
        }

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

                if (registration_in_progress) {
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

                // Mostrar feedback inmediato. No llamar PostEvent aquí —
                // FTXUI re-renderiza solo cuando CatchEvent retorna.
                register_helper_message = "Conectando...";
                registration_in_progress = true;

                // Capturar los valores que el hilo necesita (no capturas por ref de inputs locales).
                std::string reg_username  = username_input;
                std::string reg_server_ip = server_ip_input;
                int         reg_port      = parsed_server_port;

                // El hilo se lanza como detached: no necesita join y nunca bloquea el event loop.
                std::thread([&, reg_username, reg_server_ip, reg_port]() {
                    auto new_client = std::make_unique<ChatClient>(reg_server_ip, reg_port);

                    if (!new_client->connectToServer()) {
                        register_helper_message = "Error: no se pudo conectar al servidor.";
                        registration_in_progress = false;
                        screen.PostEvent(Event::Custom);
                        return;
                    }

                    if (!new_client->sendRegisterRequest(reg_username)) {
                        register_helper_message = "Error: no se pudo enviar el registro.";
                        registration_in_progress = false;
                        screen.PostEvent(Event::Custom);
                        return;
                    }

                    std::string register_response = new_client->receiveMessage();

                    if (register_response.rfind("OK|REGISTER", 0) == 0) {
                        active_chat_client = std::move(new_client);
                        current_username   = reg_username;
                        current_server_ip  = reg_server_ip;
                        current_server_port = reg_port;

                        is_registering = false;

                        {
                            std::lock_guard<std::mutex> lock(shared_state_mutex);
                            command_input.clear();
                        }

                        start_server_listener();
                        refresh_initial_data();
                    } else {
                        register_helper_message = register_response.empty()
                            ? "Error: sin respuesta del servidor."
                            : register_response;
                    }

                    registration_in_progress = false;
                    screen.PostEvent(Event::Custom);
                }).detach();

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

            if (current_view == ScreenView::GeneralChat &&
                command_input.rfind("/private ", 0) == 0) {
                std::string target = command_input.substr(9);

                while (!target.empty() && target.front() == ' ') {
                    target.erase(target.begin());
                }
                while (!target.empty() && target.back() == ' ') {
                    target.pop_back();
                }

                if (!target.empty() && active_chat_client) {
                    private_chat_user = target;
                    {
                        std::lock_guard<std::mutex> lock(shared_state_mutex);
                        private_messages.clear();
                    }
                    awaiting_private_refresh = true;
                    active_chat_client->sendGetPrivateRequest(current_username, target);
                    current_view = ScreenView::PrivateChat;
                }

                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::PrivateChat && command_input == "/return") {
                current_view = ScreenView::GeneralChat;
                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::PrivateChat &&
                command_input.rfind("/chat ", 0) == 0 &&
                !private_chat_user.empty() &&
                active_chat_client) {
                std::string content = command_input.substr(6);

                while (!content.empty() && content.front() == ' ') {
                    content.erase(content.begin());
                }
                while (!content.empty() && content.back() == ' ') {
                    content.pop_back();
                }

                if (!content.empty()) {
                    {
                        std::lock_guard<std::mutex> lock(shared_state_mutex);
                        private_messages.push_back("/" + current_username + " " + content);
                    }
                    active_chat_client->sendPrivateMessageRequest(current_username, private_chat_user, content);
                }

                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::GeneralChat && command_input.rfind("/chat ", 0) == 0) {
                std::string message_content = command_input.substr(6);
                send_public_chat_message(message_content);
                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::GeneralChat &&
                (command_input.rfind("/status ", 0) == 0 || command_input.rfind("status ", 0) == 0)) {
                std::string requested_status;

                if (command_input.rfind("/status ", 0) == 0) {
                    requested_status = command_input.substr(8);
                } else {
                    requested_status = command_input.substr(7);
                }

                while (!requested_status.empty() && requested_status.front() == ' ') {
                    requested_status.erase(requested_status.begin());
                }

                while (!requested_status.empty() && requested_status.back() == ' ') {
                    requested_status.pop_back();
                }

                send_status_update(requested_status);
                command_input.clear();
                return true;
            }

            if (current_view == ScreenView::GeneralChat && command_input == "/refresh") {
                refresh_initial_data();
                command_input.clear();
                return true;
            }

            if (command_input == "/exit") {
                if (active_chat_client) {
                    active_chat_client->sendExitRequest(current_username);
                }
                stop_server_listener();
                screen.ExitLoopClosure()();
                return true;
            }

            return true;
        }

        return false;
    });

    screen.Loop(app);
    stop_server_listener();

    return 0;
}