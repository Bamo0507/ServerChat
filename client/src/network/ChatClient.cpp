#include "ChatClient.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ChatClient::ChatClient(const std::string& server_ip, int server_port)
    : server_ip(server_ip), server_port(server_port) {
}

std::string ChatClient::registerUser(const std::string& username) {
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

    std::string register_message = "REGISTER|" + username;

    ssize_t sent_byte_count = send(
        client_socket_file_descriptor,
        register_message.c_str(),
        register_message.size(),
        0
    );

    if (sent_byte_count < 0) {
        close(client_socket_file_descriptor);
        return "ERROR|CLIENT|SEND_FAILED";
    }

    char received_data_buffer[1024];
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
}
#include "ChatClient.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ChatClient::ChatClient(const std::string& server_ip, int server_port)
    : server_ip(server_ip), server_port(server_port) {
}

std::string ChatClient::sendRequest(const std::string& request_message) {
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
}

std::string ChatClient::registerUser(const std::string& username) {
    return sendRequest("REGISTER|" + username);
}

std::string ChatClient::getAllMessages(const std::string& username) {
    return sendRequest("GETALL|" + username);
}

std::string ChatClient::sendPublicMessage(
    const std::string& username,
    const std::string& content
) {
    return sendRequest("CHAT|" + username + "|" + content);
}

std::string ChatClient::getUsers(const std::string& username) {
    return sendRequest("GETUSERS|" + username);
}