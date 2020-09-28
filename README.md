# Lab-Cliente-Servidor-HTTP
Laboratório: Implementando Cliente e Servidor HTTP

## Prerequisites
- premake5

## Installation

```bash
premake5 --file=scripts/premake5.lua gmake2
make -C build/ config=debug_linux64
```

# Usage

### Server

```bash
cd build/bin/Linux64/Debug
./server
```

### Client

```bash
telnet 127.0.0.1 3490
GET /www/index.html HTTP/1.1\r\n\r\n
```

## License
[MIT](https://choosealicense.com/licenses/mit/)
