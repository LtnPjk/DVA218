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

#define W1 1
#define W2 2

#define SYN 1
#define FIN 2
#define RST 3

#define TWH 0
#define SW 1
#define TD 2
#define EXIT 5

typedef struct hd_struct {
    uint16_t crc;
    int flags;
    int ACK;
    int seq;
    int windowsize;
    char *data;
    int length;
    int id;
} hd;

hd recvDataGram;
int sockfd;
char buffer[MAXLINE];
char *hello = "Hello from server";
struct sockaddr_in servaddr, cliaddr;
int len, n;
int seqx = 0;
int seqy = 0;
int lastseqy = 0;
hd hdtemp;

uint16_t checksum(void* vdata,size_t length) {
    // Cast the data pointer to one that can be indexed.
    char* data=(char*)vdata;

    // Initialise the accumulator.
    uint32_t acc=0xffff;

    // Handle complete 16-bit blocks.
    for (size_t i=0;i+1<length;i+=2) {
        uint16_t word;
        memcpy(&word,data+i,2);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Handle any partial block at the end of the data.
    if (length&1) {
        uint16_t word=0;
        memcpy(&word,data+length-1,1);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Return the checksum in network byte order.
    return htons(~acc);
}
// returns checksum of input bit-string
uint16_t checksum16(uint16_t *data, size_t len){
    uint16_t checksum = 0;
    size_t evn_len = len - len%2; // round down to even number of words
    int i;
    for(i = 0; i < 10; i += 2){
        uint16_t val = (data[i] + data[i+1]) % 65536;
        checksum += val % 65536;
    }
    /* if(i < len) { // add last byte if data was rounded earlier */
    /*     checksum += data[i] % 65536; */
    /* } */
    return checksum;
}
void dgram_create(hd * dgram, int seq){
    memset(dgram, 0, sizeof(dgram));
    dgram->windowsize = hdtemp.windowsize;
    dgram->seq = seq;
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
    dg->crc = checksum((void*)&(dg->flags), 30);
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
            // RADEN NEDAN ÄR FUCKED
            /* uint16_t check = checksum16((uint16_t *)(recvDataGram->data), recvDataGram->length); */
            uint16_t check = checksum((void *)&(recvDataGram->flags), 30);
            if(check != recvDataGram->crc){
                return -2;
            }
            else
                //return strlen(recvDataGram->data);
                return -4;
    }
}

/* Three way handshake loop, returning the next state in main() */
int TWH_loop(){
    printf("---Three Way Handshake---\n");
    int state = START;
    int sock;
    memset(&hdtemp, 0, sizeof(hdtemp));
    while(1){
        switch(state){
            /* starting state, directly sending a SYN */
            case START:
                printf("READY TO SEND SYN\n");
                memset(&hdtemp, 0, sizeof(hdtemp));
                hdtemp.flags = SYN;
                printf("sending SYN\n");
                writeSock(&hdtemp);
                state = R_WAIT;
                printPacket(hdtemp);
                break;

                /* state waiting for SYN+ACK */
            case R_WAIT:
                printf("WAITING FOR ACK+SYN\n");

                /* read socket and check for errors */
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                    break;
                }

                /* if checksum error, wait for resend from server */
                else if(sock == -2){
                    printf("checksum error\n");
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

                /* if error is timeout, resend previous packet and stay on this state */
                else if(sock == -3){
                    printf("TIMEOUT\n");
                    if(seqx > 0)
                        seqx--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.flags = SYN;
                    printf("resending SYN\n");
                    writeSock(&hdtemp);
                    state = R_WAIT;
                    printPacket(hdtemp);
                    break;
                }

                /* if no erorrs*/
                else{
                    /* right package, send ACK+WIN_SIZE*/
                    if(hdtemp.flags == SYN && hdtemp.ACK == seqx + 1 && hdtemp.seq == lastseqy + 1){
                        lastseqy = hdtemp.seq;
                        printf("recieved ACK+SYN\n");
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqy + 1;
                        hdtemp.windowsize = WIN_SIZE;
                        printf("sending ACK+WIN_SIZE\n");
                        writeSock(&hdtemp);
                        printPacket(hdtemp);
                        state = W_WAIT;
                    }

                    /* wrong package, wait for resend*/
                    else{
                        printf("recieved wrong packet\n");
                        int ret;
                        while(1){
                            ret = poll(&fd, 1, -1); 
                            if(ret == -1)
                                printf("error: %s\n", strerror(errno));
                            else{
                                printf("new packet found\n");
                                state = R_WAIT;
                                break;
                            }
                        }
                    }
                }
                break;

                /* state waiting for ACK+WIN_SIZE */
            case W_WAIT:
                printf("WAITING FOR ACK+WIN:SIZE\n");

                /* read socket and check for errors */
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                }

                /* if checksum error, wait for resend */
                else if(sock == -2){
                    printf("checksum error, waiting for new packet\n");
                    int ret;
                    while(1){
                        ret = poll(&fd, 1, -1); 
                        if(ret == -1)
                            printf("error: %s\n", strerror(errno));
                        else{
                            printf("new packet found\n");
                            state = W_WAIT;
                            break;
                        }
                    }
                    break;
                }

                /* if error is timeout, resend packet and stay on this state */
                else if(sock == -3){
                    printf("TIMEOUT\n");
                    if(seqx > 0)
                        seqx--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqy + 1;
                    hdtemp.windowsize = WIN_SIZE;
                    printf("resending ACK+WIN_SIZE\n");
                    writeSock(&hdtemp);
                    state = W_WAIT;
                    printPacket(hdtemp);
                    break;
                }

                /* if no errors*/
                else{
                    /* right package, send ACK */
                    if(hdtemp.windowsize == WIN_SIZE && hdtemp.ACK == seqx +1 && hdtemp.seq == lastseqy + 1){
                        printf("recieved ACK+WIN_SIZE\n");
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqy + 1;
                        printf("sending ACK\n");
                        writeSock(&hdtemp);
                        printPacket(hdtemp);
                        state = DONE;
                    }

                    /* wrong package, wait for resend */ 
                    else{
                        printf("recieved wrong packet\n");
                        int ret;
                        while(1){
                            ret = poll(&fd, 1, -1); 
                            if(ret == -1)
                                printf("error: %s\n", strerror(errno));
                            else{
                                printf("new packet found\n");
                                state = W_WAIT;
                                break;
                            }
                        }
                    }
                }
                break;

                /* Three way handshake done */
            case DONE:
                printf(">Three Way Handshake Done\n");
                //printPacket(hdtemp);
                return SW;

            default:
                break;
        }
    }
}

int SW_loop(){
    printf("in SW\n");
    int sock;
    hd dgram_s;
    hd dgram_r;
    int lastACK = seqx;
    hd winBuff[3];

    while(1){
        //fyll fönster
        printf("WINSIZE: %d",hdtemp.windowsize);
        while(abs(seqx - lastACK) < hdtemp.windowsize){
            printf("SENDING DATAGRAM:\n");
            dgram_create(&dgram_s, seqx);
            writeSock(&dgram_s);
            printPacket(dgram_s);
        }
        //läs socket
        if((sock = readSock(&dgram_r, 1000)) == -1){
            perror("could not read socket\n");
        }
        else if(sock < 0){
            printf("ERROR: %d\n", sock);
        }
        else{
            lastACK = dgram_r.ACK;
        }
    }
}

/* Teardown loop, returning next state-machine to execute */
int TD_loop(){
    printf("---Teardown---\n");
    int state = START;
    int sock;
    lastseqy = seqy;
    while(1){
        switch(state){
            /* starting state, directly sending a FIN */
            case START:
                printf("READY TO SEND FIN\n");
                memset(&hdtemp, 0, sizeof(hdtemp));
                hdtemp.flags = FIN;
                printf("sending FIN\n");
                writeSock(&hdtemp);
                state = W1;
                //printPacket(hdtemp);
                break;

                /* state waiting for ACK+FIN */
            case W1:
                printf("WAITING FOR ACK+FIN\n");

                /* read socket and check for errors */
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                }

                /* checksum error, wait for resend*/
                else if(sock == -2){
                    printf("checksum error - wait for resend\n");
                    int ret;
                    while(1){
                        ret = poll(&fd, 1, -1);
                        if(ret == -1)
                            printf("error: %s\n", strerror(errno));
                        else{
                            printf("packet found\n");
                            state = W1;
                            break;
                        }
                    }
                    break;
                }

                /* timeout error, resend FIN*/
                else if(sock == -3){
                    printf("TIMEOUT\n");
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.flags = FIN;
                    printf("resending FIN\n");
                    writeSock(&hdtemp);
                    state = W1;
                    //printPacket(hdtemp);
                    break;
                }

                /* if no errors */
                else{
                    /* right package, send ACK */
                    if(hdtemp.flags == FIN && hdtemp.ACK == seqx +1 && hdtemp.seq == lastseqy + 1){
                        lastseqy = hdtemp.seq;
                        printf("recieved ACK+FIN\n");
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqy + 1;
                        printf("sending ACK\n");
                        writeSock(&hdtemp);
                        state = W2;
                        //printPacket(hdtemp);
                        break;
                    }

                    /* wrong package, wait for resend */
                    else{
                        printf("recieved wrong packet - waiting for resend\n");
                        int ret;
                        while(1){
                            ret = poll(&fd, 1, -1);
                            if(ret == -1)
                                printf("error: %s\n", strerror(errno));
                            else{
                                printf("new packet found\n");
                                state = W1;
                                break;
                            }
                        }
                    }
                    break;
                }

                /* case waiting for either a package or timeout */
            case W2:
                printf("WAITING FOR PACKAGE OR TIMEOUT\n");

                /* read socket and check for errors */
                if((sock = readSock(&hdtemp, 1500)) == -1){
                    perror("could not read socket\n");
                }

                /* checksum error, resend ACK */
                else if(sock == -2){
                    printf("checksum error\n");
                    if(seqx > 0)
                        seqx--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqy + 1;
                    printf("resending ACK\n");
                    writeSock(&hdtemp);
                    state = W2;
                    //printPacket(hdtemp);
                    break;
                }

                /* timeout error, this means that server is done, go to state DONE */
                else if(sock == -3){
                    printf("Timeout - DONE\n");
                    state = DONE;
                    break;
                }

                /* recieving a pcket means that the server is not done and probably in */
                /* its waiting state */
                else{
                    printf("recieved a packet:\n");
                    printPacket(hdtemp);
                    if(seqx > 0)
                        seqx--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqy + 1;
                    printf("resending ACK\n");
                    writeSock(&hdtemp);
                    state = W2;
                    //printPacket(hdtemp);
                    break;
                }

                /* Teardown done */
            case DONE:
                printf("Teardown Complete\n");
                return EXIT;
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
    printf(" ---CLIENT---\n");
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
            case EXIT:
                close(sockfd);
                return 0;
        }
    }
    return 0;
}

