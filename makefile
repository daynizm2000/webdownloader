TARGET = webd

CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lssl -lcrypto

SRCS = main.c $(wildcard src/*.c)


all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)


clean:
	rm -f $(TARGET)

.PHONY: all clean
