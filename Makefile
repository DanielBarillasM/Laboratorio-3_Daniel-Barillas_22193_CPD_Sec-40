# Laboratorio 3 - compilación en WSL con Open MPI.
MPICC ?= mpicc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
BUILD_DIR := build
SOURCES := src/01_ping_pong.c src/02_token_ring.c \
           src/03_recepcion_anticipada.c src/04_pipeline_chunks.c
PROGRAMS := $(BUILD_DIR)/ping_pong $(BUILD_DIR)/token_ring \
            $(BUILD_DIR)/recepcion_anticipada $(BUILD_DIR)/pipeline_chunks

.PHONY: all clean
all: $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/ping_pong: src/01_ping_pong.c src/common.h src/visual.h | $(BUILD_DIR)
	$(MPICC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/token_ring: src/02_token_ring.c src/common.h src/visual.h | $(BUILD_DIR)
	$(MPICC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/recepcion_anticipada: src/03_recepcion_anticipada.c src/common.h src/visual.h | $(BUILD_DIR)
	$(MPICC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/pipeline_chunks: src/04_pipeline_chunks.c src/common.h src/visual.h | $(BUILD_DIR)
	$(MPICC) $(CFLAGS) $< -o $@

clean:
	rm -f $(PROGRAMS)
