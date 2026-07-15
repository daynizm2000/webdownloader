TARGET = webd

# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lssl -lcrypto

# Поиск всех файлов исходного кода
SRCS = main.c $(wildcard src/*.c)

# Главная цель: компилируем всё одной командой без промежуточного мусора
all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

# Очистка только самой программы
clean:
	rm -f $(TARGET)

.PHONY: all clean