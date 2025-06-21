# Configurações do compilador
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE

# Diretórios e arquivos
TARGET_DIR = bin
TARGET = $(TARGET_DIR)/ext2shell
SRCS = main.c systemOp.c commands.c
OBJS = $(SRCS:.c=.o)
HEADERS = headers.h commands.h

# Regras
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(TARGET_DIR)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	# Remove os arquivos objeto da raiz e o executável de dentro de /bin
	rm -f $(OBJS) $(TARGET)