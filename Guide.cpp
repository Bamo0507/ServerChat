#include "ChatServer.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "../models/Message.hpp"
#include "../protocol/MessageParser.hpp"
#include "../protocol/MessageSerializer.hpp"

#define BUFFER_SIZE     50
#define MAX_CLIENTS     5
#define PRIVATE_MSG_MAX 20
#define MAX_PRIVATE_CONVOS (MAX_CLIENTS * (MAX_CLIENTS - 1) / 2)  // 10 para 5 clientes

// ── Conversación privada entre exactamente 2 usuarios ───────────────────────
struct PrivateConversation {
    std::string user_a;                       // participante A (alfabéticamente menor)
    std::string user_b;                       // participante B
    Message     messages[PRIVATE_MSG_MAX];    // buffer circular
    int         message_count = 0;            // cuántos mensajes hay (máx PRIVATE_MSG_MAX)
    bool        active        = false;        // ¿slot en uso?
    pthread_mutex_t mutex     = PTHREAD_MUTEX_INITIALIZER; // mutex PROPIO del par
};

// ── Estado global ────────────────────────────────────────────────────────────
Message             BroadCastMessages[BUFFER_SIZE];
int                 broadcast_count = 0;

ClientSession       ConnectedClients[MAX_CLIENTS];
int                 connected_count = 0;

PrivateConversation PrivateConvos[MAX_PRIVATE_CONVOS];
int                 private_convo_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
// NOTA: PrivateConvos usa su propio mutex por slot; para crear/buscar slots
//       se reutiliza clients_mutex ya que es una operación breve.

// ── Helpers para conversaciones privadas ─────────────────────────────────────

// Devuelve la clave canónica "menor|mayor" para identificar un par
static void canonical_pair(const std::string& a, const std::string& b,
                            std::string& out_a, std::string& out_b) {
    if (a < b) { out_a = a; out_b = b; }
    else        { out_a = b; out_b = a; }
}

// Busca un slot existente; si no existe lo crea. Devuelve puntero o nullptr si lleno.
// ⚠ Llamar con clients_mutex YA tomado.
static PrivateConversation* find_or_create_convo(const std::string& a,
                                                  const std::string& b) {
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
static bool user_has_private_chat(const std::string& username) {
    for (int i = 0; i < private_convo_count; i++) {
        if (PrivateConvos[i].active &&
            (PrivateConvos[i].user_a == username ||
             PrivateConvos[i].user_b == username)) {
            return true;
        }
    }
    return false;
}

// Busca el socket_fd de un usuario conectado.
// ⚠ Llamar con clients_mutex YA tomado.
static int find_socket_by_username(const std::string& username) {
    for (int i = 0; i < connected_count; i++) {
        if (ConnectedClients[i].username == username)
            return ConnectedClients[i].socket_fd;
    }
    return -1;
}

// ── Struct de args del hilo ──────────────────────────────────────────────────
struct ClientThreadArgs {
    int         socket_fd;
    sockaddr_in address;
};

// ── Hilo por cliente ─────────────────────────────────────────────────────────
static void* handleClient(void* arg) {
    ClientThreadArgs* args = static_cast<ClientThreadArgs*>(arg);
    int         client_fd  = args->socket_fd;
    sockaddr_in client_addr = args->address;
    delete args;

    char receive_buffer[1024];
    std::memset(receive_buffer, 0, sizeof(receive_buffer));

    ssize_t received_bytes = recv(client_fd, receive_buffer,
                                  sizeof(receive_buffer) - 1, 0);

    if (received_bytes > 0) {
        std::string raw(receive_buffer, received_bytes);
        std::cout << "Mensaje recibido: " << raw << std::endl;

        Message     parsed   = MessageParser::parse(raw);
        std::string response;

        switch (parsed.type) {

            // ── REGISTER ────────────────────────────────────────────────────
            case MessageType::Register:
                pthread_mutex_lock(&clients_mutex);
                if (connected_count < MAX_CLIENTS) {
                    ConnectedClients[connected_count++] = {
                        .socket_fd  = client_fd,
                        .username   = parsed.sender,
                        .ip_address = inet_ntoa(client_addr.sin_addr),
                        .status     = "online",
                        .thread_id  = std::to_string(pthread_self())
                    };
                    response = MessageSerializer::buildOkResponse("REGISTER") + "\n";
                } else {
                    response = MessageSerializer::buildErrorResponse(
                                   "REGISTER", "SERVER_FULL") + "\n";
                }
                pthread_mutex_unlock(&clients_mutex);
                break;

            // ── CHAT PÚBLICO ─────────────────────────────────────────────────
            case MessageType::ChatPublic:
                pthread_mutex_lock(&clients_mutex);
                if (broadcast_count < BUFFER_SIZE) {
                    BroadCastMessages[broadcast_count++] = parsed;
                } else {
                    for (int i = 0; i < BUFFER_SIZE - 1; i++)
                        BroadCastMessages[i] = BroadCastMessages[i + 1];
                    BroadCastMessages[BUFFER_SIZE - 1] = parsed;
                }
                response = MessageSerializer::buildPublicMessage(
                               parsed.sender, parsed.content) + "\n";
                pthread_mutex_unlock(&clients_mutex);
                break;

            // ── CHAT PRIVADO ─────────────────────────────────────────────────
            // Formato entrada:  PRIVATE|sender|target|contenido
            // Formato respuesta al sender:  PRIVATE|sender|target|contenido
            // Formato entrega al target (push): igual, enviado directamente
            case MessageType::ChatPrivate: {
                // 1. Verificar que el target existe
                pthread_mutex_lock(&clients_mutex);
                int target_fd = find_socket_by_username(parsed.target);

                if (target_fd == -1) {
                    response = MessageSerializer::buildErrorResponse(
                                   "PRIVATE", "USER_NOT_FOUND") + "\n";
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                }

                // 2. Obtener (o crear) la conversación privada entre los dos
                PrivateConversation* convo =
                    find_or_create_convo(parsed.sender, parsed.target);

                if (!convo) {
                    response = MessageSerializer::buildErrorResponse(
                                   "PRIVATE", "MAX_CONVOS_REACHED") + "\n";
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                }
                pthread_mutex_unlock(&clients_mutex);

                // 3. Guardar el mensaje en el buffer privado (mutex propio del par)
                pthread_mutex_lock(&convo->mutex);
                if (convo->message_count < PRIVATE_MSG_MAX) {
                    convo->messages[convo->message_count++] = parsed;
                } else {
                    // Buffer circular: descarta el más antiguo
                    for (int i = 0; i < PRIVATE_MSG_MAX - 1; i++)
                        convo->messages[i] = convo->messages[i + 1];
                    convo->messages[PRIVATE_MSG_MAX - 1] = parsed;
                }
                pthread_mutex_unlock(&convo->mutex);

                // 4. Construir el mensaje formateado
                std::string private_msg = MessageSerializer::buildPrivateMessage(
                                              parsed.sender, parsed.target,
                                              parsed.content) + "\n";

                // 5. Hacer push al target (entrega directa)
                ssize_t pushed = send(target_fd, private_msg.c_str(),
                                      private_msg.size(), 0);
                if (pushed < 0)
                    std::cerr << "Error enviando mensaje privado a target." << std::endl;

                // 6. Confirmar al sender
                response = private_msg;
                break;
            }

            // ── GET ALL (mensajes públicos) ──────────────────────────────────
            case MessageType::GetAll:
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < broadcast_count; i++) {
                    if (BroadCastMessages[i].type == MessageType::ChatPublic) {
                        response += MessageSerializer::buildPublicMessage(
                                        BroadCastMessages[i].sender,
                                        BroadCastMessages[i].content) + "\n";
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                break;

            // ── GET USERS ────────────────────────────────────────────────────
            // Respuesta extendida: usuario|status|in_private_chat (true/false)
            case MessageType::GetUsers:
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < connected_count; i++) {
                    bool in_chat = user_has_private_chat(ConnectedClients[i].username);
                    response += MessageSerializer::buildUserInfo(
                                    ConnectedClients[i].username,
                                    ConnectedClients[i].status,
                                    in_chat) + "\n";
                }
                pthread_mutex_unlock(&clients_mutex);
                break;

            // ── INFO ─────────────────────────────────────────────────────────
            case MessageType::Info:
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < connected_count; i++) {
                    if (ConnectedClients[i].username == parsed.content) {
                        response = MessageSerializer::buildInfoResponse(
                                       ConnectedClients[i].username,
                                       ConnectedClients[i].ip_address,
                                       ConnectedClients[i].status) + "\n";
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                if (response.empty())
                    response = MessageSerializer::buildErrorResponse(
                                   "INFO", "USER_NOT_FOUND") + "\n";
                break;

            // ── STATUS ───────────────────────────────────────────────────────
            case MessageType::Status:
                pthread_mutex_lock(&clients_mutex);
                if (parsed.content != "online" &&
                    parsed.content != "offline" &&
                    parsed.content != "away") {
                    response = MessageSerializer::buildErrorResponse(
                                   "STATUS", "INVALID_STATUS") + "\n";
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                }
                for (int i = 0; i < connected_count; i++) {
                    if (ConnectedClients[i].socket_fd == client_fd) {
                        ConnectedClients[i].status = parsed.content;
                        response = MessageSerializer::buildOkResponse("STATUS") + "\n";
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                break;

            // ── EXIT ─────────────────────────────────────────────────────────
            case MessageType::Exit:
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < connected_count; i++) {
                    if (ConnectedClients[i].socket_fd == client_fd) {
                        ConnectedClients[i] = ConnectedClients[--connected_count];
                        break;
                    }
                }
                response = MessageSerializer::buildOkResponse("EXIT") + "\n";
                pthread_mutex_unlock(&clients_mutex);
                break;

            // ── UNKNOWN ──────────────────────────────────────────────────────
            case MessageType::Unknown:
            default:
                response = MessageSerializer::buildErrorResponse(
                               "UNKNOWN", "INVALID_MESSAGE_FORMAT") + "\n";
                break;
        }

        ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);
        if (sent < 0)
            std::cerr << "Error enviando respuesta al cliente." << std::endl;
        else
            std::cout << "Respuesta enviada: " << response << std::endl;

    } else {
        std::cerr << "Error o conexión cerrada sin datos." << std::endl;
    }

    close(client_fd);
    return nullptr;
}

// ── ChatServer ────────────────────────────────────────────────────────────────
ChatServer::ChatServer(int port)
    : server_port(port), server_socket_file_descriptor(-1) {}

bool ChatServer::start() {
    std::cout << "Inicializando servidor en el puerto " << server_port << std::endl;

    server_socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_file_descriptor < 0) {
        std::cerr << "Error: no se pudo crear el socket del servidor." << std::endl;
        return false;
    }

    sockaddr_in server_address{};
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port        = htons(server_port);

    if (bind(server_socket_file_descriptor,
             reinterpret_cast<sockaddr*>(&server_address),
             sizeof(server_address)) < 0) {
        std::cerr << "Error: bind fallido en puerto " << server_port << std::endl;
        return false;
    }

    if (listen(server_socket_file_descriptor, MAX_CLIENTS) < 0) {
        std::cerr << "Error: modo escucha fallido." << std::endl;
        return false;
    }

    std::cout << "Servidor en modo escucha correctamente." << std::endl;
    return true;
}

void ChatServer::run() {
    std::cout << "Servidor listo para aceptar conexiones..." << std::endl;

    while (true) {
        sockaddr_in client_address{};
        socklen_t   len = sizeof(client_address);

        int client_fd = accept(server_socket_file_descriptor,
                               reinterpret_cast<sockaddr*>(&client_address), &len);
        if (client_fd < 0) {
            std::cerr << "Error aceptando conexión." << std::endl;
            continue;
        }

        std::cout << "Cliente conectado. Creando hilo..." << std::endl;

        ClientThreadArgs* args = new ClientThreadArgs{
            .socket_fd = client_fd,
            .address   = client_address
        };

        pthread_t tid;
        if (pthread_create(&tid, nullptr, handleClient, args) != 0) {
            std::cerr << "Error creando hilo." << std::endl;
            close(client_fd);
            delete args;
            continue;
        }
        pthread_detach(tid);
    }
}

void ChatServer::stop() {
    if (server_socket_file_descriptor >= 0) {
        close(server_socket_file_descriptor);
        server_socket_file_descriptor = -1;
    }
}