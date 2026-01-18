#include "logger.h"
#include "utilities.h"

static int log_fd = -1;

void log_event(const char *fmt, ...){
    char buf[1024];
    va_list ap;

    pid_t tid = syscall(SYS_gettid);
    int len = snprintf(buf, sizeof(buf) - 1, "[TID:%d] ", tid);

    if(len < 0 || len >= (int)sizeof(buf)){
        len = 0;  // Overflow, abort
        return;
    }

    size_t remaining = sizeof(buf) - len - 1;

    va_start(ap, fmt);
    int msg_len = vsnprintf(buf + len, remaining, fmt, ap);
    va_end(ap);

    if(msg_len < 0) return;

    // Ensure having enough space for newline
    len += (msg_len < (int)remaining) ? msg_len : (int)remaining - 1;
    buf[len++] = '\n';

    pthread_mutex_lock(&log_mutex);

    // Lazy open FIFO
    if(log_fd == -1){
        log_fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
        if(log_fd == -1){
            pthread_mutex_unlock(&log_mutex);
            write(STDERR_FILENO, "log fallback: ", 14);
            write(STDERR_FILENO, buf, len);
            return;
        }
    }

    ssize_t w = write(log_fd, buf, len);
    if(w < 0 && (errno == EPIPE || errno == ENXIO)){
        close(log_fd);
        log_fd = -1;
        pthread_mutex_unlock(&log_mutex);
        write(STDERR_FILENO, "log fallback: ", 14);
        write(STDERR_FILENO, buf, len);
        return;
    }

    pthread_mutex_unlock(&log_mutex);
}

void run_logger_process(){
    FILE *logf = fopen(LOG_FILE, "a");
    if(!logf){
        perror("fopen gateway.log");
        exit(EXIT_FAILURE);
    }
    
    setvbuf(logf, NULL, _IOLBF, 0);
    
    int seq = 0;
    char buf[4096];
    static char leftover[256] = {0}; // Buffer for incomplete lines
    size_t leftover_len = 0;
    
    int fd = open(fifo_path, O_RDONLY);
    if(fd == -1){
        perror("open fifo");
        fclose(logf);
        exit(EXIT_FAILURE);
    }

    ssize_t r;
    // while(!stop_flag){
        // int fd = open(fifo_path, O_RDONLY);
        // if(fd == -1){
        //     sleep(1);
        //     continue;
        // }

        // ssize_t r;
    while((r = read(fd, buf, sizeof(buf) - 1)) > 0){
        buf[r] = '\0';
        
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char timestr[32];
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);
        
        char *start = buf;
        char *end = buf + r;
        
        // Process leftover from previous read
        if(leftover_len > 0){
            char *newline = memchr(start, '\n', end - start);
            if(newline){
                *newline = '\0';
                fprintf(logf, "%d %s %s%s\n", seq++, timestr, leftover, start);

                // Modify fprintf:
                // pid_t tid = syscall(SYS_gettid);  // Get current thread ID
                // fprintf(logf, "%d %s [TID:%d] %s%s\n", seq++, timestr, tid, leftover, start);

                start = newline + 1;
                leftover_len = 0;
            } 
            else{
                // Still no complete line, append to leftover
                size_t copy_len = (sizeof(leftover) - leftover_len - 1 < (size_t)r) ? sizeof(leftover) - leftover_len - 1 : (size_t)r;
                memcpy(leftover + leftover_len, start, copy_len);
                leftover_len += copy_len;
                leftover[leftover_len] = '\0';
                continue;
            }
        }
        
        // Process complete lines
        while(start < end){
            char *newline = memchr(start, '\n', end - start);
            if(newline){
                *newline = '\0';
                if(start != newline){ // Skip empty lines
                    fprintf(logf, "%d %s %s\n", seq++, timestr, start);
                }
                start = newline + 1;
            } 
            else{
                // Incomplete line - save to leftover
                leftover_len = end - start;
                if(leftover_len >= sizeof(leftover)){
                    leftover_len = sizeof(leftover) - 1;
                }
                memcpy(leftover, start, leftover_len);
                leftover[leftover_len] = '\0';
                break;
            }
        }
    }
    
    /* read() exited */
    if(r == 0){ // FIFO EOF
        // Flush any remaining leftover when writer closes
        if(leftover_len > 0){
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            char timestr[32];
            strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);
            fprintf(logf, "%d %s %s\n", seq++, timestr, leftover);
            leftover_len = 0;
        }
        printf("[LOGGER] Logger shutdowns completely\n");
    }
    else{
        perror("read fifo");
    }

    close(fd);
    // }

    fflush(logf);
    fclose(logf);

    exit(0);
}

void close_logger_process(void){
    pthread_mutex_lock(&log_mutex);
    if(log_fd != -1){
        close(log_fd); // EOF
        log_fd = -1;
        printf("[LOGGER] Logger is shutdowning\n");
    }
    pthread_mutex_unlock(&log_mutex);
}