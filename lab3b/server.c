/* File: server.c
 * Trying out socket communication between processes using the Internet protocol family.
 * Authors: Felix Sjöqivst and Olle Olofsson
 * 2019-05-02
 */
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT     5555
#define MAXLINE 1024

#define START 0
#define R_WAIT 1
#define R_WAIT2 2
#define DONE 3

#define SYN 1
#define FIN 2
#define RST 3

#define TWH 0
#define SW 1
#define TD 2

typedef struct hd_struct {
    int flags;
    int ACK;
    int seq;
    int windowsize;
    uint16_t crc;
    char *data;
    int length;
    int id;
} hd;

int sockfd;
void logic_loop();
char buffer[MAXLINE];
char *hello = "Hello from server";
struct sockaddr_in servaddr, cliaddr;
int len, n;
int seqy = 0;
int seqx = 0;
int win_size;
// Driver code
int main(){
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);


    // Bind the socket with the server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,
                sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    logic_loop();
    return 0;
}


hd *recvDataGram;

// returns checksum of input bit-string
uint16_t Fletcher16( char *data, int count){
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    int index;

    for(index = 0; index < count; ++index ){
        sum1 = (sum1 + data[index]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }
    return (sum2 << 8 ) | sum1;

}

void writeSock(hd *dg){
    // calc checksum
    dg->crc = Fletcher16(dg->data, dg->length);
    //incr seq
    seqy++;
    dg->seq = seqy;
    // send data
    if(sendto(sockfd, dg, sizeof(*dg),
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
            sizeof(cliaddr)) == -1){
        printf("failed to send - %s\n", strerror(errno));
    }
    printf("seqx = %d\nseqy = %d\n", seqx, seqy);
}

// returns number of successfull bytes read, or -1 if recvfrom error, or -2 if checksum err
int readSock(hd *recvDataGram){
    len = sizeof(cliaddr);
    n = recvfrom(sockfd, recvDataGram, sizeof(*recvDataGram), MSG_WAITALL,
            (struct sockaddr *) &cliaddr, &len);
    //seqx++;
    seqx = recvDataGram->seq;
    if(n == -1)
        return -1;
    // do checksum-test
    /* uint16_t check = Fletcher16(recvDataGram->data, recvDataGram->length); */
    /* if(check == recvDataGram->crc){ */
    /*     seqx = recvDataGram->seq; */
    /*     return strlen(recvDataGram->data); */
    /* } */
    else
        return -3;
}

int TWH_loop(){
    int state = START;
    int sock;
    hd hdtemp;
    memset(&hdtemp, 0, sizeof(hdtemp));
    while(1){
        switch(state){
            case START:
                printf("case: START\n");
                if((sock = readSock(&hdtemp)) == -1){
                    perror("Could not read socket\n");
                }
                else if(sock == -2){
                    printf("checksum error\n");
                }
                else{
                    if(hdtemp.flags == SYN){
                        printf("yaas\n");
                        //hdtemp.seq = seqy;
                        hdtemp.ACK = seqx + 1;
                        hdtemp.flags = SYN;
                        writeSock(&hdtemp);
                        state = R_WAIT;
                    }
                }
                break;
            case R_WAIT:
                printf("case: R_WAIT\n");
                if((sock = readSock(&hdtemp)) == -1){
                    perror("Could not read socket\n");
                }
                else if(sock == -2){
                    printf("checksum error\n");
                }
                else{
                    if(hdtemp.ACK == seqy + 1 && hdtemp.windowsize != 0){
                        win_size = hdtemp.windowsize;
                        hdtemp.ACK = seqy + 2;
                        writeSock(&hdtemp);
                        state = R_WAIT2;
                        printf("send ack + win size\n");
                    }
                    printf("test - %d\n", hdtemp.ACK);
                    printf("test - %d\n", hdtemp.windowsize);
                }

                break;
            case R_WAIT2:
                printf("case: R_WAIT2\n");
                if((sock = readSock(&hdtemp)) == -1){
                    perror("Could not read socket\n");
                }
                else if(sock == -2){
                    printf("checksum error\n");
                }
                else{
                    if(hdtemp.ACK == seqy + 2){
                        state = DONE;
                    }
                }
                break;
            case DONE:
                printf("Three Way Handshake Done\n");
                return SW;
            default:
                break;
        }
    }
}
// returns next state-machine to execute
int SW_loop(){
printf("in SW\n");
}
// returns next state-machine to execute
int TD_loop(){

}
void logic_loop(){
        int nextMachine = 0;
    while(1){
        printf("----SERVER----\n");
        hd *dGram;
        /* n = recvfrom(sockfd, dGram, MAXLINE, */
        /*             MSG_WAITALL, ( struct sockaddr *) &cliaddr, */
        /*             &len); */
        /* buffer[n] = '\0'; */
        //Determine machine
        switch (nextMachine){
            case TWH:
                nextMachine = TWH_loop();
                printf("hejj\n");
                break;
            case SW:
                nextMachine = SW_loop();
                break;
            case TD:
                nextMachine = TD_loop();
                break;
            default:
                printf("DEFAULT\n");
                sleep(2);
                break;
        }

        /* printf("Client : %d\n", dGram->windowsize); */
        /* sendto(sockfd, (const char *)hello, strlen(hello), */
        /*         MSG_CONFIRM, (const struct sockaddr *) &cliaddr, */
        /*         len); */
        /* printf("Hello message sent.\n"); */
    }
}
