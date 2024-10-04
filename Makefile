CC=g++
CFLAGS=-Wall -Werror -Wno-error=unused-variable -g

build: server subscriber

server: server.cpp common.cpp
	$(CC) -o $@ $^ $(CFLAGS)

subscriber: subscriber.cpp common.cpp
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf server subscriber

pack:
	zip 324CA_Drinciu_Cristina_Tema2.zip server.cpp  subscriber.cpp common.cpp common.h Makefile readme.txt