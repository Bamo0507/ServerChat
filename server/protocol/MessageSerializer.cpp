#include "MessageSerializer.hpp"

// OK|REGISTER  /  OK|STATUS  /  OK|EXIT
std::string MessageSerializer::buildOkResponse(const std::string& action) {
    return "OK|" + action;
}

// ERROR|REGISTER|SERVER_FULL  /  ERROR|PRIVATE|USER_NOT_FOUND  /  ERROR|STATUS|INVALID_STATUS
std::string MessageSerializer::buildErrorResponse(
    const std::string& action,
    const std::string& reason
) {
    return "ERROR|" + action + "|" + reason;
}

// PUBLIC_MSG|Bryan|Hola a todos
std::string MessageSerializer::buildPublicMessage(
    const std::string& sender,
    const std::string& content
) {
    return "PUBLIC_MSG|" + sender + "|" + content;
}

// SERVER_WARN|Bryan se ha desconectado
std::string MessageSerializer::buildServerWarning(const std::string& content) {
    return "SERVER_WARN|" + content;
}

// PRIVATE_MSG|Bryan|Ana|Hola Ana
std::string MessageSerializer::buildPrivateMessage(
    const std::string& username,
    const std::string& target,
    const std::string& content
) {
    return "PRIVATE_MSG|" + username + "|" + target + "|" + content;
}

// INFO|Bryan|192.168.1.50|online
std::string MessageSerializer::buildInfoResponse(
    const std::string& username,
    const std::string& ip_address,
    const std::string& status
) {
    return "INFO|" + username + "|" + ip_address + "|" + status;
}

// USER_INFO|Bryan|online|true
std::string MessageSerializer::buildUserInfo(
    const std::string& username,
    const std::string& status,
    bool in_private_chat
) {
    return "USER_INFO|" + username + "|" + status + "|" + (in_private_chat ? "true" : "false");
}