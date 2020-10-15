# Lab-Cliente-Servidor-HTTP
Laboratório: Implementando Cliente e Servidor HTTP

## Prerequisites
- premake5
- [Python pwntools](https://github.com/Gallopsled/pwntools), apenas para testes mais complexos

## Installation


```bash
premake5 --file=scripts/premake5.lua gmake2
make -C build/ config=debug_linux64 
```

# Usage

Create desired files inside desired folder, in examples was used www/ folder

### Server

```bash
cd build/bin/Linux64/Debug
./server 0.0.0.0 8000 .
```

### Client

```bash
telnet 127.0.0.1 3490
GET /www/index.html HTTP/1.1\r\n\r\n
```
or

```bash
./client http://localhost:8000/www/index.html http://localhost:8000/www/index2.html
```

or using pwntools, file path max size in Linux systems is 4096 bytes, max tcp size is 64Kb(64 * 1024)
```bash
ipython
from pwn import *
request = "GET " + "/" +"www/../www/../www/../" * 190 + "www/index.html" + ' HTTP/1.1\r\n' + "B" * 2 * 64 * 1024  + '\r\n\r\n'
r = remote('127.0.0.1', 8000)
r.send(request)
r.interactive()
```

## Additional tests
[OWASP](www.owasp.org) site was clone using httrack tool with depht value equal 2, and was saved on www/owasp folder

### Server

Alternatively you can use the links to executables that premake5 create on main folder.

```bash
./server 0.0.0.0 8000 www/owasp
```

![local owasp](misc/owasp.png)


## License
[MIT](https://choosealicense.com/licenses/mit/)
