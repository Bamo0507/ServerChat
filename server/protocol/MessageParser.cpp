#include "MessageParser.hpp"

#include <sstream>
#include <vector>

using std::string;
using std::vector;

vector<string> splitMessage(const string& raw_message, char delimiter) {
    vector<string> message_parts;

    std::stringstream message_stream(raw_message);
    string current_part;

    while (std::getline(message_stream, current_part, delimiter)) {
        message_parts.push_back(current_part);
    }

    return message_parts;
}

MessageType parseMessageType(const string& command_text) {
    if (command_text == "REGISTER") return MessageType::Register;
    if (command_text == "CHAT") return MessageType::ChatPublic;
    if (command_text == "PRIVATE") return MessageType::ChatPrivate;
    if (command_text == "STATUS") return MessageType::Status;
    if (command_text == "INFO") return MessageType::Info;
    if (command_text == "GETALL") return MessageType::GetAll;
<<<<<<< server-responses
    if (command_text == "GETUSERS") return MessageType::GetUsers;
=======
>>>>>>> main
    if (command_text == "EXIT") return MessageType::Exit;

    return MessageType::Unknown;
}

Message MessageParser::parse(const string& raw_message) {
    Message message;

    vector<string> message_parts = splitMessage(raw_message, '|');

    if (message_parts.empty()) {
        message.type = MessageType::Unknown;
        return message;
    }

    MessageType message_type = parseMessageType(message_parts[0]);
    message.type = message_type;

    switch (message_type) {

        case MessageType::Register:
            if (message_parts.size() >= 2) {
                message.sender = message_parts[1];
            }
            break;

        case MessageType::ChatPublic:
            if (message_parts.size() >= 3) {
                message.sender = message_parts[1];
                message.content = message_parts[2];
            }
            break;

        case MessageType::ChatPrivate:
            if (message_parts.size() >= 4) {
                message.sender = message_parts[1];
                message.target = message_parts[2];
                message.content = message_parts[3];
            }
            break;

        case MessageType::Status:
            if (message_parts.size() >= 3) {
                message.sender = message_parts[1];
                message.content = message_parts[2];
            }
            break;

        case MessageType::Info:
<<<<<<< server-responses
            if (message_parts.size() >= 3) {
                message.sender = message_parts[1];
                message.content = message_parts[2];
            }
            break;

        case MessageType::GetAll:
        case MessageType::GetUsers:
=======
        case MessageType::GetAll:
>>>>>>> main
        case MessageType::Exit:
            if (message_parts.size() >= 2) {
                message.sender = message_parts[1];
            }
            break;

        case MessageType::Unknown:
        default:
            break;
    }

    return message;
}