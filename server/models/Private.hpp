#define PRIVATE_MSG_MAX 10

struct PrivateConversation {
    std::string user_a; // participante A (alfabéticamente menor)
    std::string user_b; // participante B
    Message messages[PRIVATE_MSG_MAX]; // buffer mensajes privados
    int message_count = 0;
    bool active = false; // ¿slot en uso?
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // mutex PROPIO del par
};