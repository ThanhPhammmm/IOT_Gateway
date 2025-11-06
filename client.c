#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

int main(int argc, char **argv){
    if(argc < 5) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <sensor_id> <sensor_type> [interval_ms] [count]\n", argv[0]);
        fprintf(stderr, "sensor_type: 1=temp, 2=hum, 3=light\n");
        return 1;
    }
    const char *srv = argv[1];
    int port = atoi(argv[2]);
    int id = atoi(argv[3]);
    int type = atoi(argv[4]);
    int interval = (argc > 5) ? atoi(argv[5]) : 2000;
    int count = (argc > 6) ? atoi(argv[6]) : 20;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("socket");
        return 1;
    }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);

    if(inet_pton(AF_INET, srv, &serv.sin_addr) <= 0){
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    if(connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    printf("Sensor %d type %d connected to %s:%d\n", id, type, srv, port);
    srand(time(NULL) + id);
    
    for(int i = 0; i < count; i++) {
        double val = 0.0;
        if(type == 1) {
            val = 20.0 + (rand() % 1000) / 100.0; // 20 +-10
        } else if(type == 2) {
            val = 40.0 + (rand() % 400) / 10.0;
        } else {
            val = 100.0 + (rand() % 900);
        }
        char line[128];
        snprintf(line, sizeof(line), "%d %d %.2f\n", id, type, val);
        if(write(sock, line, strlen(line)) < 0) {
            perror("write");
            break;
        }
        printf("Sent: %s", line);
        usleep(interval * 1000);
    }
    close(sock);
    printf("Sensor %d disconnected\n", id);
    return 0;
}
