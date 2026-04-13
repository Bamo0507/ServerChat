#include "ChatClient.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <mutex>
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

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
        close(client_socket_file_descriptor);
        client_socket_file_descriptor = -1;
        return false;
    }

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

bool ChatClient::sendExitRequest(const std::string& username) {
    return ensureConnectedAndSend("EXIT|" + username);
}