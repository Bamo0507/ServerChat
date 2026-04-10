#include <cstdlib>
#include <iostream>

#include "core/ChatServer.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usar: ./chat_server <puerto>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);

    if (port <= 0) {
        std::cerr << "Error: el puerto proporcionado no es válido." << std::endl;
        return 1;
    }

    ChatServer chat_server(port);

    if (!chat_server.start()) {
        std::cerr << "Error: No se pudo iniciar el servidor en el puerto " << port << std::endl;
        return 1;
    }

    std::cout << "Servidor iniciado en el puerto " << port << std::endl;

    chat_server.run();

    return 0;
}