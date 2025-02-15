#include "../lib/microtcp.h"
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char **argv) {
    ssize_t bytes_received;
    microtcp_sock_t socket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); 
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = INADDR_ANY;

    if (microtcp_bind(&socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror("Bind failed");
        return -1;
    }
    
    while(1) {
        char buffer[1024];
        printf("Waiting to accept a client...\n");
        if (microtcp_accept(&socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            printf("Three way handshake failed.\n");
            continue; 
        }
        printf("Client Connected\n");
        if(1){
            bytes_received = microtcp_recv(&socket, buffer, sizeof(buffer), 0);
        }else{
            bytes_received = microtcp_simulate_packetLoss_recv(&socket, buffer, sizeof(buffer), 0);

        }
        if(bytes_received > 0) {
            printf("Bytes received %zd\n", bytes_received);
        }
        
        if (socket.state == FIN_RECEIVED) {
            printf("Server detected FIN from client. Calling shutdown...\n");
            microtcp_shutdown(&socket, 0); 
            printf("Shutdown sequence completed. Socket state: %d\n", socket.state);
        } else {
            printf("Connection is not in ESTABLISHED state for shutdown.\n");
        }
        break;  
    }
    return 0;
}
