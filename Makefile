# ==========================
#        COMPILER
# ==========================
CC      = gcc
CFLAGS  = -Wall -Wextra -pthread \
          -I. -IClient -ICloud -ICommon -IDatabase -ILogger -IServer -IThreadManager

LDFLAGS_MAIN = -lsqlite3 -lmosquitto

# ==========================
#     OUTPUT DIRECTORY
# ==========================
BINDIR  = RunProgram
$(shell mkdir -p $(BINDIR))

# ==========================
#     SOURCE COLLECTION
# ==========================

# Main executable includes all modules (except client.c)
SRCS_MAIN = \
    $(wildcard Server/*.c) \
    $(wildcard ThreadManager/*.c) \
    $(wildcard Common/*.c) \
    $(wildcard Logger/*.c) \
    $(wildcard Database/*.c) \
    $(wildcard Cloud/*.c)

# Client executable
SRCS_CLIENT = Client/client.c

# Output binaries
TARGET_MAIN   = $(BINDIR)/main_process
TARGET_CLIENT = $(BINDIR)/client

# ==========================
#          BUILD
# ==========================
all: $(TARGET_MAIN) $(TARGET_CLIENT)

$(TARGET_MAIN): $(SRCS_MAIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_MAIN)

$(TARGET_CLIENT): $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

# ==========================
#          CLEAN
# ==========================
clean:
	rm -f $(TARGET_MAIN) $(TARGET_CLIENT)
	rm -f */*.o *.o

re: clean all

# ==========================
#    BEAGLEBONE DEPLOY
# ==========================
BBB_USER = debian
BBB_IP   = 192.168.7.2
BBB_PATH = /home/debian/IOT_GATEWAY

send:
	@echo ">>> Copying source code to BBB..."
	scp -r \
		Client Cloud Common Database Logger Server ThreadManager \
		*.c *.h *.md *.db MakefileBBB \
		$(BBB_USER)@$(BBB_IP):$(BBB_PATH)
	@echo ">>> Done!"

deploy: all send
	@echo ">>> Build + Deploy completed!"

.PHONY: all clean re send deploy

