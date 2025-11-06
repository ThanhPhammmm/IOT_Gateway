#include "utilities.h"

void sigint_handler(int sig){
    (void)sig;
    stop_flag = 1;
    pthread_cond_broadcast(&sbuffer.cond);
    // send a newline to FIFO to wake logger if needed
    int fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if(fd >= 0){
        const char *msg = "shutdown\n";
        write(fd, msg, strlen(msg));
        close(fd);
    }
}
void ensure_fifo_exists(void){
    struct stat st;
    if(stat(fifo_path, &st) == -1){
        if(mkfifo(fifo_path, 0666) == -1 && errno != EEXIST){
            perror("mkfifo");
            // non-fatal: still continue, but logging will fail
        }
    }
}