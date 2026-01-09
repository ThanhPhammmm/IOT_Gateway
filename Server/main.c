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
struct mosquitto *mosq = NULL;

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);  // Prevent SIGPIPE crashes
    
    ensure_fifo_exists();

    // Fork logger
    logger_pid = fork();
    
    if(logger_pid < 0){
        perror("fork");
        return 1;
    }
    
    if(logger_pid == 0){
        // Child: logger process
        signal(SIGINT, SIG_IGN);  // Logger ignores SIGINT
        run_logger_process();
        exit(0);
    }

    // Parent: main process
    usleep(50000); // Wait 50ms for logger to initialize
    
    sbuffer_init(&sbuffer);
    log_event("[MAIN] Gateway system started on port %d", port);
    
    int temp;
    pthread_t connection_thread, data_thread, storage_thread, cloud_thread;
    
    temp = pthread_create(&connection_thread, NULL, connection_manager_thread, &port);
    if(temp != 0){
        perror("pthread_create error");
    }

    temp = pthread_create(&data_thread, NULL, data_manager_thread, NULL);
    if(temp != 0){
        perror("pthread_create error");
    }

    temp = pthread_create(&storage_thread, NULL, storage_manager_thread, NULL);
    if(temp != 0){
        perror("pthread_create error");
    }

    temp = pthread_create(&cloud_thread, NULL, cloud_manager_thread, NULL);
    if(temp != 0){
        perror("pthread_create error");
    }

    temp = pthread_join(connection_thread, NULL);
    if(temp != 0){
        perror("pthread_join error");
    }

    temp = pthread_join(data_thread, NULL);
    if(temp != 0){
        perror("pthread_join error");
    }

    temp = pthread_join(storage_thread, NULL);
    if(temp != 0){
        perror("pthread_join error");
    }
    
    temp = pthread_join(cloud_thread, NULL);
    if(temp != 0){
        perror("pthread_join error");
    }
    
    sbuffer_free_all(&sbuffer);
    stats_free_all();

    // Log BEFORE shutting down logger
    // printf("[MAIN] Gateway shutdown complete");
    // log_event("[MAIN] Gateway shutdown complete");
    
    // // Give logger time to flush
    // usleep(100000); // 100ms
    
    // // Graceful logger shutdown with timeout
    // if(logger_pid > 0){
    //     log_event("[MAIN] Shutting down logger process (PID %d)", logger_pid);
    //     printf("[MAIN] Shutting down logger process (PID %d)", logger_pid);

    //     // Send SIGTERM
    //     kill(logger_pid, SIGTERM);
        
    //     // Wait with timeout
    //     int status;
    //     int timeout_count = 0;
    //     const int MAX_TIMEOUT = 10; // 10 iterations = 1 second
        
    //     while(timeout_count < MAX_TIMEOUT){
    //         pid_t result = waitpid(logger_pid, &status, WNOHANG);
            
    //         if(result > 0){
    //             // Logger exited normally
    //             if(WIFEXITED(status)){
    //                 printf("[MAIN] Logger exited with status %d\n", WEXITSTATUS(status));
    //             } 
    //             else if(WIFSIGNALED(status)){
    //                 printf("[MAIN] Logger killed by signal %d\n", WTERMSIG(status));
    //             }
    //             break;
    //         } 
    //         else if(result < 0){
    //             // Error
    //             perror("waitpid");
    //             break;
    //         }
            
    //         // Still running, wait more
    //         usleep(100000); // 100ms
    //         timeout_count++;
    //     }
        
    //     // If still running after timeout, force kill
    //     if(timeout_count >= MAX_TIMEOUT){
    //         printf ("[MAIN] Logger not responding, sending SIGKILL\n");
    //         kill(logger_pid, SIGKILL);
    //         waitpid(logger_pid, NULL, 0); // Wait for force kill
    //     }
    // }

    printf("[MAIN] Gateway shutdowns completely\n");
    log_event("[MAIN] Gateway shutdowns completely");

    // Close FIFO writer → EOF
    close_logger_process();

    // Wait logger exit naturally
    if(logger_pid > 0){
        waitpid(logger_pid, NULL, 0);
    }

    return 0;
}