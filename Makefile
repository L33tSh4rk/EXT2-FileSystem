# Configurações do compilador
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE

# Diretórios e arquivos
TARGET = bin/ext2shell
SRCS = main.c systemOp.c
OBJS = $(SRCS:.c=.o)
HEADERS = headers.h

# Regras
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)
