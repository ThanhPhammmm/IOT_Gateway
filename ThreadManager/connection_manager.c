#include "connection_manager.h"
#include "client_thread.h"
#include "logger.h"

volatile sig_atomic_t active_clients = 0;

void *connection_manager_thread(void *arg){
    int port = *(int*)arg;
    
    log_event("[CONNECTION] Connection manager thread started");
    
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        log_event("[CONNECTION] Socket creation failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0){
        log_event("[CONNECTION] setsockopt failed: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Bind socket
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    
    if(bind(server_fd, (struct sockaddr*)&server, sizeof(server)) < 0){
        log_event("[CONNECTION] Bind failed on port %d: %s", port, strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if(listen(server_fd, LISTEN_BACKLOG) < 0){
        log_event("[CONNECTION] Listen failed: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    log_event("[CONNECTION] Listening on port %d", port);
    
    size_t total_connections = 0;
    
    // Main accept loop
    while(!stop_flag){
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        struct timeval timeout = {SELECT_TIMEOUT_SEC, 0};
        int ret = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if(ret < 0){
            if(errno == EINTR) continue;
            log_event("[CONNECTION] select() error: %s", strerror(errno));
            break;
        }
        
        if(ret == 0){
            // Timeout, loop to check stop_flag
            continue;
        }
        
        // Accept new connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if(client_fd < 0){
            if(errno == EINTR) continue;
            log_event("[CONNECTION] accept() error: %s", strerror(errno));
            continue; // Try to accept next connection
        }
        
        // Allocate client info
        client_info_t *client_info = malloc(sizeof(client_info_t));
        if(!client_info){
            log_event("[CONNECTION] malloc failed for client info");
            close(client_fd);
            continue;
        }

        // Check connection limit
        if(active_clients >= MAX_CONCURRENT_CLIENTS){
            log_event("[CONNECTION] Max clients reached (%d), rejecting %s:%d", MAX_CONCURRENT_CLIENTS, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            // Send rejection message
            const char *reject_msg = "ERROR: Server full\n";
            write(client_fd, reject_msg, strlen(reject_msg));
            
            close(client_fd);
            continue;  // Skip to next accept
        }

        client_info->client_fd = client_fd;
        client_info->addr = client_addr;
        

        __sync_fetch_and_add(&active_clients, 1);

        // Create client thread
        pthread_t client_tid;
        int rc = pthread_create(&client_tid, NULL, client_thread_func, client_info);
        if(rc != 0){
            log_event("[CONNECTION] pthread_create failed: %s", strerror(rc));

            // Decrement on failure
            __sync_fetch_and_sub(&active_clients, 1);

            free(client_info);
            close(client_fd);
            continue;
        }
        
        pthread_detach(client_tid);
        total_connections++;
        
        log_event("[CONNECTION] New client connected from %s:%d (total: %zu)", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),total_connections);
    }
    
    // Cleanup
    close(server_fd);
    
    log_event("[CONNECTION] Connection manager thread exiting. Total connections: %zu", 
              total_connections);
    
    return NULL;
}