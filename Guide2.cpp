static void* handleClient(void* arg) {
    ClientThreadArgs* args = static_cast<ClientThreadArgs*>(arg);
    int         client_fd  = args->socket_fd;
    sockaddr_in client_addr = args->address;
    delete args;

    // ── Loop principal: mantener conexión viva ───────────────────────────────
    while (true) {
        char receive_buffer[1024];
        std::memset(receive_buffer, 0, sizeof(receive_buffer));

        ssize_t received_bytes = recv(client_fd, receive_buffer,
                                      sizeof(receive_buffer) - 1, 0);

        // Cliente desconectado o error
        if (received_bytes <= 0) {
            std::cout << "Cliente desconectado: fd=" << client_fd << std::endl;

            // Limpiar de ConnectedClients igual que Exit
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < connected_count; i++) {
                if (ConnectedClients[i].socket_fd == client_fd) {
                    ConnectedClients[i] = ConnectedClients[--connected_count];
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            break; // salir del loop → hilo termina
        }

        std::string raw(receive_buffer, received_bytes);
        std::cout << "Mensaje recibido del cliente: " << raw << std::endl;

        Message parsed = MessageParser::parse(raw);
        trim_newlines(parsed.sender);
        trim_newlines(parsed.target);
        trim_newlines(parsed.content);

        std::string response;
        bool should_exit = false;  // flag para salir del loop limpiamente

        switch (parsed.type) {

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

            case MessageType::ChatPrivate: {
                pthread_mutex_lock(&clients_mutex);
                int target_fd = find_socket_by_username(parsed.target);

                if (target_fd == -1) {
                    response = MessageSerializer::buildErrorResponse(
                                   "PRIVATE", "USER_NOT_FOUND") + "\n";
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                }

                PrivateConversation* convo =
                    find_or_create_convo(parsed.sender, parsed.target);

                if (!convo) {
                    response = MessageSerializer::buildErrorResponse(
                                   "PRIVATE", "MAX_CONVOS_REACHED") + "\n";
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                }
                pthread_mutex_unlock(&clients_mutex);

                pthread_mutex_lock(&convo->mutex);
                if (convo->message_count < PRIVATE_MSG_MAX) {
                    convo->messages[convo->message_count++] = parsed;
                } else {
                    for (int i = 0; i < PRIVATE_MSG_MAX - 1; i++)
                        convo->messages[i] = convo->messages[i + 1];
                    convo->messages[PRIVATE_MSG_MAX - 1] = parsed;
                }
                pthread_mutex_unlock(&convo->mutex);

                std::string private_msg = MessageSerializer::buildPrivateMessage(
                                              parsed.sender, parsed.target,
                                              parsed.content) + "\n";
                send(target_fd, private_msg.c_str(), private_msg.size(), 0);
                response = private_msg;
                break;
            }

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
                should_exit = true;  // salir del loop después de responder
                break;

            case MessageType::Unknown:
            default:
                response = MessageSerializer::buildErrorResponse(
                               "UNKNOWN", "INVALID_MESSAGE_FORMAT") + "\n";
                break;
        }

        // Enviar respuesta
        if (!response.empty()) {
            ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);
            if (sent < 0)
                std::cerr << "Error enviando respuesta." << std::endl;
            else
                std::cout << "Respuesta enviada al cliente: " << response << std::endl;
        }

        if (should_exit) break;

    } // fin while(true)

    close(client_fd);
    return nullptr;
}