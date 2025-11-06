#include "main.h"
#include "connection_manager.h"
#include "client_thread.h"
#include "logger.h"
#include "sbuffer.h"
#include "utilities.h"
#include "data_manager.h"
#include "storage_manager.h"
#include "cloud_manager.h"

pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
const char *fifo_path = FIFO_PATH;
volatile sig_atomic_t stop_flag = 0;
sbuffer_t sbuffer;
sensor_stat_t *stats_head = NULL;
pid_t logger_pid = 0;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

cloud_client_t clients[] = {
    {1, "bcVWopy6l9cfHxDQBXd4", NULL, 0},
    {2, "H1KOvekgc0xEYacv3DyI", NULL, 0},
    {3, "rIDas8QcUC7Oc1nAqfQw", NULL, 0},
};

struct mosquitto *mosq = NULL;

int main(int argc, char ** argv){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    signal(SIGINT, sigint_handler);
    ensure_fifo_exists();

    // fork logger
    logger_pid = fork();
    
    if(logger_pid < 0){
        perror("fork");
        return 1;
    }
    else if(logger_pid == 0){
        // child: logger process
        run_logger_process();
        exit(0);
    }

    sbuffer_init(&sbuffer);
    printf("[MAIN] Started System ...\n");
    pthread_t connection_thread, data_thread, storage_thread, cloud_thread;
    
    pthread_create(&connection_thread, NULL, connection_manager_thread, &port);
    pthread_create(&data_thread, NULL, data_manager_thread, NULL);
    pthread_create(&storage_thread, NULL, storage_manager_thread, NULL);
    pthread_create(&cloud_thread, NULL, cloud_manager_thread, NULL);

    pthread_join(connection_thread, NULL);
    pthread_join(data_thread, NULL);
    pthread_join(storage_thread, NULL);
    pthread_join(cloud_thread, NULL);
    
    sbuffer_free_all(&sbuffer);
    stats_free_all();

    stop_flag = 1;

    kill(logger_pid, SIGTERM);
    waitpid(logger_pid, NULL, 0);

    log_event("Gateway shutdown cleanly.\n");

    return 0;
}