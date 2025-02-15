#include "microtcp.h"
#include "../utils/crc32.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h> 
#include <fcntl.h> 
//remove
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> 

#define PACKET_DROP_PROBABILITY 30  // 30% chance to drop a packet
microtcp_sock_t microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t new_socket;
  if ((new_socket.sd = socket(domain,type,protocol)) == -1) {
    perror("could not open socket");
    exit(EXIT_FAILURE);
  }
  
  new_socket.state = CLOSED;
  new_socket.init_win_size = MICROTCP_WIN_SIZE;
  new_socket.curr_win_size = MICROTCP_RECVBUF_LEN;
  new_socket.recvbuf = (uint8_t *)malloc(sizeof(uint8_t) * MICROTCP_RECVBUF_LEN);
  if (!new_socket.recvbuf) {
    perror("failed to allocate memory");
    exit(EXIT_FAILURE);
  }
  new_socket.buf_fill_level = 0;
  new_socket.cwnd = MICROTCP_INIT_CWND;
  new_socket.ssthresh = MICROTCP_INIT_SSTHRESH;
  new_socket.seq_number = 0;
  new_socket.ack_number = 0;
  new_socket.packets_send = 0;
  new_socket.packets_received = 0;
  new_socket.packets_lost = 0;
  new_socket.bytes_send = 0 ;
  new_socket.bytes_received = 0;
  new_socket.bytes_lost = 0;
  return new_socket;
}


int microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{
    if (bind(socket->sd, address, address_len) == -1) {
        perror("Error binding socket");
  
        return -1;
    }

    socket->server_address = address;
    socket->server_address_len = address_len;
    socket->state = LISTEN;
    printf("Listening for connections...\n");
   
    return EXIT_SUCCESS;
}
int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len) {
    socket->seq_number = rand() % 10000;  
    socket->server_address = address;
    socket->server_address_len = address_len;
    socket->state = WAIT;

    //Send SYN
    microtcp_header_t syn_header = {0};
    syn_header.seq_number = socket->seq_number;
    syn_header.ack_number = 0;  
    syn_header.control = MICROTCP_SYN_CONTROL;
    syn_header.checksum = crc32((uint8_t*)&syn_header, sizeof(microtcp_header_t));

    printf("Client: Sending SYN packet - Seq: %u\n", syn_header.seq_number);
    sendto(socket->sd, &syn_header, sizeof(syn_header), 0, socket->server_address, socket->server_address_len);

    //Receive SYN-ACK
    microtcp_header_t syn_ack_packet;
    ssize_t recv_len = recvfrom(socket->sd, &syn_ack_packet, sizeof(microtcp_header_t), 0, NULL, NULL);
    
    if (recv_len < 0) {
        perror("Client: Error receiving SYN-ACK");
        return -1;
    }

    printf("Client: Received SYN-ACK - Seq: %u, Ack: %u\n", syn_ack_packet.seq_number, syn_ack_packet.ack_number);

    socket->ack_number = syn_ack_packet.seq_number + 1;  

    // Send Final ACK
    microtcp_header_t ack_header = {0};
    ack_header.seq_number = socket->seq_number + 1;  
    ack_header.ack_number = socket->ack_number+1;  
    ack_header.control = MICROTCP_ACK_CONTROL;
    ack_header.checksum = crc32((uint8_t*)&ack_header, sizeof(microtcp_header_t));

    printf("Client: Sending final ACK - Seq: %u, Ack: %u\n", ack_header.seq_number, ack_header.ack_number);
    sendto(socket->sd, &ack_header, sizeof(ack_header), 0, socket->server_address, socket->server_address_len);

  
    socket->seq_number = ack_header.ack_number;
    socket->ack_number = ack_header.ack_number;  
    socket->state = ESTABLISHED;

    return EXIT_SUCCESS;
}




int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len) {
    socket->seq_number = rand() % 10000;  
    socket->client_address = address;
    socket->client_address_len = address_len;

    //Receive SYN
    microtcp_header_t syn_packet = {0};
    ssize_t recv_len = recvfrom(socket->sd, &syn_packet, sizeof(microtcp_header_t), 0, socket->client_address, &socket->client_address_len);

    if (recv_len < 0) {
        perror("Server: Failed to receive SYN packet");
        return -1;
    }

    printf("Server: Received SYN - Seq: %u\n", syn_packet.seq_number);

    socket->ack_number = syn_packet.seq_number + 1;

    // **Send SYN-ACK**
    microtcp_header_t syn_ack_packet = {0};
    syn_ack_packet.control = MICROTCP_SYN_CONTROL | MICROTCP_ACK_CONTROL;
    syn_ack_packet.seq_number = socket->seq_number;
    syn_ack_packet.ack_number = socket->ack_number; 

    printf("Server: Sending SYN-ACK - Seq: %u, Ack: %u\n", syn_ack_packet.seq_number, syn_ack_packet.ack_number);
    sendto(socket->sd, &syn_ack_packet, sizeof(microtcp_header_t), 0, socket->client_address, socket->client_address_len);

    //Receive final ACK from client
    microtcp_header_t ack_packet;
    recv_len = recvfrom(socket->sd, &ack_packet, sizeof(microtcp_header_t), 0, NULL, NULL);
    
    if (recv_len < 0) {
        perror("Server: Failed to receive ACK packet");
        return -1;
    }

    printf("Server: Received final ACK - Seq: %u, Ack: %u\n", ack_packet.seq_number, ack_packet.ack_number);

    socket->seq_number = ack_packet.ack_number; 
    socket->ack_number = ack_packet.seq_number + 1; 
    socket->state = ESTABLISHED;

    return 0;
}






int microtcp_shutdown(microtcp_sock_t *socket, int how) {
    printf("Attempting shutdown. Current state: %d, Method: %d\n", socket->state, how);
    ssize_t recv_len;
    struct timeval tv;
    tv.tv_sec = 5;  // Timeout for 5 sec
    tv.tv_usec = 0;
    setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    microtcp_header_t packet = {0};
    uint8_t buffer[sizeof(microtcp_header_t)];

    if (how == 1) {  //Client-initiated shutdown
        printf("Client: Sending FIN...\n");
        packet.control = MICROTCP_FIN_CONTROL;
        packet.seq_number = socket->seq_number;
        packet.ack_number = socket->seq_number + 1;
        packet.checksum = crc32((uint8_t*)&packet, sizeof(microtcp_header_t));

        printf("Client: Sending FIN packet - Seq: %u, Ack: %u, Control: %d, Checksum: %08X\n",
               packet.seq_number, packet.ack_number, packet.control, packet.checksum);

        if (sendto(socket->sd, &packet, sizeof(packet), 0, 
                   (struct sockaddr *)socket->server_address, socket->server_address_len) < 0) {
            perror("Client: Failed to send FIN");
            return -1;
        }

        socket->state = FIN_WAIT_1;

        //Wait for ACK from server
        recv_len = recvfrom(socket->sd, &packet, sizeof(packet), 0, NULL, NULL);
        if (recv_len < 0 || packet.control != MICROTCP_ACK_CONTROL) {
            printf("Client: Expected ACK after FIN, but got something else.\n");
            return -1;
        }
       
         printf("Client: ACK from server received.- Seq: %u, Ack: %u, Control: %d, Checksum: %08X\n",
                   packet.seq_number, packet.ack_number, packet.control, packet.checksum);
        socket->state = FIN_WAIT_2;

        //Wait for FIN from server
        int retries = 3;
        while (retries > 0) {
            recv_len = recvfrom(socket->sd, &packet, sizeof(packet), 0, NULL, NULL);
            printf("Client: FIN received from server packet - Seq: %u, Ack: %u, Control: %d, Checksum: %08X\n",
                   packet.seq_number, packet.ack_number, packet.control, packet.checksum);

            if (recv_len > 0 && packet.control == MICROTCP_FIN_CONTROL) {
                printf("Client: FIN received from server. Sending final ACK...\n");
                break;
            } else {
                printf("Client: No FIN received, got Control: %d instead. Retrying...\n", packet.control);
                sleep(1);
                retries--;
            }
        }
        if (retries == 0) {
            printf("Client: Failed to receive FIN from server after retries.\n");
            return -1;
        }
        socket->ack_number = packet.seq_number + 1;
        //Send final ACK
        packet.control = MICROTCP_ACK_CONTROL;
        packet.seq_number = socket->seq_number;
        packet.ack_number = socket->ack_number;
        packet.checksum = crc32((uint8_t*)&packet, sizeof(microtcp_header_t));

        printf("Client: Sending final ACK packet - Seq: %u, Ack: %u, Control: %d, Checksum: %08X\n",
               packet.seq_number, packet.ack_number, packet.control, packet.checksum);

        if (sendto(socket->sd, &packet, sizeof(packet), 0, 
                   (struct sockaddr *)socket->server_address, socket->server_address_len) < 0) {
            perror("Client: Failed to send final ACK");
            return -1;
        }

        printf("Client: Final ACK sent. Connection closing.\n");
        socket->state = CLOSED;
    } 

    else if (how == 0) {  //Server responding to FIN
        printf("Server: Checking for FIN...\n");

        if (socket->state != FIN_RECEIVED) {
            recv_len = recvfrom(socket->sd, &packet, sizeof(packet), 0, NULL, NULL);
            if (recv_len < 0 || packet.control != MICROTCP_FIN_CONTROL) {
                printf("Server: Expected FIN, but got something else.\n");
                return -1;
            }
            socket->ack_number = packet.seq_number + 1;
            printf("Server: FIN received.\n");
        } else {
            printf("Server: FIN was already received earlier.\n");
        }

        //Send ACK for FIN
        microtcp_header_t ack_packet = {0};
        ack_packet.control = MICROTCP_ACK_CONTROL;
        ack_packet.seq_number = socket->seq_number;  
        ack_packet.ack_number = socket->ack_number;  
        ack_packet.checksum = crc32((uint8_t*)&ack_packet, sizeof(microtcp_header_t));

        printf("Server: Sending ACK packet - Seq: %u, Ack: %u, Control: %d, Checksum: %08X\n",
               ack_packet.seq_number, ack_packet.ack_number, ack_packet.control, ack_packet.checksum);

        sendto(socket->sd, &ack_packet, sizeof(ack_packet), 0, 
               (struct sockaddr *)socket->client_address, socket->client_address_len);

        //Now Send FIN
        printf("Server: Sending FIN to client...\n");
        microtcp_header_t fin_packet = {0};
        fin_packet.control = MICROTCP_FIN_CONTROL;
        fin_packet.seq_number = socket->seq_number; 
        fin_packet.ack_number = socket->ack_number;
        fin_packet.checksum = crc32((uint8_t*)&fin_packet, sizeof(microtcp_header_t));
        socket->seq_number += 1; 
        printf("Server: Sending FIN packet - Seq: %u, Ack: %u, Control: %d, Checksum: %08X\n",
               fin_packet.seq_number, fin_packet.ack_number, fin_packet.control, fin_packet.checksum);

        sendto(socket->sd, &fin_packet, sizeof(fin_packet), 0, 
               (struct sockaddr *)socket->client_address, socket->client_address_len);

        
        socket->seq_number++;

        //Wait for final ACK
        recv_len = recvfrom(socket->sd, &ack_packet, sizeof(ack_packet), 0, NULL, NULL);
        if (recv_len < 0 || ack_packet.control != MICROTCP_ACK_CONTROL) {
            printf("Server: Expected final ACK, but got something else.\n");
            return -1;
        }

        printf("Server: Received final ACK - Seq: %u, Ack: %u, Control: %d, Checksum: %08X\n",
               ack_packet.seq_number, ack_packet.ack_number, ack_packet.control, ack_packet.checksum);

        printf("Server: Final ACK received. Connection closed.\n");
        socket->state = CLOSED;
    }

    return 0;
}



uint32_t crc32(const uint8_t *data, size_t length);


int validate_checksum(microtcp_header_t *packet, uint8_t *data) {
    uint32_t received_checksum = packet->checksum;  // Store received checksum
    packet->checksum = 0; 

    uint32_t computed_checksum = crc32(data, sizeof(microtcp_header_t) + packet->data_len);

    packet->checksum = received_checksum; 
    printf("Computed checksum: %08X, Received checksum: %08X\n", computed_checksum, packet->checksum);
    
    return computed_checksum == received_checksum;
}

   


//Check if ACK is a duplicate
int is_duplicate_ack(microtcp_sock_t *socket, microtcp_header_t *ack_packet) {
    return (ack_packet->ack_number == socket->ack_number) && 
           (ack_packet->window == socket->curr_win_size);
}


// Handle retransmission
int handle_retransmission(microtcp_sock_t *socket, uint8_t *packet_buffer, size_t packet_size, int flags) {
    int retries = 3;
    while (retries > 0) {
        printf("Retransmitting last packet (Attempt: %d)\n", 4 - retries);
        sendto(socket->sd, packet_buffer, packet_size, flags, 
               (struct sockaddr *)socket->server_address, socket->server_address_len);
        sleep(1);
        retries--;
    }
    printf("Failed to receive valid ACK after retries. Aborting transmission.\n");
    return -1;
}

// Send ACK
void send_ack(microtcp_sock_t *socket) {
    microtcp_header_t ack_packet = {0};
    ack_packet.seq_number = socket->seq_number; 
    ack_packet.ack_number = socket->ack_number;
    ack_packet.control = MICROTCP_ACK_CONTROL;
    ack_packet.checksum = crc32((uint8_t*)&ack_packet, sizeof(microtcp_header_t));
    ack_packet.window = socket->curr_win_size; 

    printf("Sending ACK - Seq: %u, Ack: %u, Window: %u\n", ack_packet.seq_number, ack_packet.ack_number, ack_packet.window);
    
    sendto(socket->sd, &ack_packet, sizeof(ack_packet), 0, 
           (struct sockaddr *)socket->client_address, socket->client_address_len);

    socket->seq_number += 1; 
}




#define MAX_DUP_ACKS 3 

void send_duplicate_ack(microtcp_sock_t *socket) {
    static int dup_ack_count = 0; 

    if (dup_ack_count >= MAX_DUP_ACKS) {
        printf("Skipping duplicate ACK (Max reached)\n");
        return;  
    }

    microtcp_header_t dup_ack = {0};
    dup_ack.seq_number = socket->seq_number;
    dup_ack.ack_number = socket->ack_number;
    dup_ack.control = MICROTCP_ACK_CONTROL;
    dup_ack.checksum = crc32((uint8_t*)&dup_ack, sizeof(microtcp_header_t));

    printf("Sending duplicate ACK (%d/%d) - Seq: %u, Ack: %u\n",
           dup_ack_count + 1, MAX_DUP_ACKS, dup_ack.seq_number, dup_ack.ack_number);

    sendto(socket->sd, &dup_ack, sizeof(dup_ack), 0, 
           (struct sockaddr *)socket->client_address, socket->client_address_len);

    dup_ack_count++; 
}


ssize_t microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length, int flags) {
    const uint8_t *data = (const uint8_t *)buffer;
    size_t bytes_sent = 0;
    int duplicate_ack_count = 0;
    uint32_t last_unack_seq = socket->seq_number;

    while (bytes_sent < length) {
        size_t segment_size = (length - bytes_sent > MICROTCP_MSS) ? MICROTCP_MSS : (length - bytes_sent);

        //Prepare packet
        microtcp_header_t packet = {0};
        packet.seq_number = socket->seq_number;
        packet.ack_number = socket->ack_number;
        packet.control = 0;
        packet.data_len = segment_size;
        packet.window = socket->curr_win_size;

        uint8_t packet_buffer[sizeof(microtcp_header_t) + MICROTCP_MSS];
        memcpy(packet_buffer, &packet, sizeof(microtcp_header_t));
        memcpy(packet_buffer + sizeof(microtcp_header_t), data + bytes_sent, segment_size);

        packet.checksum = crc32(packet_buffer, sizeof(microtcp_header_t) + segment_size);
        memcpy(packet_buffer, &packet, sizeof(microtcp_header_t));

        printf("Sending packet - Seq: %u, Ack: %u, Data Len: %u, Window: %u, Checksum: %08X\n",
               packet.seq_number, packet.ack_number, packet.data_len, packet.window, packet.checksum);

        //Send packet
        sendto(socket->sd, packet_buffer, sizeof(microtcp_header_t) + segment_size, flags,
               (struct sockaddr *)socket->server_address, socket->server_address_len);

        microtcp_header_t ack_packet;
        ssize_t recv_len;
        int retries = 3;

        while (retries > 0) {
            recv_len = recvfrom(socket->sd, &ack_packet, sizeof(microtcp_header_t), 0, NULL, NULL);
            if (recv_len > 0) {
                printf("Received ACK - Seq: %u, Ack: %u, Window: %u\n",
                       ack_packet.seq_number, ack_packet.ack_number, ack_packet.window);

                socket->curr_win_size = ack_packet.window;

                //Ignore window update ACKs
                if ((ack_packet.ack_number == socket->ack_number) && (ack_packet.window != socket->curr_win_size)) {
                    printf("Window update received. New Window Size: %u\n", ack_packet.window);
                    socket->curr_win_size = ack_packet.window;
                    duplicate_ack_count = 0;
                    continue;
                }

                //Check if ACK is for the expected sequence number
                if (ack_packet.ack_number == socket->seq_number + segment_size) {
                    socket->seq_number += segment_size;  
                    socket->ack_number = ack_packet.ack_number;
                    bytes_sent += segment_size;
                    duplicate_ack_count = 0; 
                    last_unack_seq = socket->seq_number; 
                    break;
                } 
                //Handle duplicate ACKs
                else if (ack_packet.ack_number == last_unack_seq) {
                    duplicate_ack_count++;
                    printf("Duplicate ACK detected (%d). Retrying...\n", duplicate_ack_count);
                    
                    // Fast Retransmit on triple duplicate ACK
                    if (duplicate_ack_count >= 3) {
                        printf("Triple duplicate ACKs detected. Fast retransmit triggered for Seq: %u\n", last_unack_seq);

                        //Retransmit last unacknowledged packet
                        sendto(socket->sd, packet_buffer, sizeof(microtcp_header_t) + segment_size, flags,
                               (struct sockaddr *)socket->server_address, socket->server_address_len);
                        
                        duplicate_ack_count = 0;  
                        retries = 3;  
                        continue;  
                    }
                }
            } else {
                printf("Timeout waiting for ACK. Retransmitting last packet.\n");
                sendto(socket->sd, packet_buffer, sizeof(microtcp_header_t) + segment_size, flags,
                       (struct sockaddr *)socket->server_address, socket->server_address_len);
                retries--;
            }
        }

        if (retries == 0) {
            printf("Failed to receive valid ACK after retries. Aborting transmission.\n");
            return -1;
        }
    }
    return bytes_sent;
}




ssize_t microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags) {
    uint8_t *data = (uint8_t *)buffer;
    size_t bytes_received = 0;
    FILE *fp = fopen("received_file.txt", "a");

    if (!fp) {
        perror("Failed to open file for writing");
        return -EXIT_FAILURE;
    }

    while (1) { //Keep receiving until the client sends all data
        uint8_t packet_buffer[sizeof(microtcp_header_t) + MICROTCP_MSS];
        microtcp_header_t *packet = (microtcp_header_t *) packet_buffer;
        ssize_t packet_len = recvfrom(socket->sd, packet_buffer, sizeof(packet_buffer), 0, NULL, NULL);

        if (packet_len < 0) break;

        printf("Received packet - Seq: %u, Ack: %u, Data Len: %u, Checksum: %08X, Window: %u\n",
               packet->seq_number, packet->ack_number, packet->data_len, packet->checksum, packet->window);
	if (packet->control == MICROTCP_FIN_CONTROL) {
            socket->state = FIN_RECEIVED;
            return bytes_received;
        }
        if (!validate_checksum(packet, packet_buffer)) {
            printf("Checksum mismatch! Dropping packet.\n");
            continue;
        }

        if (packet->seq_number != socket->ack_number) {
            printf("Out-of-order packet detected! Expected Seq: %u, Got: %u. Sending duplicate ACK.\n",
                   socket->ack_number, packet->seq_number);
            send_duplicate_ack(socket);
            continue;
        }

        fwrite(packet_buffer + sizeof(microtcp_header_t), sizeof(char), packet->data_len, fp);
        fflush(fp);

        bytes_received += packet->data_len;
        socket->buf_fill_level += packet->data_len;
        if (socket->buf_fill_level > MICROTCP_RECVBUF_LEN) {
            socket->buf_fill_level = MICROTCP_RECVBUF_LEN;
        }

        if (socket->buf_fill_level >= MICROTCP_RECVBUF_LEN * 0.7) {
            size_t bytes_to_clear = MICROTCP_RECVBUF_LEN / 2;
            socket->buf_fill_level -= bytes_to_clear;
        }

        socket->ack_number = packet->seq_number + packet->data_len;
        socket->curr_win_size = MICROTCP_RECVBUF_LEN - socket->buf_fill_level;

        printf("Updated Window Size: %zu, Buffer Fill Level: %zu\n", socket->curr_win_size, socket->buf_fill_level);
        send_ack(socket);
    }

    fclose(fp);
    return bytes_received;
}



ssize_t microtcp_simulate_packetLoss_send(microtcp_sock_t *socket, const void *buffer, size_t length, int flags) {
    const uint8_t *data = (const uint8_t *)buffer;
    size_t bytes_sent = 0;
    int duplicate_ack_count = 0;
    struct timeval tv;

    //Set timeout for receiving ACKs
    tv.tv_sec = 2;  // Timeout of 2 seconds
    tv.tv_usec = 0;
    setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    while (bytes_sent < length) {
        size_t segment_size = (length - bytes_sent > MICROTCP_MSS) ? MICROTCP_MSS : (length - bytes_sent);

        microtcp_header_t packet = {0};
        packet.seq_number = socket->seq_number;
        packet.ack_number = socket->ack_number;
        packet.control = 0;
        packet.data_len = segment_size;

        uint8_t packet_buffer[sizeof(microtcp_header_t) + MICROTCP_MSS];
        memcpy(packet_buffer, &packet, sizeof(microtcp_header_t));
        memcpy(packet_buffer + sizeof(microtcp_header_t), data + bytes_sent, segment_size);

        packet.checksum = crc32(packet_buffer, sizeof(microtcp_header_t) + segment_size);
        memcpy(packet_buffer, &packet, sizeof(microtcp_header_t));

        int retries = 3; 

        while (retries > 0) {
            //Send the packet
            printf("Sending packet - Seq: %u, Ack: %u, Data Len: %u, Checksum: %08X\n",
                   packet.seq_number, packet.ack_number, packet.data_len, packet.checksum);

            sendto(socket->sd, packet_buffer, sizeof(microtcp_header_t) + segment_size, flags,
                   (struct sockaddr *)socket->server_address, socket->server_address_len);

            //Wait for ACK
            microtcp_header_t ack_packet;
            ssize_t recv_len = recvfrom(socket->sd, &ack_packet, sizeof(microtcp_header_t), 0, NULL, NULL);

            if (recv_len > 0) {
                printf("Received ACK - Seq: %u, Ack: %u\n", ack_packet.seq_number, ack_packet.ack_number);

                // Check for duplicate ACKs
              if (is_duplicate_ack(socket, &ack_packet)) {
                duplicate_ack_count++;
                printf("Duplicate ACK detected (%d). Retransmitting...\n", duplicate_ack_count);
                if (duplicate_ack_count >= 3) { 
                    duplicate_ack_count = 0;
                    return handle_retransmission(socket, packet_buffer, sizeof(microtcp_header_t) + segment_size, flags);
                }
            }else {
                duplicate_ack_count = 0;
            }

                // ACK is valid, move to next packet
                if (ack_packet.ack_number >= packet.seq_number + segment_size) {
                    socket->seq_number += segment_size;
                    bytes_sent += segment_size;
                    break;
                }
            } else {
                // No ACK received, retry transmission
                printf("No ACK received. Retransmitting (%d attempts left)...\n", retries - 1);
                retries--;
            }
        }

        if (retries == 0) {
            printf("Packet lost after retries! Aborting...\n");
            return -1;
        }
    }
    return bytes_sent;
}

ssize_t microtcp_simulate_packetLoss_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags) {
    uint8_t *data = (uint8_t *)buffer;
    size_t bytes_received = 0;

    srand(time(NULL)); 

    while (bytes_received < length) {
        uint8_t packet_buffer[sizeof(microtcp_header_t) + length];
        microtcp_header_t *packet = (microtcp_header_t *) packet_buffer;
        ssize_t packet_len = recvfrom(socket->sd, packet_buffer, sizeof(packet_buffer), 0, NULL, NULL);

        if (packet_len < 0) return -1;

        // Simulate packet loss
        if ((rand() % 100) < PACKET_DROP_PROBABILITY) {
            printf("Packet loss simulated: Dropping packet - Seq: %u\n", packet->seq_number);

            // Send a duplicate ACK for the last correctly received packet
            printf("Sending duplicate ACK for last successful packet - Seq: %u, Ack: %u\n",
                socket->seq_number, socket->ack_number);
            send_duplicate_ack(socket);

            continue;  // Drop the packet and wait for retransmission
        }


        printf("Received packet - Seq: %u, Ack: %u, Data Len: %u, Checksum: %08X\n",
               packet->seq_number, packet->ack_number, packet->data_len, packet->checksum);

        if (!validate_checksum(packet, packet_buffer)) {
            printf("Checksum mismatch! Dropping packet.\n");
            continue;
        }

        if (packet->seq_number != socket->ack_number) {
            printf("Out-of-order packet detected! Expected Seq: %u, Got: %u. Sending duplicate ACK.\n",
                   socket->ack_number, packet->seq_number);
            send_duplicate_ack(socket);
            continue;
        }
        printf("Received packet - Seq: %u, Ack: %u, Control: %d, Data Len: %u, Checksum: %08X\n",
               packet->seq_number, packet->ack_number, packet->control, packet->data_len, packet->checksum);

        if (packet->control == MICROTCP_FIN_CONTROL) {
            socket->state = FIN_RECEIVED;
            return bytes_received;
        }
        memcpy(data + bytes_received, packet_buffer + sizeof(microtcp_header_t), packet->data_len);
        bytes_received += packet->data_len;

        socket->ack_number = packet->seq_number + packet->data_len;
        send_ack(socket);
    }
    return bytes_received;
}
