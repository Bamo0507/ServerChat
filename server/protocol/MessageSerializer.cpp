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

// TODO: Implementar serializers adicionales cuando se agreguen más
// respuestas del servidor al protocolo.
// - mensajes privados
// - listado de usuarios conectados
// - información de usuario
// - confirmaciones de cambio de estado