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
void logic_loop();
char buffer[MAXLINE];
char *hello = "Hello from server";
struct sockaddr_in servaddr, cliaddr;
int len, n;
int seqx = 0;
int seqy = 0;
hd hdtemp;

// returns checksum of input bit-string
uint16_t checksum16(uint16_t *data, size_t len){
    uint16_t checksum = 0;
    size_t evn_len = len - len%2; // round down to even number of words
    int i;
    for(i = 0; i < evn_len; i += 2){
        uint16_t val = (data[i] + data[i+1]) % 65536;
        checksum += val;
    }
    if(i < len) { // add last byte if data was rounded earlier
        checksum += data[i] % 65536;
    }
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
    //dg->crc = checksum16((uint16_t*)dg->data[sizeof(dg->crc)], (sizeof(hd)-sizeof(dg->crc)/2));
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
            uint16_t check = 0;
            if(check != recvDataGram->crc){
                return -2;
            }
            else
                //return strlen(recvDataGram->data);
                return -4;
    }
}

int TWH_loop(){
    printf("---Three Way Handshake---\n");
    int state = START;
    int sock;
    memset(&hdtemp, 0, sizeof(hdtemp));
    while(1){
        switch(state){
            // starting state, directly sending a SYN
            case START:
                printf("READY TO SEND SYN\n");
                /* send SYN=x to server */
                memset(&hdtemp, 0, sizeof(hdtemp));
                //hdtemp.crc = 1;
                hdtemp.flags = SYN;
                printf("sending SYN\n");
                writeSock(&hdtemp);
                state = R_WAIT;
                //printPacket(hdtemp);
                break;
                // state waiting for SYN+ACK
            case R_WAIT:
                //sleep(4);
                printf("WAITING FOR ACK+SYN\n");
                // read socket and check for errors
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                    break;
                }
                // if checksum error, wait for resend from server
                else if(sock == -2){
                    printf("checksum error\n");
                    /* if(seqx > 0) */
                    /*     seqx--; */
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
                    printf("TIMEOUT\n");
                    seqx = 0;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.flags = SYN;
                    printf("resending SYN\n");
                    writeSock(&hdtemp);
                    state = R_WAIT;
                    //printPacket(hdtemp);
                    break;
                }
                // if no erorrs, send ACK+WIN_SIZE
                else{
                    if(hdtemp.flags == SYN && hdtemp.ACK == seqx + 1){
                        printf("recieved ACK+SYN\n");
                        //seqy = hdtemp.seq;
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        /* sleep(3); */
                        //hdtemp.crc = 1;
                        hdtemp.ACK = seqy + 1;
                        hdtemp.windowsize = WIN_SIZE;
                        printf("sending ACK+WIN_SIZE\n");
                        writeSock(&hdtemp);
                        //printPacket(hdtemp);
                        state = W_WAIT;
                    }
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
                // state waiting for ACK+WIN_SIZE
            case W_WAIT:
                printf("WAITING FOR ACK+WIN:SIZE\n");
                //read socket and check for errors
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                }
                // if checksum error, wait for resend
                else if(sock == -2){
                    printf("checksum error, waiting for new packet\n");
                    //seqy = 0;
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
                    /* printf("checksum error\n"); */
                    /* if(seqx > 0) */
                    /*     seqx--; */
                    /* break; */
                }
                // if error is timeout, resend packet and stay on this state
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
                    //printPacket(hdtemp);
                    break;
                }
                // if no errors, send ACK and go to state DONE
                else{
                    if(hdtemp.windowsize == WIN_SIZE && hdtemp.ACK == seqx +1){
                        printf("recieved ACK+WIN_SIZE\n");
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqy + 1;
                        printf("sending ACK\n");
                        writeSock(&hdtemp);
                        //printPacket(hdtemp);
                        state = DONE;
                    }
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
                /* if recieve WIN_SIZE and id -> send ACK=x+2 */
                /* if timeout -> resend ACK=y+1 and WIN_SIZE */
                break;
            case DONE:
                printf(">Three Way Handshake Done\n");
                /* if we entered the DONE state we want to go to the SW state */
                /* printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc); */
                //printPacket(hdtemp);
                return TD;

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
        while(abs(seqx - lastACK) < hdtemp.windowsize){
            seqx++;
            dgram_create(&dgram_s, seqx);
            writeSock(&dgram_s);
        }
        //läs socket
        if((sock = readSock(&dgram_r, 1000)) == -1){
            perror("could not read socket\n");
        }
        else if(sock == -2){
            printf("CRC error\n");
            dgram_create(&dgram_s, seqx + 1);
            dgram_s.ACK = seqy + 1;
            writeSock(&dgram_s);
        }
        else if(sock == -3){
            printf("timeout error\n");
            //GÖR INGET FÖR TILLFÄLLET
        }
        else{
        }
    }

}

int TD_loop(){
    printf("---Teardown---\n");
    int state = START;
    int sock;
    while(1){
        switch(state){
            // starting state, directly sending a FIN
            case START:
                printf("READY TO SEND FIN\n");
                /* send FIN=x to server */
                memset(&hdtemp, 0, sizeof(hdtemp));
                //hdtemp.crc = 1;
                hdtemp.flags = FIN;
                printf("sending FIN\n");
                writeSock(&hdtemp);
                state = W1;
                //printPacket(hdtemp);
                break;

            case W1:
                printf("WAITING FOR ACK+FIN\n");
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                }
                // checksum error
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
                // timeout error
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
                else{
                    if(hdtemp.flags == FIN && hdtemp.ACK == seqx +1){
                        printf("recieved ACK+FIN\n");
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqy + 1;
                        printf("sending ACK\n");
                        writeSock(&hdtemp);
                        state = W2;
                        //printPacket(hdtemp);
                        break;
                    }
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

            case W2:
                printf("WAITING FOR PACKAGE OR TIMEOUT\n");
                if((sock = readSock(&hdtemp, 1500)) == -1){
                    perror("could not read socket\n");
                }
                // checksum error
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
                // timeout error
                else if(sock == -3){
                    printf("Timeout - DONE\n");
                    state = DONE;
                    break;
                }
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

