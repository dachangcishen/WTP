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
	struct PacketHeader *ack = (struct PacketHeader*)malloc(sizeof(struct PacketHeader));
	char header[16] = { 0 };
	char buffer[DATA_SIZE] = { 0 };
	char* data_of_window = (char*)malloc(sizeof(char) * DATA_SIZE*windowsize);
	memset(data_of_window, 0, DATA_SIZE * windowsize);

	printf("message setted\n");

	//start getting message
	int cou = 0;
	int status = 0;
	int ack_seq = 0;
	int start_seq;
	int cc = 0;
	while (1) {
		cc++;
		memset((char*)ack, 0, sizeof(ack));
		memset((char*)data_of_window, 0, DATA_SIZE*windowsize);
		int rec = recvfrom(sockfd, (char*)data_of_window, DATA_SIZE * windowsize, 0, (struct sockaddr*)&other_addr, &slen);	
		if (rec > 0) {
			char first_header[16] = { 0 };
			for (int i = 0; i < 16; i++) {
				first_header[i] = data_of_window[i];
			}
			struct PacketHeader* fheader = (struct PacketHeader*)first_header;
			if (fheader->type == 0) {
				logging(log, fheader);
				fheader->type = 3;
				start_seq = fheader->seqNum;
				char acc[200] = { 0 };
				memcpy(acc, fheader, 16);
				sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, fheader);
			}
			if (fheader->type == 1) {
				logging(log, fheader);
				fheader->type = 3;
				start_seq = fheader->seqNum;
				char acc[200] = { 0 };
				memcpy(acc, fheader, 16);
				sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, fheader);
				break;
			}
			if (fheader->type == 2) {
				cou = 0;
				int i = 0;
				int max = 0 ;
				for(i = 0;i<windowsize;i++){
					int offset = i * DATA_SIZE;
					for (int j = 0; j < 16; j++) {
						header[j] = data_of_window[j + offset];
					}
					struct PacketHeader* head = (struct PacketHeader*)header;
					if(head->type!=2){
						break;
					}
					if(head->seqNum<ack_seq){
						printf("Packet %u Before Window, Window starts from %u\n",head->seqNum,ack_seq);
						continue;
					}
					if(head->seqNum!=ack_seq+cou){
						printf("Wrong Order Or missing packet %d\n",ack_seq+cou);
						continue;
					}
					char data[1456] = { 0 };
					for (int i = 0; i < head->length; i++) {
						data[i] = data_of_window[i + 16+offset];
					}
					if(head->checksum!=crc32(data, head->length)){
						printf("Wrong Content in %u\n", head->seqNum);
						break;
					}
					logging(log, head);
					fseek(fd_output, head->seqNum * 1456, SEEK_SET);
					fwrite(data, 1, head->length, fd_output);
					cou++;
				}
				ack_seq += cou;
				ack->type = 3;
				ack->seqNum = ack_seq;
				char acc[1024] = { 0 };
				memcpy(acc, ack, 16);
				sendto(sockfd, acc, sizeof(acc), 0, (struct sockaddr*)&other_addr, slen);
				logging(log, ack);
				printf("ackseq %d\n", ack_seq);
			}
		}
	}
	free(data_of_window);
	free(ack);
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