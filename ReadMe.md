## Instrucciones

Al decidir correr el `Client`, asegurarse de correr utilizando lo siguiente:

```
cmake -S . -B build
cmake --build build
./build/chat_cli
```

## Puntos a tomar en cuenta

El archivo de CMakeLists.txt estaba en la documentación para poder utilizar la librería que se está empleando para la interfaz. La idea es de que aquí se tienen las instrucciones para poder descargarla y trabajar con ella, pero también se deben declarar los ejecutables con los archivos con los que se va a trabajar. Deben estar acá en un apartado que se llama **add_executable**, aquí van todos los puntos cpp que se utilizan que involucren alguna lógica para el chat.



