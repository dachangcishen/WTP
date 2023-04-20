#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define DATA_SIZE 1456


struct packet {
    int type;       // 0: START; 1: END; 2: DATA; 3: ACK
    int seq_num;
    int length;     // Length of data; 0 for ACK, START and END packets
    int checksum;   // 32-bit CRC
    char data[DATA_SIZE];    // up to 1456B 
};

int create_packet(struct packet *p, int type, int seq_num, int length, int checksum, char *data) {
    p->type = type;
    p->seq_num = seq_num;
    p->length = length;
    p->checksum = checksum;
    memcpy(p->data, data, length);
    return length;
}

int send_packet(int sockfd, struct packet *p, struct sockaddr_in *addr) {
    int len = sizeof(*addr);
    return sendto(sockfd, p, sizeof(*p), 0, (struct sockaddr*) addr, len);
}

int recv_packet(int sockfd, struct packet *p, struct sockaddr_in *addr) {
    int len = sizeof(*addr);
    return recvfrom(sockfd, p, sizeof(*p), MSG_DONTWAIT, (struct sockaddr*) addr, &len);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        printf("Error! Please Run with proper argument\n");
        return 0;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int windowsize = atoi(argv[3]);
    char *filename = argv[4];
    __time_t timeout = 5;
    char *log = argv[5];
    int max_attempts = 10;

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        return 0;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Error creating socket");
        return 0;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting socket options");
        return 0;
    }
    
    int seq_num = -1;
    srand(time(NULL));
    int ran_num = (rand() % 2048) + 1;
    int ack_num = 0;
    int start_ack = 0;
    int end_ack = 0;
    int send_ack = 0;
    char buffer[DATA_SIZE];
    int buffer_len = 0;
    int total_bytes_sent = 0;
    int total_packets_sent = 0;
    int total_packets_acked = 0;
    int attempts = 0;
    int done = 0;

    while (!done) {
        struct packet p;
        memset(&p, 0, sizeof(p));

        if (seq_num == -1) {
            // Send start packet
            create_packet(&p, 0, ran_num, strlen(filename) + 1, 0, filename);
            if (send_packet(sockfd, &p, &addr) < 0) {
                perror("Error sending packet");
                return 0;
            }
            printf("Sent start packet\n");
            start_ack = 0;
            // Wait for ack
            attempts = 0;
            while (start_ack == 0 && attempts < max_attempts) {
                if (recv_packet(sockfd, &p, &addr) >= 0 && p.type == 3 && p.seq_num == ran_num) {
                    printf("Received start ack\n");
                    start_ack = 1;
                }
                else {
                    printf("Timeout waiting for start ack\n");
                    if (send_packet(sockfd, &p, &addr) < 0) {
                        perror("Error resending packet");
                        return 0;
                    }
                    attempts++;
                }
            }
            if (start_ack == 0) {
                perror("Error sending start packet");
                return 0;
            }
        }
        else {
            // Send data packet
            buffer_len = fread(buffer, 1, DATA_SIZE, fp);
            if (buffer_len == 0) {
                // End of file reached
                create_packet(&p, 1, ran_num, 0, 0, "");
                if (send_packet(sockfd, &p, &addr) < 0) {
                    perror("Error sending packet");
                    return 0;
                }
                printf("Sent end packet\n");
                done = 1;
                
                end_ack = 0;
                attempts = 0;
                while (end_ack == 0 && attempts < max_attempts) {
                    if (recv_packet(sockfd, &p, &addr) >= 0 && p.type == 3 && p.seq_num == ran_num) {
                        printf("Received ack %d\n", ran_num);
                        end_ack = 1;
                    }
                    else {
                        printf("Timeout waiting for ack %d\n", ran_num);
                        if (send_packet(sockfd, &p, &addr) < 0) {
                            perror("Error resending packet");
                            return 0;
                        }
                        attempts++;
                    }
                }
                if (end_ack == 0) {
                    perror("Error sending end packet");
                    return 0;
                }
            }
            else {
                create_packet(&p, 2, seq_num, buffer_len, 0, buffer);
                if (send_packet(sockfd, &p, &addr) < 0) {
                    perror("Error sending packet");
                    return 0;
                }
                printf("Sent data packet %d\n", seq_num);
                total_bytes_sent += buffer_len;
                total_packets_sent++;

                send_ack = 0;
                attempts = 0;
                while (send_ack == 0 && attempts < max_attempts) {
                    if (recv_packet(sockfd, &p, &addr) >= 0 && p.type == 3 && p.seq_num == ack_num) {
                        printf("Received ack %d\n", ack_num);
                        send_ack = 1;
                        ack_num++;
                        total_packets_acked++;
                    }
                    else {
                        printf("Timeout waiting for ack %d\n", ack_num);
                        if (send_packet(sockfd, &p, &addr) < 0) {
                            perror("Error resending packet");
                            return 0;
                        }
                        attempts++;
                    }
                }
                if (send_ack == 0) {
                    perror("Error sending data packet");
                    return 0;
                }
            }
        }

        seq_num++;
    }

    printf("Total bytes sent: %d\n", total_bytes_sent);
    printf("Total packets sent: %d\n", total_packets_sent);
    printf("Total packets acked: %d\n", total_packets_acked);

    fclose(fp);
    close(sockfd);

    return 0;
}
