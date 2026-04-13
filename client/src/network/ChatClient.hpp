#ifndef CHAT_CLIENT_HPP
#define CHAT_CLIENT_HPP

#include <mutex>
#include <string>

class ChatClient {
public:
    ChatClient(const std::string& server_ip, int server_port);
    ~ChatClient();

    bool connectToServer();
    void disconnect();
    bool isConnected() const;
    void enableReceiveTimeout(int seconds);

    bool sendMessage(const std::string& request_message);
    std::string receiveMessage();

    // Métodos síncronos actuales: envían la solicitud y esperan la respuesta.
    std::string registerUser(const std::string& username);
    std::string getAllMessages(const std::string& username);
    std::string sendPublicMessage(const std::string& username, const std::string& content);
    std::string getUsers(const std::string& username);
    std::string updateStatus(const std::string& username, const std::string& status);

    // Métodos base para la siguiente fase: envían la solicitud pero dejan
    // la recepción de la respuesta al hilo listener del cliente.
    bool sendRegisterRequest(const std::string& username);
    bool sendGetAllRequest(const std::string& username);
    bool sendPublicMessageRequest(const std::string& username, const std::string& content);
    bool sendGetUsersRequest(const std::string& username);
    bool sendStatusRequest(const std::string& username, const std::string& status);
    bool sendPrivateMessageRequest(const std::string& username,
                                   const std::string& target,
                                   const std::string& content);
    bool sendGetPrivateRequest(const std::string& username, const std::string& target);
    bool sendInfoRequest(const std::string& username, const std::string& target);
    bool sendExitRequest(const std::string& username);

private:
    bool ensureConnectedAndSend(const std::string& request_message);

    std::string server_ip;
    int server_port;
    int client_socket_file_descriptor;
    bool connected;
    std::mutex send_mutex;
};

#endif