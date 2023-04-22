#include <arpa/inet.h>		// ntohs()
#include <stdio.h>		// printf(), perror()
#include <stdlib.h>		// atoi()
#include <string.h>		// strlen()
#include <sys/socket.h>		// socket(), connect(), send(), recv()
#include <unistd.h>		// close()
#include <netdb.h>
#include <time.h>
#include <netinet/in.h>
#include "PacketHeader.h"
#include "crc32.h"

#define DATA_SIZE 1456

typedef struct CHUNK {
	struct PacketHeader header;
	char content[DATA_SIZE];
} chunk;

void logging(FILE* log, struct PacketHeader buffer)
{
	fflush(log);
	fprintf(log, "%u %u %u %u\n", buffer.type, buffer.seqNum, buffer.length, buffer.checksum);
	fflush(log);
}


int recive(int sockfd, struct sockaddr_in other_addr, int port, int windowsize,char * outFile, char * log , int counter) {
	
	//setting file
	FILE* fd;
	char file[100];
	if (outFile[0] == '/') {
		file[0] = '.';
	}
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
	printf("%s\n", path);
	FILE* fd_output = fopen(path, "w+");
	if (fd_output == NULL) {
		printf("Erro in open dic");
		exit(-1);
	}
	FILE* fd_log = fopen(log, "w+");
	if (fd_log == NULL) {
		printf("Error in open log file");
		exit(-1);
	}
	printf("receiver file and log set\n");
	int slen = sizeof(other_addr);
	//setting packet message

	struct PacketHeader ack;
	chunk buffer;

	printf("message setted");

	//start getting message
	memset((char*)&ack, 0, sizeof(ack));
	memset((char*)&buffer, 0, sizeof(buffer));
	int cou = 0;
	int status = 0;
	int ack_seq = 0;
	int start_seq;
	while (1) {
		memset((char*)&ack, 0, sizeof(ack));
		memset((char*)&buffer, 0, sizeof(buffer));
		int rec = recvfrom(sockfd, &buffer, sizeof(chunk), 0, (struct sockaddr*)&other_addr, &slen);
		if (rec > 0) {
			if (buffer.header.type == 0) {
				logging(fd_log, buffer.header);
				ack.type = 3;
				ack.length = 0;
				start_seq = buffer.header.seqNum;
				ack.seqNum = buffer.header.seqNum;
				sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
				logging(fd_log, ack);
			}
			if (buffer.header.type == 1 && status == 1) {
				logging(fd_log, buffer.header);
				ack.type = 3;
				ack.length = 0;
				ack.seqNum = buffer.header.seqNum;
				sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
				logging(fd_log, ack);
				break;
			}
			if (buffer.header.type == 2 && buffer.header.seqNum < ack_seq + windowsize && buffer.header.seqNum >= ack_seq ) {
				if (buffer.header.checksum == crc32(buffer.content, buffer.header.length)) {
					cou++;
					logging(fd_log, buffer.header);
					fseek(fd_output, SEEK_END, SEEK_SET);
					fwrite(buffer.content, 1, buffer.header.length, fd_output);
					status = 1;
					if (cou == windowsize) {
						ack_seq += cou;
						ack.type = 3;
						ack.seqNum = buffer.header.seqNum + 1;
						sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
						logging(fd_log, ack);
						cou = 0;
					}
				}
				else {
					ack_seq += cou;
					ack.type = 3;
					ack.seqNum = buffer.header.seqNum;
					sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
					logging(fd_log, ack);
					cou = 0;
				}
			}
		}
		else {
			if (cou = 0) {
				continue;
			}
			else {
				ack_seq += cou;
				ack.type = 3;
				ack.seqNum = buffer.header.seqNum + 1;
				sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
				logging(fd_log, ack);
				cou = 0;
			}
		}
		printf("ackseq %d\n", ack_seq);
	}
}

int main(int argc, const char **argv) {
	if(argc != 5){
		printf("Error: missing or extra arguments");
			return 1;
	}
	int  port = atoi(argv[1]);
	int windowsize = atoi(argv[2]);
	char file[100];
	strcpy(file, argv[3]);
	char log[100];
	strcpy(log, argv[4]);
	
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



  	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		perror("Error binding stream socket");
		return -1;
	}
	
	//set timeout
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 500000;
	int res = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
	if(res < 0)
	{
		perror("Setting sockopt failed");
	}

	for (int i = 0;; i++) {

		int result = recive(sockfd, other_addr, port, windowsize, file, log, i);
		if (result != 0)
			i--;
	}

	close(sockfd);
}
