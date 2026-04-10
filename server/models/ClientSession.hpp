struct ClientSession {
    int socket_fd;
    std::string username;
    std::string ip_address;
    std::string status;
    std::string thread_id;
};