#ifndef CHAT_CLIENT_HPP
#define CHAT_CLIENT_HPP

#include <string>

class ChatClient {
public:
    // Constructor con parámetros para IP y puerto del servidor
    ChatClient(const std::string& server_ip, int server_port);

    // Intenta registrarse en el servidor
    // Devuelve la respuesta del servidor como string
    std::string registerUser(const std::string& username);

private:
    std::string server_ip;
    int server_port;
};

#endif