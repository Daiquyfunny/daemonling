CC = gcc
CFLAGS = -Wall -Wextra
SRC = src/main.c
TARGET = main

${TARGET} : ${SRC}
	$(CC) $(CFLAGS) ${SRC} -o ${TARGET}

clean:
	rm -f ${TARGET}
