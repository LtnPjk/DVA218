/* File: client.c
 * Trying out socket communication between processes using the Internet protocol family.
 * Usage: client [host name], that is, if a server is running on 'lab1-6.idt.mdh.se'
 * then type 'client lab1-6.idt.mdh.se' and follow the on-screen instructions.
 * Authors: Felix Sjöqvist and Olle Olofsson
 * 2019-05-02
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <poll.h>

#define PORT 5555
#define message 1024
#define MAXLINE 1024
#define WIN_SIZE 5

#define START 0
#define R_WAIT 1
#define W_WAIT 2
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

hd recvDataGram;
int sockfd;
void logic_loop();
char buffer[MAXLINE];
char *hello = "Hello from server";
struct sockaddr_in servaddr, cliaddr;
int len, n;
int seqx = 0;
int seqy = 0;
// returns checksum of input bit-string
uint16_t Fletcher16( uint8_t *data, int count){
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    int index;

    for(index = 0; index < count; ++index ){
        sum1 = (sum1 + data[index]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }
    return (sum2 << 8 ) | sum1;

}

void printPacket(hd dg){
    printf("seqx: %d | ", seqx);
    printf("seqy: %d | ", seqy);
    printf("flags: %d | ", dg.flags);
    printf("ACK: %d | ", dg.ACK);
    printf("seq: %d | ", dg.seq);
    printf("winsize: %d | ", dg.windowsize);
    printf("crc: %u\n", (unsigned int)dg.crc);
    return;
}

void writeSock(hd *dg){
    len = sizeof(cliaddr);
    // calc checksum
    //dg->crc = Fletcher16(dg->data, dg->length);
    //incr seq
    seqx++;
    dg->seq = seqx;
    // send data
    if(sendto(sockfd, dg, sizeof(hd),
                MSG_CONFIRM, (const struct sockaddr *) &servaddr,
                sizeof(servaddr)) == -1){
        printf("failed to send - %s\n", strerror(errno));
    }
}

struct pollfd fd;
// returns number of successfull bytes read, or -1 if recvfrom error, or -2 if checksum err
int readSock(hd *recvDataGram, int timeout){
    int ret;

    fd.fd = sockfd; // your socket handler
    fd.events = POLLIN;
    ret = poll(&fd, 1, timeout); // 1 second for timeout
    switch (ret) {
        case -1:
            // Error
            return -1;
        case 0:
            // Timeout
            return -3;
        default:
            len = sizeof(cliaddr);
            n = recvfrom(sockfd, recvDataGram, sizeof(hd), MSG_WAITALL,
                    (struct sockaddr *) &servaddr, &len);
            seqy = recvDataGram->seq;
            if(n == -1){
                printf("jaaa\n");
                return -1;
            }
            // do checksum-test
            uint16_t check = Fletcher16(recvDataGram->data, recvDataGram->length);
            if(check != recvDataGram->crc){
                return -2;
            }
            else
                //return strlen(recvDataGram->data);
                return -4;
    }
}

int TWH_loop(){
    int state = START;
    int sock;
    hd hdtemp;
    memset(&hdtemp, 0, sizeof(hdtemp));
    while(1){
        switch(state){
            // starting state, directly sending a SYN
            case START:
                printf(">state: START\n");
                /* send SYN=x to server */
                memset(&hdtemp, 0, sizeof(hdtemp));
                hdtemp.flags = SYN;
                writeSock(&hdtemp);
                state = R_WAIT;
                printPacket(hdtemp);
                break;
                // state waiting for SYN+ACK
            case R_WAIT:
                //sleep(4);
                printf(">state: R_WAIT\n");
                // read socket and check for errors
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                    break;
                }
                // if checksum error, wait for resend from server
                else if(sock == -2){
                    printf("checksum error\n");
                    seqx = 0;
                    int ret;
                    while(1){
                        ret = poll(&fd, 1, -1); 
                        if(ret == -1)
                            printf("error: %s\n", strerror(errno));
                        else{
                            printf("packet found\n");
                            state = R_WAIT;
                            break;
                        }
                    }
                    break;
                }
                // if error is timeout, resend previous packet and stay on this state
                else if(sock == -3){
                    seqx = 0;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.flags = SYN;
                    writeSock(&hdtemp);
                    state = R_WAIT;
                    printPacket(hdtemp);
                    break;
                }
                // if no erorrs, send ACK+WIN_SIZE
                else{
                    if(hdtemp.flags == SYN && hdtemp.ACK == seqx + 1){
                        //seqy = hdtemp.seq;
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        /* sleep(3); */
                        //hdtemp.crc = 3;
                        hdtemp.ACK = seqy + 1;
                        hdtemp.windowsize = WIN_SIZE;
                        writeSock(&hdtemp);
                        /* printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc); */
                        printPacket(hdtemp);
                        state = W_WAIT;
                    }
                    else{
                        int ret;
                        while(1){
                            ret = poll(&fd, 1, -1); 
                            if(ret == -1)
                                printf("error: %s\n", strerror(errno));
                            else{
                                printf("packet found\n");
                                state = R_WAIT;
                                break;
                            }
                        }
                    }
                }
                break;
                // state waiting for ACK+WIN_SIZE
            case W_WAIT:
                printf(">state: W_WAIT\n");
                //read socket and check for errors
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                }
                // if checksum error, resend packet with right sequence number
                else if(sock == -2){
                    printf("checksum error\n");
                    seqx = 1;
                    break;
                }
                // if error is timeout, resend packet and stay on this state
                else if(sock == -3){
                    seqx = 1;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqy + 1;
                    hdtemp.windowsize = WIN_SIZE;
                    writeSock(&hdtemp);
                    state = W_WAIT;
                    /* printf("Resend-seqx = %d\nResend-seqy = %d\nResend-crc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc); */
                    printPacket(hdtemp);
                    break;
                }
                // if no errors, send ACK and go to state DONE
                else{
                    if(hdtemp.windowsize == WIN_SIZE && hdtemp.ACK == seqx +1){
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqy + 1;
                        writeSock(&hdtemp);
                        /* printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc); */
                        printPacket(hdtemp);
                        state = DONE;
                    }
                    else{
                        int ret;
                        while(1){
                            ret = poll(&fd, 1, -1); 
                            if(ret == -1)
                                printf("error: %s\n", strerror(errno));
                            else{
                                printf("packet found\n");
                                state = W_WAIT;
                                break;
                            }
                        }
                    }
                }
                sleep(1);
                /* if recieve WIN_SIZE and id -> send ACK=x+2 */
                /* if timeout -> resend ACK=y+1 and WIN_SIZE */
                break;
            case DONE:
                printf(">Three Way Handshake Done\n");
                /* if we entered the DONE state we want to go to the SW state */
                /* printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc); */
                //printPacket(hdtemp);
                return SW;

            default:
                break;

        }
    }
}

int SW_loop(){
    printf("in SW\n");
}

int TD_loop(){
    int state = START;
    
    while(1){
        switch(state){
            /* case START: */
            
            /* case W1: */

            /* case W2: */

            /* case DONE: */



        }

    }
}

int main(int argc, char *argv[]) {
#define TWH 0
#define SW 1
#define TD 2
    recvDataGram.flags = 0;
    recvDataGram.ACK = 0;
    recvDataGram.seq = 0;
    recvDataGram.windowsize = 4;
    recvDataGram.crc = 0;
    recvDataGram.data = "";
    char buffer[MAXLINE];
    char *hello = "Hello from client";
    int state = TWH;

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf(" |-CLIENT|-\n");
    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server informatio
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    while(1){
        switch(state){
            case TWH:
                state = TWH_loop();
                break;
            case SW:
                state = SW_loop();
                break;
            case TD:
                state = TD_loop();
                break;
        }
    }
    /* int n, len; */

    /* sendto(sockfd, &ok, sizeof(hd), */
    /*         MSG_CONFIRM, (const struct sockaddr *) &servaddr, */
    /*         sizeof(servaddr)); */
    /* printf("Hello message sent.\n"); */

    /* n = recvfrom(sockfd, (char *)buffer, MAXLINE, */
    /*         MSG_WAITALL, (struct sockaddr *) &servaddr, */
    /*         &len); */
    /* buffer[n] = '\0'; */
    /* printf("Server : %s\n", buffer); */

    /* close(sockfd); */
    /* return 0; */
}

