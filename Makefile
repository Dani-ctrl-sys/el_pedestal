# ==============================================================================
# Makefile — el_pedestal
# Proyecto de criptografía en C99
# ==============================================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -Wpedantic -O2 -std=c99 -I./inc

ifdef DILITHIUM_MODE
CFLAGS += -DDILITHIUM_MODE=$(DILITHIUM_MODE)
endif

# Directorios
SRC_DIR   := src
INC_DIR   := inc
TEST_DIR  := tests
BUILD_DIR := build

# Fuentes y objetos de la biblioteca principal
SRCS    := $(wildcard $(SRC_DIR)/*.c)
OBJS    := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Ejecutables de pruebas
TEST_ARITH_SRC := $(TEST_DIR)/test_arith.c
TEST_ARITH_BIN := $(BUILD_DIR)/test_arith

TEST_HASH_SRC  := $(TEST_DIR)/test_hash.c
TEST_HASH_BIN  := $(BUILD_DIR)/test_hash

TEST_POLY_SRC  := $(TEST_DIR)/test_poly.c
TEST_POLY_BIN  := $(BUILD_DIR)/test_poly

# ==============================================================================
# Reglas
# ==============================================================================

.PHONY: all clean test_arith test_hash test_poly

## all: compila los objetos de src/
all: $(OBJS)

## Construcción de archivos objeto en build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test_arith: $(OBJS) $(TEST_ARITH_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_ARITH_SRC) -o $(TEST_ARITH_BIN)
	@echo "==> Ejecutando pruebas Aritmeticas..."
	./$(TEST_ARITH_BIN)

test_hash: $(OBJS) $(TEST_HASH_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_HASH_SRC) -o $(TEST_HASH_BIN)
	@echo \"==> Ejecutando auditoria Keccak...\"
	./$(TEST_HASH_BIN)

test_poly: $(OBJS) $(TEST_POLY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_POLY_SRC) -o $(TEST_POLY_BIN)
	@echo \"==> Ejecutando test de Polinomios Uniformes...\"
	./$(TEST_POLY_BIN)

## Crea el directorio build/ si no existe
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

## clean: elimina artefactos generados
clean:
	rm -rf $(BUILD_DIR)
