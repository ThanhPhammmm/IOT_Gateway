#include "connection_manager.h"
#include "client_thread.h"

void *connection_manager_thread(void *arg){
    printf("Connection manager thread started\n");

    int port = *(int*)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    socklen_t servlen = sizeof(server);

    if(bind(server_fd, (struct sockaddr*)&server, servlen) < 0){
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if(listen(server_fd, 16) < 0){
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Connection manager: listening on port %d\n", port);

    while(!stop_flag){
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);

        struct timeval tv = {1, 0}; // timeout 1s
        int ret = select(server_fd + 1, &fds, NULL, NULL, &tv);
        if(ret < 0){
            if(errno == EINTR) continue;
            perror("select");
            break;
        }
        else if(ret == 0){
            continue; // timeout, check again the stop_flag
        }
        struct sockaddr_in client;
        socklen_t clilen = sizeof(client);
        int cfd = accept(server_fd, (struct sockaddr*)&client, &clilen);
        if(cfd < 0){
            if(errno == EINTR) continue;
            perror("accept");
            break;
        }
        client_info_t *ci = malloc(sizeof(*ci));
        ci->client_fd = cfd;
        ci->addr = client;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread_func, ci);
        pthread_detach(tid);
    }

    close(server_fd);
    printf("Connection manager thread exiting\n");
    return NULL;
}