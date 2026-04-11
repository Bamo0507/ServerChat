#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>

// MessageType permite clasificar de forma explícita el tipo de interacción
// que el cliente está intentando realizar con el servidor.
// Esto facilita el parsing, la validación y el enrutamiento de cada solicitud.
enum class MessageType {
    Register,
    ChatPublic,
    ChatPrivate,
    Status,
    Info,
    GetAll,
<<<<<<< server-responses
    GetUsers,
=======
>>>>>>> main
    Exit,
    Unknown
};

// Message representa una solicitud ya interpretada por el servidor.
// No todos los campos se utilizan en todos los tipos de mensaje,
// por lo que algunos pueden permanecer vacíos según la interacción.
struct Message {
    MessageType type = MessageType::Unknown;
    std::string sender = "";
    std::string target = "";
    std::string content = "";
};

#endif