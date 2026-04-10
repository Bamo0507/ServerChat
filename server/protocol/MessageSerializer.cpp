#include "MessageSerializer.hpp"

std::string MessageSerializer::buildOkResponse(const std::string& action) {
    return "OK|" + action;
}

std::string MessageSerializer::buildErrorResponse(
    const std::string& action,
    const std::string& reason
) {
    return "ERROR|" + action + "|" + reason;
}

std::string MessageSerializer::buildPublicMessage(
    const std::string& sender,
    const std::string& content
) {
    return "PUBLIC_MSG|" + sender + "|" + content;
}

std::string MessageSerializer::buildServerWarning(const std::string& content) {
    return "SERVER_WARN|" + content;
}

std::string MessageSerializer::buildPrivateMessage(
    const std::string& username,
    const std::string& target,
    const std::string& status
    //bool in_private_chat
) {
    return "PRIVATE_MSG|" + username + "|" + target + "|" + status ;
    //+ "|" + (in_private_chat ? "true" : "false")
}

std::string MessageSerializer::buildInfoResponse(
    const std::string& username,
    const std::string& ip_address,
    const std::string& status
) {
    return "INFO|" + username + "|" + ip_address + "|" + status;
}

std::string MessageSerializer::buildUserInfo(
    const std::string& username,
    const std::string& status,
    bool in_private_chat
) {
    return "USER_INFO|" + username + "|" + status + "|" + (in_private_chat ? "true" : "false");
}


// TODO: Implementar serializers adicionales cuando se agreguen más
// respuestas del servidor al protocolo.
// - mensajes privados
// - listado de usuarios conectados
// - información de usuario
// - confirmaciones de cambio de estado