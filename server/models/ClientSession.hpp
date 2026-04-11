struct ClientSession {
    int socket_fd; // fd -> file descriptor, aka identificador del socket
    std::string username;
    std::string ip_address;
    std::string status;
    std::string thread_id;
};