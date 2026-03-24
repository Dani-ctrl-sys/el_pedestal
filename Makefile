# ==============================================================================
# Makefile — el_pedestal
# Proyecto de criptografía en C99
# ==============================================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -Wpedantic -O2 -std=c99 -I./inc

# Directorios
SRC_DIR   := src
INC_DIR   := inc
TEST_DIR  := tests
BUILD_DIR := build

# Fuentes y objetos de la biblioteca principal
SRCS    := $(wildcard $(SRC_DIR)/*.c)
OBJS    := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Ejecutable de pruebas
TEST_SRC  := $(TEST_DIR)/test_arith.c
TEST_BIN  := $(BUILD_DIR)/test_arith

# ==============================================================================
# Reglas
# ==============================================================================

.PHONY: all test clean

## all: compila los objetos de src/
all: $(OBJS)

## Construcción de archivos objeto en build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

## test: compila src/ + tests/test_arith.c y ejecuta el binario
test: $(OBJS) $(TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_SRC) -o $(TEST_BIN)
	@echo "==> Ejecutando pruebas..."
	./$(TEST_BIN)

## Crea el directorio build/ si no existe
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

## clean: elimina artefactos generados
clean:
	rm -rf $(BUILD_DIR)
