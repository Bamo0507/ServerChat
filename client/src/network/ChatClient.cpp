#include "ChatClient.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ChatClient::ChatClient(const std::string& server_ip, int server_port)
    : server_ip(server_ip),
      server_port(server_port),
      client_socket_file_descriptor(-1),
      connected(false) {
}

ChatClient::~ChatClient() {
    disconnect();
}

bool ChatClient::connectToServer() {
    if (connected) {
        return true;
    }

    client_socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket_file_descriptor < 0) {
        return false;
    }

    // getaddrinfo resuelve tanto IPs numéricas ("192.168.1.1") como hostnames
    // ("0.tcp.ngrok.io"), lo que permite conectar a través de túneles como ngrok.
    struct addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* resolved_address_list = nullptr;
    std::string port_str = std::to_string(server_port);

    int resolution_result = getaddrinfo(
        server_ip.c_str(), port_str.c_str(), &hints, &resolved_address_list
    );

    if (resolution_result != 0 || resolved_address_list == nullptr) {
        close(client_socket_file_descriptor);
        client_socket_file_descriptor = -1;
        return false;
    }

    sockaddr_in server_address{};
    std::memcpy(&server_address, resolved_address_list->ai_addr, sizeof(server_address));
    freeaddrinfo(resolved_address_list);

    if (connect(
            client_socket_file_descriptor,
            reinterpret_cast<sockaddr*>(&server_address),
            sizeof(server_address)
        ) < 0) {
        close(client_socket_file_descriptor);
        client_socket_file_descriptor = -1;
        return false;
    }

    connected = true;
    return true;
}

void ChatClient::enableReceiveTimeout(int seconds) {
    if (client_socket_file_descriptor < 0) {
        return;
    }

    struct timeval recv_timeout;
    recv_timeout.tv_sec = seconds;
    recv_timeout.tv_usec = 0;
    setsockopt(
        client_socket_file_descriptor,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &recv_timeout,
        sizeof(recv_timeout)
    );
}

void ChatClient::disconnect() {
    if (client_socket_file_descriptor >= 0) {
        close(client_socket_file_descriptor);
        client_socket_file_descriptor = -1;
    }

    connected = false;
}

bool ChatClient::isConnected() const {
    return connected;
}

bool ChatClient::sendMessage(const std::string& request_message) {
    if (!connected) {
        return false;
    }

    // El \n al final actúa como delimitador de mensaje para que el servidor
    // pueda separar mensajes consecutivos que llegan en el mismo recv().
    std::string framed_message = request_message + "\n";

    std::lock_guard<std::mutex> lock(send_mutex);
    ssize_t sent_byte_count = send(
        client_socket_file_descriptor,
        framed_message.c_str(),
        framed_message.size(),
        0
    );

    return sent_byte_count >= 0;
}

std::string ChatClient::receiveMessage() {
    if (!connected) {
        return "ERROR|CLIENT|NOT_CONNECTED";
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
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return "TIMEOUT";
        }
        return "ERROR|CLIENT|RECEIVE_FAILED";
    }

    if (received_byte_count == 0) {
        disconnect();
        return "ERROR|CLIENT|CONNECTION_CLOSED";
    }

    return std::string(received_data_buffer, received_byte_count);
}

bool ChatClient::ensureConnectedAndSend(const std::string& request_message) {
    if (!connectToServer()) {
        return false;
    }

    return sendMessage(request_message);
}

std::string ChatClient::registerUser(const std::string& username) {
    if (!sendRegisterRequest(username)) {
        return "ERROR|CLIENT|SEND_FAILED";
    }

    return receiveMessage();
}

std::string ChatClient::getAllMessages(const std::string& username) {
    if (!sendGetAllRequest(username)) {
        return "ERROR|CLIENT|SEND_FAILED";
    }

    return receiveMessage();
}

std::string ChatClient::sendPublicMessage(
    const std::string& username,
    const std::string& content
) {
    if (!sendPublicMessageRequest(username, content)) {
        return "ERROR|CLIENT|SEND_FAILED";
    }

    return receiveMessage();
}

std::string ChatClient::getUsers(const std::string& username) {
    if (!sendGetUsersRequest(username)) {
        return "ERROR|CLIENT|SEND_FAILED";
    }

    return receiveMessage();
}

std::string ChatClient::updateStatus(
    const std::string& username,
    const std::string& status
) {
    if (!sendStatusRequest(username, status)) {
        return "ERROR|CLIENT|SEND_FAILED";
    }

    return receiveMessage();
}

bool ChatClient::sendRegisterRequest(const std::string& username) {
    return ensureConnectedAndSend("REGISTER|" + username);
}

bool ChatClient::sendGetAllRequest(const std::string& username) {
    return ensureConnectedAndSend("GETALL|" + username);
}

bool ChatClient::sendPublicMessageRequest(
    const std::string& username,
    const std::string& content
) {
    return ensureConnectedAndSend("CHAT|" + username + "|" + content);
}

bool ChatClient::sendGetUsersRequest(const std::string& username) {
    return ensureConnectedAndSend("GETUSERS|" + username);
}

bool ChatClient::sendStatusRequest(
    const std::string& username,
    const std::string& status
) {
    return ensureConnectedAndSend("STATUS|" + username + "|" + status);
}

bool ChatClient::sendPrivateMessageRequest(
    const std::string& username,
    const std::string& target,
    const std::string& content
) {
    return ensureConnectedAndSend("PRIVATE|" + username + "|" + target + "|" + content);
}

bool ChatClient::sendGetPrivateRequest(
    const std::string& username,
    const std::string& target
) {
    return ensureConnectedAndSend("GETPRIVATE|" + username + "|" + target);
}

bool ChatClient::sendInfoRequest(const std::string& username, const std::string& target) {
    return ensureConnectedAndSend("INFO|" + username + "|" + target);
}

bool ChatClient::sendExitRequest(const std::string& username) {
    return ensureConnectedAndSend("EXIT|" + username);
}