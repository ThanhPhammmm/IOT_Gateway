#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

// Helper function: validate integer input
static int parse_int(const char *str, int min, int max, const char *name){
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    
    if(errno == ERANGE || val < INT_MIN || val > INT_MAX){
        fprintf(stderr, "Error: %s out of range\n", name);
        return -1;
    }
    
    if(endptr == str || *endptr != '\0'){
        fprintf(stderr, "Error: %s is not a valid number\n", name);
        return -1;
    }
    
    if(val < min || val > max){
        fprintf(stderr, "Error: %s must be between %d and %d\n", name, min, max);
        return -1;
    }
    
    return (int)val;
}

int main(int argc, char **argv){
    if(argc < 5) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <sensor_id> <sensor_type> [interval_ms] [count]\n", 
                argv[0]);
        fprintf(stderr, "  server_ip:   Server IP address\n");
        fprintf(stderr, "  port:        Server port (1-65535)\n");
        fprintf(stderr, "  sensor_id:   Sensor ID (1-255)\n");
        fprintf(stderr, "  sensor_type: 1=temp, 2=hum, 3=light\n");
        fprintf(stderr, "  interval_ms: Send interval in milliseconds (default: 2000)\n");
        fprintf(stderr, "  count:       Number of packets to send (default: 20)\n");
        return 1;
    }
    
    const char *srv = argv[1];
    
    // Validate port
    int port = parse_int(argv[2], 1, 65535, "port");
    if(port < 0) return 1;
    
    // Validate sensor ID
    int id = parse_int(argv[3], 1, 255, "sensor_id");
    if(id < 0) return 1;
    
    // Validate sensor type
    int type = parse_int(argv[4], 1, 3, "sensor_type");
    if(type < 0) return 1;
    
    // Validate optional interval
    int interval = 2000;
    if(argc > 5){
        interval = parse_int(argv[5], 100, 60000, "interval_ms");
        if(interval < 0) return 1;
    }
    
    // Validate optional count
    int count = 20;
    if(argc > 6){
        count = parse_int(argv[6], 1, 10000, "count");
        if(count < 0) return 1;
    }
    
    // Validate IP address format
    struct in_addr addr;
    if(inet_pton(AF_INET, srv, &addr) != 1){
        fprintf(stderr, "Error: Invalid IP address format: %s\n", srv);
        return 1;
    }

    printf("Starting sensor client:\n");
    printf("  Server: %s:%d\n", srv, port);
    printf("  Sensor ID: %d\n", id);
    printf("  Type: %d (%s)\n", type, type == 1 ? "Temperature" : type == 2 ? "Humidity" : "Light");
    printf("  Interval: %d ms\n", interval);
    printf("  Packets: %d\n", count);
    printf("\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("socket");
        return 1;
    }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    serv.sin_addr = addr;  // Use validated address
    
    if(connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0){
        perror("connect");
        close(sock);
        return 1;
    }
    
    printf("Connected to server!\n\n");
    
    srand(time(NULL) + id);
    
    for(int i = 0; i < count; i++){
        double val = 0.0;
        
        // Generate realistic sensor values
        if(type == 1) {
            // Temperature: 15-35°C
            val = 15.0 + (rand() % 2000) / 100.0;
        } 
        else if(type == 2){
            // Humidity: 20-90%
            val = 20.0 + (rand() % 700) / 10.0;
        } 
        else{
            // Light: 0-1000 lux
            val = (rand() % 1001);
        }
        
        char line[128];
        int len = snprintf(line, sizeof(line), "%d %d %.2f\n", id, type, val);
        
        if(len < 0 || len >= (int)sizeof(line)){
            fprintf(stderr, "Error: Line buffer too small\n");
            break;
        }
        
        ssize_t written = write(sock, line, strlen(line));
        if(written < 0){
            perror("write");
            break;
        }
        
        printf("[%d/%d] Sent: %s", i + 1, count, line);
        
        usleep(interval * 1000);
    }
    
    close(sock);
    printf("\nSensor %d disconnected\n", id);
    
    return 0;
}