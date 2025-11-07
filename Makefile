# === Compiler and flags ===
CC       = gcc
CFLAGS   = -Wall -Wextra -pthread -g

# === Source files ===
SRCS_MAIN   = main.c utilities.c connection_manager.c data_manager.c \
              storage_manager.c cloud_manager.c cloud_uploader.c \
              database.c logger.c client_thread.c sbuffer.c

SRCS_CLIENT = client.c

# === Output binaries ===
TARGET_MAIN = main_process
TARGET_CLIENT = client

# === Default target ===
all: $(TARGET_MAIN) $(TARGET_CLIENT)

# === Compile for x86 (local PC) ===
$(TARGET_MAIN): $(SRCS_MAIN)
	$(CC) $(CFLAGS) -o $@ $^ -lsqlite3 -lmosquitto

$(TARGET_CLIENT): $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

# === Clean up ===
clean:
	rm -f *.o $(TARGET_MAIN) $(TARGET_CLIENT) $(TARGET_BBB)

# === Rebuild everything ===
re: clean all

.PHONY: all clean re

