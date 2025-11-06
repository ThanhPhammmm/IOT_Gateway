#include "utilities.h"

void sigint_handler(int sig){
    (void)sig;
    stop_flag = 1;
    pthread_cond_broadcast(&sbuffer.cond);

    int fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        static const char msg[] = "shutdown\n";
        ssize_t n = write(fd, msg, sizeof(msg) - 1);
        if (n < 0) {
            perror("[SIGINT] write to FIFO failed");
        }
        close(fd);
    } else {
        perror("[SIGINT] open FIFO failed");
    }

    fprintf(stderr, "\n[MAIN] SIGINT received — shutting down gracefully...\n");
}
void ensure_fifo_exists(void){
    struct stat st;

    if (stat(fifo_path, &st) == -1) {
        if (mkfifo(fifo_path, 0666) == -1 && errno != EEXIST) {
            perror("mkfifo");
        }
    } 
    else if (!S_ISFIFO(st.st_mode)) {
        fprintf(stderr, "Warning: %s exists but is not a FIFO\n", fifo_path);
    }
}
