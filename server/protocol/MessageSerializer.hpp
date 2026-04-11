#ifndef MESSAGE_SERIALIZER_HPP
#define MESSAGE_SERIALIZER_HPP

#include <string>

// MessageSerializer se encarga de construir mensajes de texto
// que el servidor enviará a los clientes siguiendo el protocolo definido.
class MessageSerializer {
public:
    static std::string buildOkResponse(const std::string& action);
    static std::string buildErrorResponse(const std::string& action, const std::string& reason);
    static std::string buildPublicMessage(const std::string& sender, const std::string& content);
    static std::string buildServerWarning(const std::string& content);
    static std::string buildPrivateMessage(
        const std::string& username,
        const std::string& target,
        const std::string& content
    );
    static std::string buildInfoResponse(
        const std::string& username,
        const std::string& ip_address,
        const std::string& status
    );
    static std::string buildUserInfo(
        const std::string& username,
        const std::string& status,
        bool in_private_chat
    );
    static std::string buildBroadcastMessage(
        const std::string& sender,
        const std::string& content
    );
};

#endif