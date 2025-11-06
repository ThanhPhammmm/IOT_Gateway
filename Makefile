# === Compiler and flags ===
CC       = gcc
ARM_CC   = /usr/bin/arm-linux-gnueabihf-gcc
CFLAGS   = -Wall -Wextra -pthread -g

# === Paths ===
SYSROOT = /home/thanhpham25/PersonalProject/IOT_Gateway/main_project/bbb_sysroot

# Compiler flags
CFLAGS_ARM = -Wall -Wextra -pthread -g --sysroot=$(SYSROOT)
LDFLAGS_ARM = --sysroot=$(SYSROOT) \
          -L$(SYSROOT)/lib/arm-linux-gnueabihf \
          -L$(SYSROOT)/usr/lib/arm-linux-gnueabihf \
          -Wl,-rpath-link,$(SYSROOT)/lib/arm-linux-gnueabihf \
          -Wl,-rpath-link,$(SYSROOT)/usr/lib/arm-linux-gnueabihf
LIBS = -lsqlite3 -lmosquitto

# === Source files ===
SRCS_MAIN   = main.c utilities.c connection_manager.c data_manager.c \
              storage_manager.c cloud_manager.c cloud_uploader.c \
              database.c logger.c client_thread.c sbuffer.c

SRCS_CLIENT = client.c

# === Output binaries ===
TARGET_MAIN = main_process
TARGET_CLIENT = client
TARGET_BBB = bbb_gateway

# === Default target ===
all: $(TARGET_MAIN) $(TARGET_CLIENT)

# === Compile for x86 (local PC) ===
$(TARGET_MAIN): $(SRCS_MAIN)
	$(CC) $(CFLAGS) -o $@ $^ -lsqlite3 -lmosquitto

$(TARGET_CLIENT): $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

# === Cross compile for BeagleBone Black ===
$(TARGET_BBB): $(SRCS_MAIN)
	$(ARM_CC) $(CFLAGS_ARM) $(LDFLAGS_ARM) -o $@ $^ $(LIBS)

# === Clean up ===
clean:
	rm -f *.o $(TARGET_MAIN) $(TARGET_CLIENT) $(TARGET_BBB)

# === Rebuild everything ===
re: clean all

.PHONY: all clean re

