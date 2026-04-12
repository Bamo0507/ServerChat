#ifndef CHAT_CLIENT_HPP
#define CHAT_CLIENT_HPP

#include <string>

class ChatClient {
public:
    ChatClient(const std::string& server_ip, int server_port);

    std::string registerUser(const std::string& username);

    // Método base reutilizable para cualquier request simple al servidor
    std::string sendRequest(const std::string& request_message);

    // Helpers para comandos concretos
    std::string getAllMessages(const std::string& username);
    std::string sendPublicMessage(const std::string& username, const std::string& content);
    std::string getUsers(const std::string& username);

private:
    std::string server_ip;
    int server_port;
};

#endif