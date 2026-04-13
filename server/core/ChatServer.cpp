#include "ChatServer.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "../models/ClientSession.hpp"
#include "../models/Message.hpp"
#include "../models/Private.hpp"
#include "../protocol/MessageParser.hpp"
#include "../protocol/MessageSerializer.hpp"

#define MAX_PUBLIC_MESSAGES 50
#define MAX_CLIENTS 5
#define MAX_PRIVATE_CONVERSATIONS (MAX_CLIENTS * (MAX_CLIENTS - 1) / 2)

ChatServer::ChatServer(int port)
    : server_port(port), server_socket_file_descriptor(-1) {
}

Message public_chat_messages[MAX_PUBLIC_MESSAGES];
int public_chat_message_count = 0;

ClientSession connected_clients[MAX_CLIENTS];
int connected_client_count = 0;

PrivateConversation private_conversations[MAX_PRIVATE_CONVERSATIONS];
int private_conversation_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helpers para conversaciones privadas
static void buildCanonicalPair(
    const std::string& first_username,
    const std::string& second_username,
    std::string& canonical_first_username,
    std::string& canonical_second_username
) {
    if (first_username < second_username) {
        canonical_first_username = first_username;
        canonical_second_username = second_username;
    } else {
        canonical_first_username = second_username;
        canonical_second_username = first_username;
    }
}

// Busca una conversación privada existente entre dos usuarios y, si no existe,
// crea una nueva si todavía hay espacio disponible.
// Debe llamarse con clients_mutex ya tomado para evitar condiciones de carrera.
static PrivateConversation* findOrCreatePrivateConversation(
    const std::string& first_username,
    const std::string& second_username
) {
    std::string canonical_first_username;
    std::string canonical_second_username;
    buildCanonicalPair(
        first_username,
        second_username,
        canonical_first_username,
        canonical_second_username
    );

    for (int conversation_index = 0; conversation_index < private_conversation_count; conversation_index++) {
        if (private_conversations[conversation_index].active &&
            private_conversations[conversation_index].user_a == canonical_first_username &&
            private_conversations[conversation_index].user_b == canonical_second_username) {
            return &private_conversations[conversation_index];
        }
    }

    if (private_conversation_count >= MAX_PRIVATE_CONVERSATIONS) {
        return nullptr;
    }

    PrivateConversation& new_private_conversation =
        private_conversations[private_conversation_count++];

    new_private_conversation.user_a = canonical_first_username;
    new_private_conversation.user_b = canonical_second_username;
    new_private_conversation.message_count = 0;
    new_private_conversation.active = true;

    // pthread_mutex_init(&new_private_conversation.mutex, nullptr); // Se inicializa x defecto en el objeto su mutex
    return &new_private_conversation;
}

// Indica si dos usuarios ya comparten una conversación privada activa.
// Debe llamarse con clients_mutex ya tomado.
static bool userHasPrivateChatWithMe(
    const std::string& listed_username, // Usuario que se lista en la conversación
    const std::string& current_username // Usuario que está consultando
) {
    for (int conversation_index = 0; conversation_index < private_conversation_count; conversation_index++) {
        if (private_conversations[conversation_index].active &&
            (private_conversations[conversation_index].user_a == listed_username ||
             private_conversations[conversation_index].user_b == listed_username) &&
            (private_conversations[conversation_index].user_a == current_username ||
             private_conversations[conversation_index].user_b == current_username)) {
            return true;
        }
    }
    return false;
}

// Busca el socket de un usuario conectado a partir de su nombre.
// Debe llamarse con clients_mutex ya tomado.
static int findSocketByUsername(const std::string& username) {
    for (int client_index = 0; client_index < connected_client_count; client_index++) {
        if (connected_clients[client_index].username == username) {
            return connected_clients[client_index].socket_fd;
        }
    }
    return -1;
}

// Elimina saltos de línea y espacios del inicio y final de un texto recibido.
static void trimLineBreaksAndSpaces(std::string& text) {
    while (!text.empty() &&
           (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }

    while (!text.empty() &&
           (text.front() == '\n' || text.front() == '\r' || text.front() == ' ')) {
        text.erase(text.begin());
    }
}

// Estructura con la información necesaria para inicializar el hilo de un cliente.
struct ClientThreadArgs {
    int socket_fd; // identificador del socket
    sockaddr_in address; // IP, PUERTO, IPv4
};

bool ChatServer::start() {
    std::cout << "Inicializando servidor en el puerto "<< server_port << std::endl;

    // SOCK_STREAM -> Socket orientado a flujos
    // 0 -> Protocolo x defecto aka TCP
    server_socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0); 

    if (server_socket_file_descriptor < 0) {
        std::cerr << "Error: no se pudo crear el socket del servidor." << std::endl;
        return false;
    }

    std::cout << "Socket del servidor creado correctamente." << std::endl;

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY; // Acepta conecciones de todos lados
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

    // 5 -> tamaño máximo de la cola de conexiones pendientes que pueden esperar antes de ser aceptadas
    if (listen(server_socket_file_descriptor, 5) < 0) {
        std::cerr << "Error: no se pudo dejar el servidor en modo escucha." << std::endl;
        return false;
    }

    std::cout << "Servidor en modo escucha correctamente." << std::endl;

    return true;
}

static void* handleClient(void* raw_thread_arguments) {
    ClientThreadArgs* client_thread_arguments =
        static_cast<ClientThreadArgs*>(raw_thread_arguments);

    int client_socket_file_descriptor = client_thread_arguments->socket_fd;

    sockaddr_in client_address = client_thread_arguments->address;

    delete client_thread_arguments; // liberar memoria

    // Buffer acumulador: guarda bytes recibidos hasta completar una línea (\n).
    // Esto resuelve el problema de framing en TCP — varios mensajes del cliente
    // pueden llegar concatenados en un solo recv(), y este buffer permite
    // procesarlos uno a uno correctamente.
    std::string incoming_buffer;
    bool client_exited_cleanly = false;

    while (true) {
        char received_data_buffer[1024];
        std::memset(received_data_buffer, 0, sizeof(received_data_buffer));

        ssize_t received_byte_count = recv(
            client_socket_file_descriptor,
            received_data_buffer,
            sizeof(received_data_buffer) - 1,
            0
        );

        if (received_byte_count > 0) {
            incoming_buffer.append(received_data_buffer, received_byte_count);

            // Procesar todos los mensajes completos (terminados en \n) que haya en el buffer.
            std::size_t newline_position;
            while ((newline_position = incoming_buffer.find('\n')) != std::string::npos) {
                std::string received_raw_message = incoming_buffer.substr(0, newline_position);
                incoming_buffer.erase(0, newline_position + 1);

                if (received_raw_message.empty()) {
                    continue;
                }

                std::cout << "Mensaje recibido del cliente: " << received_raw_message << std::endl;

                Message parsed_client_message = MessageParser::parse(received_raw_message);
                std::string server_response_message;

                switch (parsed_client_message.type) {
                    case MessageType::Register:
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);

                        if (connected_client_count < MAX_CLIENTS) {
                            connected_clients[connected_client_count++] = {
                                .socket_fd = client_socket_file_descriptor,
                                .username = parsed_client_message.sender,
                                .ip_address = inet_ntoa(client_address.sin_addr),
                                .status = "online",
                                .thread_id = [] {
                                    std::ostringstream thread_id_stream;
                                    thread_id_stream << pthread_self();
                                    return thread_id_stream.str();
                                }()
                            };
                        } else {
                            server_response_message = MessageSerializer::buildErrorResponse(
                                "REGISTER",
                                "SERVER_FULL"
                            ) + "\n";
                            pthread_mutex_unlock(&clients_mutex);
                            break;
                        }

                        server_response_message = MessageSerializer::buildOkResponse("REGISTER") + "\n";
                        pthread_mutex_unlock(&clients_mutex);
                        break;

                    case MessageType::ChatPublic:
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);
                        trimLineBreaksAndSpaces(parsed_client_message.content);

                        if (public_chat_message_count < MAX_PUBLIC_MESSAGES) {
                            public_chat_messages[public_chat_message_count++] = parsed_client_message;
                        } else {
                            for (int message_index = 0; message_index < MAX_PUBLIC_MESSAGES - 1; message_index++) {
                                public_chat_messages[message_index] = public_chat_messages[message_index + 1];
                            }
                            public_chat_messages[MAX_PUBLIC_MESSAGES - 1] = parsed_client_message;
                        }

                        server_response_message = MessageSerializer::buildPublicMessage(
                            parsed_client_message.sender,
                            parsed_client_message.content
                        ) + "\n";
                        pthread_mutex_unlock(&clients_mutex);
                        break;

                    case MessageType::ChatPrivate: {
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);
                        trimLineBreaksAndSpaces(parsed_client_message.target);
                        trimLineBreaksAndSpaces(parsed_client_message.content);

                        int target_socket_file_descriptor =
                            findSocketByUsername(parsed_client_message.target);

                        if (target_socket_file_descriptor == -1) {
                            server_response_message =
                                MessageSerializer::buildErrorResponse("PRIVATE", "USER_NOT_FOUND") + "\n";
                            pthread_mutex_unlock(&clients_mutex);
                            break;
                        }

                        PrivateConversation* private_conversation =
                            findOrCreatePrivateConversation(
                                parsed_client_message.sender,
                                parsed_client_message.target
                            );

                        if (!private_conversation) {
                            server_response_message = MessageSerializer::buildErrorResponse(
                                "PRIVATE",
                                "MAX_CONVOS_REACHED"
                            ) + "\n";
                            pthread_mutex_unlock(&clients_mutex);
                            break;
                        }
                        pthread_mutex_unlock(&clients_mutex);

                        pthread_mutex_lock(&private_conversation->mutex);
                        if (private_conversation->message_count < PRIVATE_MSG_MAX) {
                            private_conversation->messages[private_conversation->message_count++] =
                                parsed_client_message;
                        } else {
                            for (int message_index = 0; message_index < PRIVATE_MSG_MAX - 1; message_index++) {
                                private_conversation->messages[message_index] =
                                    private_conversation->messages[message_index + 1];
                            }
                            private_conversation->messages[PRIVATE_MSG_MAX - 1] = parsed_client_message;
                        }
                        pthread_mutex_unlock(&private_conversation->mutex);

                        // Enviar solo el mensaje nuevo al destinatario.
                        // El historial completo se entrega únicamente via GETPRIVATE.
                        server_response_message = MessageSerializer::buildPrivateMessage(
                            parsed_client_message.sender,
                            parsed_client_message.target,
                            parsed_client_message.content
                        ) + "\n";

                        send(
                            target_socket_file_descriptor,
                            server_response_message.c_str(),
                            server_response_message.size(),
                            0
                        );
                        break;
                    }

                    case MessageType::GetPrivate: {
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);
                        trimLineBreaksAndSpaces(parsed_client_message.target);

                        PrivateConversation* private_conversation =
                            findOrCreatePrivateConversation(
                                parsed_client_message.sender,
                                parsed_client_message.target
                            );
                        pthread_mutex_unlock(&clients_mutex);

                        if (private_conversation) {
                            pthread_mutex_lock(&private_conversation->mutex);
                            for (int message_index = 0;
                                 message_index < private_conversation->message_count;
                                 message_index++) {
                                server_response_message += MessageSerializer::buildPrivateMessage(
                                    private_conversation->messages[message_index].sender,
                                    private_conversation->messages[message_index].target,
                                    private_conversation->messages[message_index].content
                                ) + "\n";
                            }
                            pthread_mutex_unlock(&private_conversation->mutex);
                        }
                        break;
                    }

                    case MessageType::GetAll:
                        pthread_mutex_lock(&clients_mutex);
                        for (int message_index = 0;
                             message_index < public_chat_message_count;
                             message_index++) {
                            if (public_chat_messages[message_index].type == MessageType::ChatPublic) {
                                server_response_message += MessageSerializer::buildPublicMessage(
                                    public_chat_messages[message_index].sender,
                                    public_chat_messages[message_index].content
                                ) + "\n";
                            }
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        break;

                    case MessageType::GetUsers:
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);
                        for (int client_index = 0; client_index < connected_client_count; client_index++) {
                            bool shares_private_conversation = userHasPrivateChatWithMe(
                                connected_clients[client_index].username,
                                parsed_client_message.sender
                            );

                            server_response_message += MessageSerializer::buildUserInfo(
                                connected_clients[client_index].username,
                                connected_clients[client_index].status,
                                shares_private_conversation
                            ) + "\n";
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        break;

                    case MessageType::Info:
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);
                        trimLineBreaksAndSpaces(parsed_client_message.content);

                        for (int client_index = 0; client_index < connected_client_count; client_index++) {
                            if (connected_clients[client_index].username == parsed_client_message.content) {
                                server_response_message = MessageSerializer::buildInfoResponse(
                                    connected_clients[client_index].username,
                                    connected_clients[client_index].ip_address,
                                    connected_clients[client_index].status
                                ) + "\n";
                                break;
                            }
                        }
                        pthread_mutex_unlock(&clients_mutex);

                        if (server_response_message.empty()) {
                            server_response_message = MessageSerializer::buildErrorResponse(
                                "INFO",
                                "USER_NOT_FOUND"
                            ) + "\n";
                        }
                        break;

                    case MessageType::Status:
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);
                        trimLineBreaksAndSpaces(parsed_client_message.content);

                        if (parsed_client_message.content != "online" &&
                            parsed_client_message.content != "offline" &&
                            parsed_client_message.content != "away") {
                            server_response_message = MessageSerializer::buildErrorResponse(
                                "STATUS",
                                "INVALID_STATUS"
                            ) + "\n";
                            pthread_mutex_unlock(&clients_mutex);
                            break;
                        }

                        for (int client_index = 0; client_index < connected_client_count; client_index++) {
                            if (connected_clients[client_index].socket_fd == client_socket_file_descriptor) {
                                connected_clients[client_index].status = parsed_client_message.content;
                                server_response_message = MessageSerializer::buildOkResponse("STATUS") + "\n";
                                break;
                            }
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        break;

                    case MessageType::Exit: {
                        pthread_mutex_lock(&clients_mutex);
                        trimLineBreaksAndSpaces(parsed_client_message.sender);

                        for (int client_index = 0; client_index < connected_client_count; client_index++) {
                            if (connected_clients[client_index].socket_fd == client_socket_file_descriptor) {
                                connected_clients[client_index] =
                                    connected_clients[--connected_client_count];
                                break;
                            }
                        }

                        // Notificar a todos los demás clientes que este usuario se desconectó.
                        std::string disconnect_broadcast =
                            MessageSerializer::buildServerWarning(
                                parsed_client_message.sender + " se ha desconectado"
                            ) + "\n";

                        for (int client_index = 0; client_index < connected_client_count; client_index++) {
                            send(
                                connected_clients[client_index].socket_fd,
                                disconnect_broadcast.c_str(),
                                disconnect_broadcast.size(),
                                0
                            );
                        }

                        server_response_message = MessageSerializer::buildOkResponse("EXIT") + "\n";
                        pthread_mutex_unlock(&clients_mutex);
                        client_exited_cleanly = true;
                        break;
                    }

                    case MessageType::Unknown:
                    default:
                        server_response_message = MessageSerializer::buildErrorResponse(
                            "UNKNOWN",
                            "INVALID_MESSAGE_FORMAT"
                        ) + "\n";
                        break;
                }

                if (!server_response_message.empty()) {
                    ssize_t sent_byte_count = send(
                        client_socket_file_descriptor,
                        server_response_message.c_str(),
                        server_response_message.size(),
                        0
                    );

                    if (sent_byte_count < 0) {
                        std::cerr << "Error: no se pudo enviar la respuesta al cliente." << std::endl;
                    }

                    std::cout << "Respuesta enviada al cliente: " << server_response_message << std::endl;
                }

                if (client_exited_cleanly) {
                    break;
                }
            }

            if (client_exited_cleanly) {
                break;
            }
        } else {
            std::cerr << "Error o conexión cerrada sin datos." << std::endl;

            // La remoción del cliente debe protegerse con mutex porque el arreglo
            // de clientes conectados es compartido entre múltiples hilos.
            pthread_mutex_lock(&clients_mutex);
            for (int client_index = 0; client_index < connected_client_count; client_index++) {
                if (connected_clients[client_index].socket_fd == client_socket_file_descriptor) {
                    connected_clients[client_index] =
                        connected_clients[--connected_client_count];
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            break;
        }
    }

    close(client_socket_file_descriptor);
    return nullptr;
}

void ChatServer::run() {
    std::cout << "Servidor listo para aceptar conexiones..." << std::endl;

    while (true) {
        sockaddr_in connected_client_address{};
        socklen_t connected_client_address_length = sizeof(connected_client_address);

        int connected_client_socket_file_descriptor = accept(
            server_socket_file_descriptor,
            reinterpret_cast<sockaddr*>(&connected_client_address),
            &connected_client_address_length
        );

        if (connected_client_socket_file_descriptor < 0) {
            std::cerr << "Error: no se pudo aceptar la conexión del cliente." << std::endl;
            continue;
        }

        std::cout << "Cliente conectado. Creando hilo para manejar su sesión." << std::endl;

        ClientThreadArgs* client_thread_arguments = new ClientThreadArgs{
            .socket_fd = connected_client_socket_file_descriptor,
            .address = connected_client_address
        };

        pthread_t client_thread_id;
        if (pthread_create(&client_thread_id, nullptr, handleClient, client_thread_arguments) != 0) {
            std::cerr << "Error: no se pudo crear el hilo para el cliente." << std::endl;
            close(connected_client_socket_file_descriptor);
            delete client_thread_arguments;
            continue;
        }

        // El hilo se marca como detached para que el sistema libere sus recursos
        // automáticamente cuando termine su ejecución.
        pthread_detach(client_thread_id);
    }
}

void ChatServer::stop() {
    if (server_socket_file_descriptor >= 0) {
        close(server_socket_file_descriptor);
        server_socket_file_descriptor = -1;
    }
}