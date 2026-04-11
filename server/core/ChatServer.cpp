#include "ChatServer.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
<<<<<<< server-responses
#include <pthread.h>
#include <sstream>
=======
>>>>>>> main

#include "../models/Message.hpp"
#include "../protocol/MessageParser.hpp"
#include "../protocol/MessageSerializer.hpp"
<<<<<<< server-responses
#include "../models/ClientSession.hpp"
#include "../models/Private.hpp"


#define BUFFER_SIZE 50
#define MAX_CLIENTS 5
#define MAX_PRIVATE_CONVOS (MAX_CLIENTS * (MAX_CLIENTS - 1) / 2)  // 10 para 5 clientes

=======
>>>>>>> main

ChatServer::ChatServer(int port)
    : server_port(port), server_socket_file_descriptor(-1) {
}

<<<<<<< server-responses
Message BroadCastMessages[BUFFER_SIZE];
int broadcast_count = 0;

ClientSession ConnectedClients[MAX_CLIENTS];
int connected_count = 0;

PrivateConversation PrivateConvos[MAX_PRIVATE_CONVOS];
int                 private_convo_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


// ── Helpers para conversaciones privadas ─────────────────────────────────────

// Devuelve la clave canónica "menor|mayor" para identificar un par
static void canonical_pair(const std::string& a, const std::string& b, std::string& out_a, std::string& out_b) {
    if (a < b) { out_a = a; out_b = b; }
    else        { out_a = b; out_b = a; }
}

// Busca un slot existente; si no existe lo crea. Devuelve puntero o nullptr si lleno.
// ⚠ Llamar con clients_mutex YA tomado.
static PrivateConversation* find_or_create_convo(const std::string& a, const std::string& b) {
    std::string ka, kb;
    canonical_pair(a, b, ka, kb);

    for (int i = 0; i < private_convo_count; i++) {
        if (PrivateConvos[i].active &&
            PrivateConvos[i].user_a == ka &&
            PrivateConvos[i].user_b == kb) {
            return &PrivateConvos[i];
        }
    }

    // Crear nuevo slot
    if (private_convo_count >= MAX_PRIVATE_CONVOS) return nullptr;

    PrivateConversation& slot = PrivateConvos[private_convo_count++];
    slot.user_a        = ka;
    slot.user_b        = kb;
    slot.message_count = 0;
    slot.active        = true;
    pthread_mutex_init(&slot.mutex, nullptr);
    return &slot;
}

// Devuelve true si el usuario tiene al menos una conversación privada activa.
// ⚠ Llamar con clients_mutex YA tomado.
static bool user_has_private_chat_with_me(const std::string& username, const std::string& my_username) {
    for (int i = 0; i < private_convo_count; i++) {
        if (PrivateConvos[i].active &&
            (PrivateConvos[i].user_a == username || PrivateConvos[i].user_b == username) &&
            (PrivateConvos[i].user_a == my_username || PrivateConvos[i].user_b == my_username)) {
            return true;
        }
    }
    return false;
}

// Busca el socket_fd de un usuario conectado.
// ⚠ Llamar con clients_mutex YA tomado.
static int find_socket_by_username(const std::string& username) {
    // std::cout << "DEBUG find_socket: buscando '" << username 
    //           << "' entre " << connected_count << " clientes:" << std::endl;
    for (int i = 0; i < connected_count; i++) {
        // std::cout << "  [" << i << "] username='" << ConnectedClients[i].username 
        //           << "' fd=" << ConnectedClients[i].socket_fd << std::endl;
        if (ConnectedClients[i].username == username)
            return ConnectedClients[i].socket_fd;
    }
    return -1;
}

// ── Helper: elimina \r y \n del inicio y final de un string ─────────────────
static void trim_newlines(std::string& s) {
    // Desde el final
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    // Desde el inicio
    while (!s.empty() && (s.front() == '\n' || s.front() == '\r' || s.front() == ' '))
        s.erase(s.begin());
}



// Argument struct passed to each client thread
struct ClientThreadArgs {
    int socket_fd;
    sockaddr_in address;
};

=======
>>>>>>> main
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

<<<<<<< server-responses
static void* handleClient(void* arg) {
    ClientThreadArgs* args = static_cast<ClientThreadArgs*>(arg);
    int client_socket_file_descriptor = args->socket_fd;
    sockaddr_in client_address = args->address;
    delete args; // free heap allocation made in run()

    while (true) {
=======
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

>>>>>>> main
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
<<<<<<< server-responses
                    pthread_mutex_lock(&clients_mutex);
                    trim_newlines(parsed_message.sender);
                    if (connected_count < MAX_CLIENTS) {
                        ConnectedClients[connected_count++] = {
                            .socket_fd   = client_socket_file_descriptor,
                            .username    = parsed_message.sender,
                            .ip_address  = inet_ntoa(client_address.sin_addr),
                            .status      = "online",
                            .thread_id = [] {
                                std::ostringstream oss;
                                oss << pthread_self();
                                return oss.str();
                            }()
                        };
                    } else {
                        server_response = MessageSerializer::buildErrorResponse(
                            "REGISTER",
                            "SERVER_FULL"
                        ) + "\n";
                        pthread_mutex_unlock(&clients_mutex);
                        break;
                    }
                    server_response = MessageSerializer::buildOkResponse("REGISTER") + "\n";
                    pthread_mutex_unlock(&clients_mutex);
                    break;

                case MessageType::ChatPublic:
                    pthread_mutex_lock(&clients_mutex);
                    trim_newlines(parsed_message.sender);
                    trim_newlines(parsed_message.content);
                    if (broadcast_count < BUFFER_SIZE) {
                        BroadCastMessages[broadcast_count++] = parsed_message;
                    } else {
                        // Shift all messages down by one and add new message at the end
                        for (int i = 0; i < BUFFER_SIZE - 1; i++) {
                            BroadCastMessages[i] = BroadCastMessages[i + 1];
                        }
                        BroadCastMessages[BUFFER_SIZE - 1] = parsed_message;
                    }
=======
                    server_response = MessageSerializer::buildOkResponse("REGISTER") + "\n";
                    break;

                case MessageType::ChatPublic:
>>>>>>> main
                    server_response = MessageSerializer::buildPublicMessage(
                        parsed_message.sender,
                        parsed_message.content
                    ) + "\n";
<<<<<<< server-responses
                    pthread_mutex_unlock(&clients_mutex);
                    break;


                // PRIVATE MESSAGE: PRIVATE|SENDER|TARGET|CONTENT
                case MessageType::ChatPrivate: {
                    // 1. Verificar que el target existe
                    pthread_mutex_lock(&clients_mutex);
                    trim_newlines(parsed_message.sender);
                    trim_newlines(parsed_message.target);
                    trim_newlines(parsed_message.content);
                    int target_fd = find_socket_by_username(parsed_message.target);

                    if (target_fd == -1) {
                        server_response = MessageSerializer::buildErrorResponse("PRIVATE", "USER_NOT_FOUND") + "\n";
                        pthread_mutex_unlock(&clients_mutex);
                        break;
                    }

                    // 2. Obtener (o crear) la conversación privada entre los dos
                    PrivateConversation* convo =
                        find_or_create_convo(parsed_message.sender, parsed_message.target);

                    if (!convo) {
                        server_response = MessageSerializer::buildErrorResponse("PRIVATE", "MAX_CONVOS_REACHED") + "\n";
                        pthread_mutex_unlock(&clients_mutex);
                        break;
                    }
                    pthread_mutex_unlock(&clients_mutex);

                    // 3. Guardar el mensaje en el buffer privado (mutex propio del par)
                    pthread_mutex_lock(&convo->mutex);
                    if (convo->message_count < PRIVATE_MSG_MAX) {
                        convo->messages[convo->message_count++] = parsed_message;
                    } else {
                        // Buffer circular: descarta el más antiguo
                        for (int i = 0; i < PRIVATE_MSG_MAX - 1; i++)
                            convo->messages[i] = convo->messages[i + 1];
                        convo->messages[PRIVATE_MSG_MAX - 1] = parsed_message;
                    }
                    pthread_mutex_unlock(&convo->mutex);

                    // 4. Send all messages in the conversation to both users
                    pthread_mutex_lock(&convo->mutex);
                    for (int i = 0; i < convo->message_count; i++) {
                        std::string private_msg = MessageSerializer::buildPrivateMessage(
                            convo->messages[i].sender,
                            convo->messages[i].target,
                            convo->messages[i].content
                        ) + "\n";
                        
                        ssize_t pushed = send(target_fd, private_msg.c_str(), private_msg.size(), 0);
                        if (pushed < 0)
                            std::cerr << "Error enviando mensaje privado a target." << std::endl;
                    }
                    pthread_mutex_unlock(&convo->mutex);

                    // 5. Confirm to sender with the latest message
                    server_response = MessageSerializer::buildPrivateMessage(
                        parsed_message.sender,
                        parsed_message.target,
                        parsed_message.content
                    ) + "\n";
                    break;
                }

                // GETALL|BRYAN -> GETALL|BRYAN|CHAT_PUBLIC|CONTENT\nGETALL|BRYAN|CHAT_PUBLIC|CONTENT\n...
                case MessageType::GetAll:
                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < 50; i++) {
                        if (BroadCastMessages[i].type == MessageType::ChatPublic) {
                            server_response += MessageSerializer::buildPublicMessage(
                                BroadCastMessages[i].sender,
                                BroadCastMessages[i].content
                            ) + "\n";
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                
                // GETUSERS|BRYAN -> GETUSERS|BRYAN|USER1|STATUS1\nGETUSERS|BRYAN|USER2|STATUS2\n...
                case MessageType::GetUsers:
                    pthread_mutex_lock(&clients_mutex);
                    trim_newlines(parsed_message.sender);
                    for (int i = 0; i < connected_count; i++) {
                        bool in_chat = user_has_private_chat_with_me(ConnectedClients[i].username, parsed_message.sender);
                        server_response += MessageSerializer::buildUserInfo(
                            ConnectedClients[i].username,
                            ConnectedClients[i].status,
                            in_chat
                        ) + "\n";
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    break;

                // INFO|BRYAN|ADRIANA -> INFO|ADRIANA|IP|STATUS
                case MessageType::Info:
                    pthread_mutex_lock(&clients_mutex);
                    trim_newlines(parsed_message.sender);
                    trim_newlines(parsed_message.content);
                    // search user info by username 
                    for (int i = 0; i < connected_count; i++) {
                        if (ConnectedClients[i].username == parsed_message.content) {
                            server_response = MessageSerializer::buildInfoResponse(
                                ConnectedClients[i].username,
                                ConnectedClients[i].ip_address,
                                ConnectedClients[i].status
                            ) + "\n";
                            break;
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    if (server_response.empty()) {
                        server_response = MessageSerializer::buildErrorResponse(
                            "INFO",
                            "USER_NOT_FOUND"
                        ) + "\n";
                    }
                    break;

                // STATUS|BRYAN|away -> STATUS|BRYAN|OK or STATUS|BRYAN|ERROR|INVALID_STATUS
                case MessageType::Status:
                    pthread_mutex_lock(&clients_mutex);
                    trim_newlines(parsed_message.sender);
                    trim_newlines(parsed_message.content);
                    if (parsed_message.content != "online" &&
                        parsed_message.content != "offline" &&
                        parsed_message.content != "away") {
                        server_response = MessageSerializer::buildErrorResponse(
                            "STATUS",
                            "INVALID_STATUS"
                        ) + "\n";
                        pthread_mutex_unlock(&clients_mutex);
                        break;
                    }
                    for (int i = 0; i < connected_count; i++) {
                        if (ConnectedClients[i].socket_fd == client_socket_file_descriptor) {
                            ConnectedClients[i].status = parsed_message.content;
                            server_response = MessageSerializer::buildOkResponse("STATUS") + "\n";
                            break;
                        }
                    }
                    
                    pthread_mutex_unlock(&clients_mutex);
                    break;

                case MessageType::Exit:
                    pthread_mutex_lock(&clients_mutex);
                    trim_newlines(parsed_message.sender);
                    // Find and remove the client from ConnectedClients
                    for (int i = 0; i < connected_count; i++) {
                        if (ConnectedClients[i].socket_fd == client_socket_file_descriptor) {
                            ConnectedClients[i] = ConnectedClients[--connected_count];
                            break;
                        }
                    }
                    server_response = MessageSerializer::buildOkResponse("EXIT") + "\n";
                    pthread_mutex_unlock(&clients_mutex);
=======
                    break;

                case MessageType::Exit:
                    server_response = MessageSerializer::buildOkResponse("EXIT") + "\n";
>>>>>>> main
                    break;

                // TODO: Agregar manejo explícito para otros tipos de mensaje
                // conforme se implementen más interacciones del protocolo.
                // Casos previstos para esta sección:
                // - MessageType::ChatPrivate
<<<<<<< server-responses
                // - MessageType::Status -> DONE
                // - MessageType::Info -> DONE
                // - MessageType::GetAll -> DONE
                // - MessageType::GetUsers -> DONE
=======
                // - MessageType::Status
                // - MessageType::Info
                // - MessageType::GetAll
>>>>>>> main
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
<<<<<<< server-responses
            // Find and remove the client from ConnectedClients
            for (int i = 0; i < connected_count; i++) {
                if (ConnectedClients[i].socket_fd == client_socket_file_descriptor) {
                    ConnectedClients[i] = ConnectedClients[--connected_count];
                    break;
                }
            }
            break; // salir del loop → hilo termina
        }
    }

    close(client_socket_file_descriptor);
    return nullptr;
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

        std::cout << "Cliente conectado. Creando hilo..." << std::endl;

        ClientThreadArgs* args = new ClientThreadArgs{
            .socket_fd = client_socket_file_descriptor,
            .address = client_address
        };

        pthread_t thread_id;
        if (pthread_create(&thread_id, nullptr, handleClient, args) != 0) {
            std::cerr << "Error: no se pudo crear el hilo para el cliente." << std::endl;
            close(client_socket_file_descriptor);
            delete args;
            continue;
        }

        // Detach so resources are freed automatically when the thread finishes
        pthread_detach(thread_id);
=======
        }

        close(client_socket_file_descriptor);
>>>>>>> main
    }
}

void ChatServer::stop() {
    if (server_socket_file_descriptor >= 0) {
        close(server_socket_file_descriptor);
        server_socket_file_descriptor = -1;
    }
}