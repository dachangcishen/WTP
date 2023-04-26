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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define DATA_SIZE 1472

typedef struct CHUNK {
	struct PacketHeader header;
	char content[DATA_SIZE];
} chunk;

void logging(char * log, struct PacketHeader buffer)
{
	FILE* out = fopen(log, "a+");
	int k = fprintf(out, "%u %u %u %u\n", buffer.type, buffer.seqNum, buffer.length, buffer.checksum);
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

	struct PacketHeader ack;
	chunk * buffer=(chunk*)malloc(sizeof(chunk)*windowsize);

	printf("message setted");

	//start getting message
	int cou = 0;
	int status = 0;
	int ack_seq = 0;
	int start_seq;
	while (1) {
		memset((char*)&ack, 0, sizeof(ack));
		memset((char*)buffer, 0, sizeof(chunk)*windowsize);
		int rec = recvfrom(sockfd, buffer, sizeof(chunk)*windowsize, 0, (struct sockaddr*)&other_addr, &slen);
		if (rec > 0) {
			if (buffer[0].header.type == 0) {
				logging(log, buffer[0].header);
				ack.type = 3;
				ack.length = 0;
				start_seq = buffer[0].header.seqNum;
				ack.seqNum = buffer[0].header.seqNum;
				sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, ack);
			}
			if (buffer[0].header.type == 1 && status == 1) {
				logging(log, buffer[0].header);
				ack.type = 3;
				ack.length = 0;
				ack.seqNum = buffer[0].header.seqNum;
				sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, ack);
				break;
			}
			if (buffer[0].header.type == 2) {
				cou = 0;
				for(int i =0;i<windowsize;i++){
					if(buffer[i].header.type!=2){
						break;
					}
					if(buffer[i].header.seqNum<ack_seq){
						continue;
					}
					if(buffer[i].header.seqNum>=ack_seq+windowsize){
						break;
					}
					if(buffer[i].header.checksum!=crc32(buffer[i].content,buffer[i].header.length)){
						break;
					}
					cou++;
					logging(log, buffer[i].header);
					fseek(fd_output, buffer[i].header.seqNum * 1472, SEEK_SET);
					fwrite(buffer[i].content, 1, buffer[i].header.length, fd_output);
				}
				status =1;
				ack_seq+=cou;
				ack.type = 3;
				ack.seqNum = ack_seq;
				sendto(sockfd, &ack, sizeof(struct PacketHeader), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, ack);
			}
			printf("ackseq %d\n", ack_seq);
		}
	}
	free(buffer);
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