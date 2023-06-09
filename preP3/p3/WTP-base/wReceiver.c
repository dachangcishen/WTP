﻿#include <arpa/inet.h>		// ntohs()
#include <stdio.h>		// printf(), perror()
#include <stdlib.h>		// atoi()
#include <string.h>		// strlen()
#include <sys/socket.h>		// socket(), connect(), send(), recv()
#include <unistd.h>		// close()
#include <netdb.h>
#include <time.h>
#include <netinet/in.h>
#include "crc32.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define DATA_SIZE 1472

struct PacketHeader
{
	unsigned int type;     // 0: START; 1: END; 2: DATA; 3: ACK
	unsigned int seqNum;   // Describe afterwards
	unsigned int length;   // Length of data; 0 for ACK packets
	unsigned int checksum; // 32-bit CRC
}PH;


void logging(char * log, struct PacketHeader* buffer)
{
	FILE* out = fopen(log, "a+");
	int k = fprintf(out, "%u %u %u %u\n", buffer->type, buffer->seqNum, buffer->length, buffer->checksum);
	fclose(out);
}


int recive(int sockfd, struct sockaddr_in other_addr, int port, int windowsize, char* outFile, char* log, int counter) {

	//setting file
	FILE* fd;
	char file[100];
	memset(file, 0, 100);
	strcat(file, outFile);
	char p1[] = "/FILE-";
	char p2[5];
	sprintf(p2, "%d", counter);
	char p3[] = ".out";
	char path[100];
	strcpy(path, file);
	strcat(path, p1);
	strcat(path, p2);
	strcat(path, p3);
	FILE* fd_output = fopen(path, "w+");
	if (fd_output == NULL) {
		printf("Erro in open dic");
		exit(-1);
	}
	
	printf("receiver file and log set\n");
	int slen = sizeof(other_addr);
	//setting packet message

	struct PacketHeader * ack = (struct PacketHeader*)malloc(sizeof(struct PacketHeader));
	char header[16] = { 0 };
	char buffer[DATA_SIZE] = { 0 };

	printf("message setted\n");

	//start getting message
	int cou = 0;
	int status = 0;
	int ack_seq = 0;
	int start_seq;
	while (1) {
		memset((char*)ack, 0, sizeof(*ack));
		memset((char*)buffer, 0, DATA_SIZE);
		memset((char*)header, 0, sizeof(buffer));
		int rec = recvfrom(sockfd, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&other_addr, &slen);
		if (rec > 0) {
			for (int i = 0; i < 16; i++) {
				header[i] = buffer[i];
			}
			struct PacketHeader* head = (struct PacketHeader*)header; 
			if (head->type == 0) {
				logging(log, head); 
				ack->type = 3;
				ack->length = 0;
				start_seq = head->seqNum;
				ack->seqNum = head->seqNum;
				char acc[1024] = { 0 };
				memcpy(acc, ack, 16);
				sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, ack);
			}
			if (head->type == 1 && status == 1) {
				logging(log, head);
				ack->type = 3;
				ack->length = 0;
				start_seq = head->seqNum;
				ack->seqNum = head->seqNum;
				char acc[1024] = { 0 };
				memcpy(acc, ack, 16);
				sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, ack);
				break;
			}
			if (head->type == 2 && head->seqNum < ack_seq + windowsize && head->seqNum >= ack_seq+cou) {
				char data[1456] = { 0 };
				for (int i = 0; i < head->length; i++) {
					data[i] = buffer[i + 16];
				}
				printf("%u : %u\n", head->checksum, crc32(data, head->length));
				printf("%s", data);
				if (head->checksum == crc32(data, head->length)) {
					cou++;
					logging(log, head);
					fseek(fd_output, head->seqNum * 1456, SEEK_SET);
					fwrite(data, 1, head->length, fd_output);
					printf("%s", data);
					status = 1;
					if (cou >= windowsize) {
						ack_seq += cou;
						ack->type = 3;
						ack->seqNum = head->seqNum + 1;
						char acc[1024] = { 0 };
						memcpy(acc, ack, 16);
						sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
						logging(log, ack);
						cou = 0;
					}
				}
				else {
					ack_seq += cou;
					ack->type = 3;
					ack->seqNum = head->seqNum;
					char acc[1024] = { 0 };
					memcpy(acc, ack, 16);
					sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
					logging(log, ack);
					cou = 0;
				}
			}
		}
		else {
			if (cou == 0) {
				continue;
			}
			else {
				ack_seq += cou;
				ack->type = 3;
				ack->seqNum = ack_seq;
				char acc[1024] = { 0 };
				memcpy(acc, ack, 16);
				sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, ack);
				cou = 0;
			}
		}
		printf("ackseq %d\n", ack_seq);
	}
	fclose(fd_output);
}

int main(int argc, const char** argv) {
	if (argc != 5) {
		printf("Error: missing or extra arguments");
		return 1;
	}
	int  port = atoi(argv[1]);
	int windowsize = atoi(argv[2]);
	char file[100];
	strcpy(file, argv[3]);
	char path[] = "";
	strcat(path, file);
	printf("%s\n",path);
	char log[100];
	strcpy(log, argv[4]);

	/*FILE* k = fopen(log, "w");
	fprintf(k, "");*/

	//create UDP socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1) {
		perror("Error opening stream socket");
		return -1;
	}
	struct sockaddr_in addr, other_addr;
	int len;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	memset(&other_addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;



	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("Error binding stream socket");
		return -1;
	}

	//set timeout
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 500000;
	int res = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
	if (res < 0)
	{
		perror("Setting sockopt failed");
	}
	//fclose(k);
	for (int i = 0;; i++) {

		int result = recive(sockfd, other_addr, port, windowsize, path, log, i);
		if (result != 0)
			i--;
	}

	close(sockfd);
}