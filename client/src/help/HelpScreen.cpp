#include "HelpScreen.hpp"

#include <string>
#include <vector>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace {
    Element BuildCommandRow(
        const std::string& command,
        const std::string& description,
        const std::string& example
    ) {
        return vbox({
            text(command) | bold,
            text("Descripción: " + description),
            text("Ejemplo: " + example),
        });
    }
}

Element HelpScreen(HelpOrigin origin) {
    Elements command_elements;

    if (origin == HelpOrigin::General) {
        command_elements.push_back(BuildCommandRow(
            "/chat",
            "Envía un mensaje al chat general.",
            "/chat hola a todos"
        ));

        command_elements.push_back(separator());

        command_elements.push_back(BuildCommandRow(
            "/status",
            "Cambia tu estado entre activo, ocupado o inactivo.",
            "/status activo"
        ));

        command_elements.push_back(separator());

        command_elements.push_back(BuildCommandRow(
            "/info",
            "Consulta la información de un usuario.",
            "/info bryan"
        ));

        command_elements.push_back(separator());

        command_elements.push_back(BuildCommandRow(
            "/private",
            "Abre una conversación privada con un usuario.",
            "/private adriana"
        ));

        command_elements.push_back(separator());

        command_elements.push_back(BuildCommandRow(
            "/help",
            "Muestra esta pantalla de ayuda.",
            "/help"
        ));

        command_elements.push_back(separator());

        command_elements.push_back(BuildCommandRow(
            "/exit",
            "Sale completamente del chat.",
            "/exit"
        ));
    } else {
        command_elements.push_back(BuildCommandRow(
            "/chat",
            "Envía un mensaje al usuario de la conversación privada.",
            "/chat hola, ¿cómo estás?"
        ));

        command_elements.push_back(separator());

        command_elements.push_back(BuildCommandRow(
            "/return",
            "Regresa al chat general.",
            "/return"
        ));

        command_elements.push_back(separator());

        command_elements.push_back(BuildCommandRow(
            "/help",
            "Muestra esta pantalla de ayuda.",
            "/help"
        ));
    }

    std::string subtitle =
        (origin == HelpOrigin::General)
            ? "Comandos disponibles - Chat general"
            : "Comandos disponibles - Chat privado";

    Element header = vbox({
        filler(),
        text("AYUDA") | bold | hcenter,
        text(subtitle) | hcenter,
        filler(),
    });

    Element commands_box = vbox(std::move(command_elements)) | yflex;

    Element footer = vbox({
        filler(),
        text("Presione Enter para regresar") | bold | hcenter,
        filler(),
    });

    return vbox({
        header | border | size(HEIGHT, EQUAL, 5),
        commands_box | border | yflex,
        footer | border | size(HEIGHT, EQUAL, 5),
    }) | yflex;
}