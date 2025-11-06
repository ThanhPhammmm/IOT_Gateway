#include "logger.h"
#include "utilities.h"

void log_event(const char *fmt, ...){
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&log_mutex);
    int fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if (fd == -1) {
        // if FIFO can't be opened, print to stderr as fallback
        // (this can happen during startup race if logger not yet open)
        fprintf(stderr, "log fallback: %s\n", buf);
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    //printf("fifo: %s\n",buf);
    size_t len = strlen(buf);
    buf[len] = '\n';
    if (write(fd, buf, len + 1) < 0) {
        perror("write fifo");
    }
    // note: we do not append newline here; logger will treat messages as chunks
    close(fd);
    pthread_mutex_unlock(&log_mutex);
}

void run_logger_process() {
    // open FIFO for reading; if no writer yet, open blocks until writer opens.
    // We'll open in blocking mode and handle EOF by reopening.
    FILE *logf = fopen(LOG_FILE, "a");
    if (!logf) {
        perror("fopen gateway.log");
        exit(EXIT_FAILURE);
    }
    int seq = 0;
    while (!stop_flag){
        int fd = open(fifo_path, O_RDONLY);
        if (fd == -1) {
            // maybe FIFO not present; sleep and retry
            fprintf(stderr, "logger process failed\n");
            sleep(1);
            continue;
        }
        // read loop
        char buf[1024];
        ssize_t r;
        while ((r = read(fd, buf, sizeof(buf)-1)) > 0) {
            buf[r] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                time_t now = time(NULL);
                struct tm tm;
                localtime_r(&now, &tm);
                char timestr[64];
                strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);
                fprintf(logf, "%d %s %s\n", seq++, timestr, line);
                fflush(logf);
                line = strtok(NULL, "\n");
            }
        }
        close(fd);
        // read returned 0 => all writers closed; loop to reopen
    }
    fclose(logf);
    exit(0);
}