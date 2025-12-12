#include "utilities.h"

void sigint_handler(int sig){
    (void)sig;
    stop_flag = 1;
    pthread_cond_broadcast(&sbuffer.cond);

    int fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if(fd >= 0){
        static const char msg[] = "shutdown\n";
        write(fd, msg, sizeof(msg) - 1); // Ignore errors in signal handler
        close(fd);
    }

    // Use write() instead of fprintf in signal handler (async-signal-safe)
    static const char shutdown_msg[] = "\n[MAIN] SIGINT received — shutting down gracefully...\n";
    write(STDERR_FILENO, shutdown_msg, sizeof(shutdown_msg) - 1);
}

void ensure_fifo_exists(void){
    struct stat st;

    if(stat(fifo_path, &st) == 0){
        // File exists - check if it's a FIFO
        if(!S_ISFIFO(st.st_mode)){
            static const char warn[] = "Warning: FIFO path exists but is not a FIFO\n";
            write(STDERR_FILENO, warn, sizeof(warn) - 1);
        }
        return;
    }

    // File doesn't exist - create FIFO
    if(errno == ENOENT){
        if(mkfifo(fifo_path, 0666) == -1 && errno != EEXIST){
            perror("mkfifo");
        }
    }
}