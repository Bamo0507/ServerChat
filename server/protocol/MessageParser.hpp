#ifndef MESSAGE_PARSER_HPP
#define MESSAGE_PARSER_HPP

#include <string>
#include "../models/Message.hpp"

// MessageParser se encarga de convertir un mensaje crudo recibido
// por socket en una estructura Message que el servidor pueda procesar.
class MessageParser {
public:
    static Message parse(const std::string& raw_message);
};

#endif