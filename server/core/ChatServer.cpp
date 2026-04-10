#include "ChatServer.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "../models/Message.hpp"
#include "../protocol/MessageParser.hpp"
#include "../protocol/MessageSerializer.hpp"

ChatServer::ChatServer(int port)
    : server_port(port), server_socket_file_descriptor(-1) {
}

bool ChatServer::start() {
    std::cout << "Inicializando servidor en el puerto " << server_port << std::endl;

    server_socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket_file_descriptor < 0) {
        std::cerr << "Error: no se pudo crear el socket del servidor." << std::endl;
        return false;
    }

    std::cout << "Socket del servidor creado correctamente." << std::endl;

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    if (bind(
            server_socket_file_descriptor,
            reinterpret_cast<sockaddr*>(&server_address),
            sizeof(server_address)
        ) < 0) {
        std::cerr << "Error: no se pudo hacer bind al puerto " << server_port << std::endl;
        return false;
    }

    std::cout << "Bind realizado correctamente en el puerto " << server_port << std::endl;

    if (listen(server_socket_file_descriptor, 5) < 0) {
        std::cerr << "Error: no se pudo dejar el servidor en modo escucha." << std::endl;
        return false;
    }

    std::cout << "Servidor en modo escucha correctamente." << std::endl;

    return true;
}

void ChatServer::run() {
    std::cout << "Servidor listo para aceptar conexiones..." << std::endl;

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_address_length = sizeof(client_address);

        int client_socket_file_descriptor = accept(
            server_socket_file_descriptor,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_address_length
        );

        if (client_socket_file_descriptor < 0) {
            std::cerr << "Error: no se pudo aceptar la conexión del cliente." << std::endl;
            continue;
        }

        std::cout << "Cliente conectado correctamente." << std::endl;

        char receive_buffer[1024];
        std::memset(receive_buffer, 0, sizeof(receive_buffer));

        ssize_t received_bytes = recv(
            client_socket_file_descriptor,
            receive_buffer,
            sizeof(receive_buffer) - 1,
            0
        );

        if (received_bytes > 0) {
            std::string received_message(receive_buffer, received_bytes);
            std::cout << "Mensaje recibido del cliente: " << received_message << std::endl;

            Message parsed_message = MessageParser::parse(received_message);
            std::string server_response;

            switch (parsed_message.type) {
                case MessageType::Register:
                    server_response = MessageSerializer::buildOkResponse("REGISTER") + "\n";
                    break;

                case MessageType::ChatPublic:
                    server_response = MessageSerializer::buildPublicMessage(
                        parsed_message.sender,
                        parsed_message.content
                    ) + "\n";
                    break;

                case MessageType::Exit:
                    server_response = MessageSerializer::buildOkResponse("EXIT") + "\n";
                    break;

                // TODO: Agregar manejo explícito para otros tipos de mensaje
                // conforme se implementen más interacciones del protocolo.
                // Casos previstos para esta sección:
                // - MessageType::ChatPrivate
                // - MessageType::Status
                // - MessageType::Info
                // - MessageType::GetAll
                // La idea es que cada caso construya su respuesta utilizando
                // MessageSerializer y mantenga la lógica del servidor organizada.
                case MessageType::Unknown:
                default:
                    server_response = MessageSerializer::buildErrorResponse(
                        "UNKNOWN",
                        "INVALID_MESSAGE_FORMAT"
                    ) + "\n";
                    break;
            }

            ssize_t sent_bytes = send(
                client_socket_file_descriptor,
                server_response.c_str(),
                server_response.size(),
                0
            );

            if (sent_bytes < 0) {
                std::cerr << "Error: no se pudo enviar la respuesta al cliente." << std::endl;
            }

            std::cout << "Respuesta enviada al cliente: " << server_response << std::endl;
        } else {
            std::cerr << "Error o conexión cerrada sin datos." << std::endl;
        }

        close(client_socket_file_descriptor);
    }
}

void ChatServer::stop() {
    if (server_socket_file_descriptor >= 0) {
        close(server_socket_file_descriptor);
        server_socket_file_descriptor = -1;
    }
}