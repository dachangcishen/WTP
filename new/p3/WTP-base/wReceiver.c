#include <arpa/inet.h>		// ntohs()
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
	int counter = 0;
	while(1){
		FILE* fd;
		char file[100];
		memset(file, 0, 100);
		strcat(file, path);
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
		struct PacketHeader *ack;
		struct PacketHeader* header;
		char msg[2000] = { 0 };
		int rec;
		while (1) {
			rec = recvfrom(sockfd, (char*)msg, 400, MSG_WAITALL, (struct sockaddr*)&other_addr, &slen);
			if (rec < 0) {
				continue;
			}
			msg[rec] = '\0';
			header = (struct PacketHeader*)msg;
			logging(log, header);
			break;
		}
		if (header->type == 0) {
			header->type = (unsigned int)3; 
			char acc[500] = { 0 }; 
			logging(log, header); 
			memcpy(acc, header, sizeof(*header));
			sendto(sockfd, acc, sizeof(acc), 0, (const struct sockaddr*)&other_addr, sizeof(other_addr));
			printf("start received\n");
		}
		else {
			continue;
		}
		
		//start getting data
		
		char PH_data[500] = { 0 };
		char buffer[2000] = { 0 };
		unsigned int ack_seq = 0;
		unsigned int checksum = 0;
		unsigned int end = -1;
		int status = 0;
		int start_seq;
		char data[2000] = { 0 };
		while (1) {
			int cou = 0;
			for (int j = 0; j < windowsize; j++) {
				memset(buffer, 0, 2000);
				memset(msg, 0, 500);
				rec = recvfrom(sockfd, (char*)buffer, DATA_SIZE, MSG_DONTWAIT, (struct sockaddr*)&other_addr, &slen);
				if (rec == -1) {
					continue;
				}
				cou++;
				buffer[rec] = 0;
				for (int t = 0; t < sizeof(*header); t++) {
					PH_data[t] = buffer[t];
				}
				struct PacketHeader* PH_header = (struct PacketHeader*)PH_data;
				logging(log, PH_header);
				if (PH_header->type == 1) {
					end = PH_header->seqNum;
					continue;
				}
				int length = (int)PH_header->length; 
				if (PH_header->seqNum == ack_seq) {
					
					int t = 0; 
					for (t = 0; t < length; t++) {
						data[t] = buffer[sizeof(*header) + t];
					}
					data[t] = 0; 
					checksum = (unsigned int)crc32(data, t); printf("%u, %u\n",checksum,PH_header->checksum);
					/*if (checksum != PH_header->checksum) {
						break;
					}*/
					ack_seq = PH_header->seqNum + 1;
					fseek(fd_output, PH_header->seqNum * DATA_SIZE, SEEK_SET);
					fwrite(data, 1, length, fd_output);
					printf("%s %d", data,cou);
				}
				if (end == -1 && cou > 0) {
					header->type = 3;
					header->checksum = 0;
					header->length = 0;
					header->seqNum = ack_seq+1;
					char acc[500] = { 0 };
					logging(log, header);
					memcpy(acc, header, sizeof(*header));
					printf("1 %s\n", acc);
					sendto(sockfd, acc, sizeof(acc), 0, (const struct sockaddr*)&other_addr, sizeof(other_addr));
				}
				else if (end > 0) {
					header->type = 3;
					header->checksum = 0;
					header->length = 0;
					header->seqNum = end;
					char acc[500] = { 0 };
					logging(log, header);
					memcpy(acc, header, sizeof(*header));
					printf("2 %s\n", acc);
					sendto(sockfd, acc, sizeof(acc), 0, (const struct sockaddr*)&other_addr, sizeof(other_addr));
				}
			}
		}
	}

	close(sockfd);
}