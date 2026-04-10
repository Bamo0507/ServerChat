#ifndef CHAT_SERVER_HPP
#define CHAT_SERVER_HPP

#include <string>

class ChatServer {
public:
    // Constructor del servidor.
    // Recibe el puerto en el que se espera iniciar la escucha de conexiones.
    explicit ChatServer(int port);

    // Inicializa el servidor.
    // Aquí se crea el socket principal, se hace bind y se deja listo para escuchar.
    bool start();

    // Ciclo principal del servidor.
    // Aquí se aceptan conexiones entrantes y se procesan los mensajes recibidos.
    void run();

    // Detiene el servidor y libera los recursos asociados.
    void stop();

private:
    // Puerto en el que el servidor estará escuchando conexiones entrantes.
    int server_port;

    // File descriptor del socket principal del servidor.
    int server_socket_file_descriptor;

    // Procesa el mensaje recibido del cliente y construye la respuesta.
    std::string handleClientMessage(const std::string& raw_message);
};

#endif