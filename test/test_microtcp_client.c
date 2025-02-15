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
    ssize_t bytes_sent;
    microtcp_sock_t socket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = INADDR_ANY;

    printf("Connecting to server...\n");
    if (microtcp_connect(&socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        printf("ERROR: Connection not established. Exiting...\n");
        return -1;
    }
    printf("Connection established\n");

    
    const char* messages[] = {"HY-335a", "Second Message", "Third Message", "Final Message"};
    int num_messages = sizeof(messages) / sizeof(messages[0]);

   
    for (int i = 0; i < num_messages; i++) {
        if(1){
            bytes_sent = microtcp_send(&socket, messages[i], strlen(messages[i]) + 1, 0);
        }else{
            bytes_sent = microtcp_simulate_packetLoss_send(&socket, messages[i], strlen(messages[i]) + 1, 0);

        }
        printf("Sent '%s' to server, %zd bytes\n", messages[i], bytes_sent);

        
        sleep(1);
    }

    printf("Initiating shutdown\n");
    if (microtcp_shutdown(&socket, 1) != 0) {
        fprintf(stderr, "Error during client-initiated shutdown\n");
    } else {
        printf("Shutdown completed. Socket state: %d\n", socket.state);
    }

    return 0;
}
