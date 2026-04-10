## Instrucciones

### Cliente

Al decidir correr el `Client`, asegurarse de ejecutar:

```
cmake -S . -B build
cmake --build build
./build/chat_cli
```

### Servidor

Para correr el servidor, ubicarse dentro de la carpeta `server` y ejecutar:

```
cmake -S . -B build
cmake --build build
./build/chat_server <puerto>
```

Ejemplo:

```
./build/chat_server 5050
```

---

## Puntos a tomar en cuenta

El archivo `CMakeLists.txt` define cómo se construyen los ejecutables del proyecto.

- Permite descargar y configurar librerías necesarias (como FTXUI para la interfaz).
- Define los archivos `.cpp` que forman parte del ejecutable.

Es importante que todos los archivos con lógica del chat se incluyan dentro de `add_executable`.

---

## Estado actual del servidor

Actualmente, el servidor implementa una base funcional que permite:

- Crear un socket
- Asociarlo a un puerto (`bind`)
- Entrar en modo escucha (`listen`)
- Aceptar conexiones entrantes (`accept`)
- Recibir un mensaje desde el cliente (`recv`)
- Mostrar el mensaje recibido en consola

### Mensajes soportados (fase actual)

Se pueden enviar mensajes de prueba con formato:

```
CHAT|<usuario>|<mensaje>
```

Ejemplo:

```
CHAT|Bryan|Hola
```

---

## Decisiones de conectividad

Inicialmente, el proyecto fue planteado para ejecutarse en una infraestructura local utilizando un switch y cables RJ45, con una máquina actuando como servidor y las demás como clientes.

Durante el desarrollo, se propuso y fue validada la posibilidad de realizar la ejecución del proyecto utilizando conectividad inalámbrica o acceso remoto (por ejemplo, mediante exposición del servidor).

Este ajuste no modifica la arquitectura base del proyecto:

- Se mantiene un único servidor.
- Los clientes continúan conectándose a dicho servidor.
- La comunicación sigue realizándose mediante sockets.

Por lo tanto, la diferencia principal no está en la lógica del sistema, sino en el medio utilizado para establecer la conectividad entre las máquinas durante las pruebas.
