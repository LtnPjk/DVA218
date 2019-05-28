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
#include <poll.h>
#include <time.h>

#define PORT     5555
#define MAXLINE 1024

#define START 0
#define R_WAIT 1
#define R_WAIT2 2
#define DONE 3

#define W1 1

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

int sockfd;
//void logic_loop();
char buffer[MAXLINE];
char *hello = "Hello from server";
struct sockaddr_in servaddr, cliaddr;
int len, n;
int seqy = 0;
int seqx = 0;
int lastseqx = 0;
int win_size;
hd hdtemp;

//hd *recvDataGram;
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
/* has a chance of changing the packet */
int destroyPacket(hd *dg){
    int Case;
    if((rand() % 4) == 0){
        Case = rand() % 3;
        switch(Case){
            /* sequence number order */
            case 0:
                printf("packet change: order\n");
                if((rand() % 1) == 1)
                    dg->seq++;
                else
                    dg->seq--;
                return 2;
            /* packet corruption */
            case 1:
                printf("packet change: corruption\n");
                /* char randomletter = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[random () % 26]; */
                /* strcpy(dg->data, &randomletter); */
                dg->data = "Corrupted data";
                return 0;
            /* lost packet */
            default:
                printf("packet change: lost\n");
                return 1;
        }
    }
    return 0; //do nothing to packet
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
    int ret;
    // calc checksum
    /* dg->crc = checksum16((uint16_t*)&(dg->flags), (sizeof(hd)-sizeof(uint16_t))/2); */
    /* printf("crc: %u\n", (unsigned int)dg->crc); */
    //incr seq
    seqy++;
    dg->seq = seqy;
    dg->crc = checksum((void *)&(dg->flags), 30);
    // send data
    /* if packet is to be lost */
    if((ret = destroyPacket(dg)) == 1){
        /* seqy--; */
        return;
    }
    /* if sequence number is to be changed, a new crc must be calculated */
    else if(ret == 2)
        dg->crc = checksum((void*)&(dg->flags), 30);

    if(sendto(sockfd, dg, sizeof(hd),
                MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                sizeof(cliaddr)) == -1){
        printf("failed to send - %s\n", strerror(errno));
    }
}

struct pollfd fd;

// returns -1 if error, -2 if checksum error, -3 if timeout
int readSock(hd *recvDataGram, int timeout){
    int ret;
    fd.fd = sockfd; // socket file descriptor
    fd.events = POLLIN;
    ret = poll(&fd, 1, timeout); // timeout = time in milliseconds
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
                    (struct sockaddr *) &cliaddr, &len);
            seqx = recvDataGram->seq;
            if(n == -1)
                return -1;
            // do checksum-test
            /* uint16_t check = checksum16((uint16_t*)&(recvDataGram->flags), (sizeof(hd)-sizeof(uint16_t))/2); */
            /* printf("crc: %u\n", (unsigned int)check); */
            //uint16_t check = 0;
            //uint16_t check = checksum16(recvDataGram->data, recvDataGram->length);
            uint16_t check = checksum((void *)&(recvDataGram->flags), 30);
            if(check != recvDataGram->crc){
                printPacket(*recvDataGram);
                printf("LOCAL CHKSUM: %u \n", check);
                return -2;
            }
            else
                return 0;
    }
}

/* Three way handshake loop, returning the next state in main() */
int TWH_loop(){
    printf("---Three Way Handshake---\n");
    int state = START;
    int sock;
    int errorCount = 0;
    memset(&hdtemp, 0, sizeof(hdtemp));
    while(1){
        switch(state){
            /* starting state, waiting to recieve SYN */
            case START:
                printf("WAITING FOR SYN\n");
                /* reading socket and checking for errors */
                if((sock = readSock(&hdtemp, -1)) == -1){ // -1 argument = infinite timeout
                    perror("Could not read socket\n");
                }
                else if(sock == -2){
                    printf("checksum error\n");
                    state = START;
                    break;
                }
                /* if no errors*/
                else{
                    /* right package, send ACK+SYN */
                    if(hdtemp.flags == SYN && hdtemp.seq == lastseqx + 1){
                        lastseqx = hdtemp.seq;
                        printf("recieved SYN\n");
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        //hdtemp.crc = 1;
                        hdtemp.ACK = seqx + 1;
                        hdtemp.flags = SYN;
                        printf("sending ACK+SYN\n");
                        writeSock(&hdtemp);
                        //printPacket(hdtemp);
                        state = R_WAIT;
                    }
                    /* wrong packet, restart the START case and wait again */
                    else
                        printf("recieved wrong packet\n");
                }
                break;
                /* state waiting for ACK+WIN_SIZE */
            case R_WAIT:
                printf("WAITING FOR ACK+WIN_SIZE\n");
                /* reading socket and checking for errors */
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("Could not read socket\n");
                }
                /* checksum error, wait for resend */
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
                            state = R_WAIT;
                            break;
                        }
                    }
                    break;
                }
                /* if error is timeout, resend previous packet and stay on this state */
                else if(sock == -3){
                    printf("TIMEOUT\n");
                    if(seqy > 0)
                        seqy--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqx + 1;
                    hdtemp.flags = SYN;
                    printf("resending ACK+SYN\n");
                    writeSock(&hdtemp);
                    state = R_WAIT;
                    //printPacket(hdtemp);
                    break;
                }
                /* if no errors*/
                else{
                    /* right package, send ACK+WIN_SIZE */
                    if(hdtemp.ACK == seqy + 1 && hdtemp.windowsize != 0 && hdtemp.seq == lastseqx + 1){
                        lastseqx = hdtemp.seq;
                        printf("recieved ACK+WIN_SIZE\n");
                        win_size = hdtemp.windowsize;
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        //hdtemp.crc = 1;
                        hdtemp.windowsize = win_size;
                        hdtemp.ACK = seqx + 1;
                        printf("sending ACK+WIN_SIZE\n");
                        writeSock(&hdtemp);
                        //printPacket(hdtemp);
                        state = R_WAIT2;
                    }
                    else if(hdtemp.flags == SYN){
                        if(seqy > 0)
                            seqy--;
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqx + 1;
                        hdtemp.flags = SYN;
                        printf("resending ACK+SYN\n");
                        writeSock(&hdtemp);
                        state = R_WAIT;
                        //printPacket(hdtemp);
                        break;

                    }
                    /* wrong package, wait for resend */
                    else{
                        printf("recieved wrong packet, waiting for new packet\n");
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
                /* state waiting for final ACK from the client */
            case R_WAIT2:
                printf("WAITING FOR FINAL ACK\n");
                /* read socket and check for errors */
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("Could not read socket\n");
                }
                /* checksum error */
                else if(sock == -2){
                    printf("checksum error\n");
                    seqy = 1;
                    break;
                }
                /* if error is timeout, resend previous packet and stay on this state */
                else if(sock == -3){
                    printf("TIMEOUT\n");
                    if(seqy > 0)
                        seqy--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.windowsize = win_size;
                    hdtemp.ACK = seqx + 1;
                    printf("resending ACK+WIN_SIZE\n");
                    writeSock(&hdtemp);
                    state = R_WAIT2;
                    //printPacket(hdtemp);
                    break;
                }
                /* if no erros*/
                else{
                    /* right package, go to state DONE */
                    if(hdtemp.ACK == seqy + 1 && hdtemp.seq == lastseqx + 1){
                        printf("recieved final ACK\n");
                        state = DONE;
                    }
                    /* we will recieve this packet if client is still in R_WAIT */
                    else if(hdtemp.ACK == seqy + 1 && hdtemp.windowsize != 0 && hdtemp.seq == lastseqx + 1){
                        lastseqx = hdtemp.seq;
                        printf("recieved ACK+WIN_SIZE\n");
                        win_size = hdtemp.windowsize;
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        //hdtemp.crc = 1;
                        hdtemp.windowsize = win_size;
                        hdtemp.ACK = seqx + 1;
                        printf("sending ACK+WIN_SIZE\n");
                        writeSock(&hdtemp);
                        //printPacket(hdtemp);
                        state = R_WAIT2;
                    }
                    /* wrong package, wait for resend */
                    else{
                        errorCount++;
                        if(errorCount > 4){
                            printf("guessing that client is in SW\n");
                            state = DONE;
                            break;
                        }
                        printf("recieved wrong packet\n");
                        int ret;
                        while(1){
                            ret = poll(&fd, 1, -1);
                            if(ret == -1)
                                printf("error: %s\n", strerror(errno));
                            else{
                                printf("packet found\n");
                                state = R_WAIT2;
                                break;
                            }
                        }
                    }
                }
                //printPacket(hdtemp);
                break;
                /* state DONE, there should now be a connection */
            case DONE:
                printf(">Three Way Handshake Done\n");
                //printPacket(hdtemp);
                return SW;

            default:
                break;
        }
    }
}
// returns next state-machine to execute
int SW_loop(){
    printf("---Sliding Window---\n");
    int sock;
    int state = 0;
    hd dgram_s;
    hd dgram_r;
    int lastACK = seqx;
    int prevPack = seqx;

    while(1){
        //clear memory for datagram
        memset(&dgram_s, 0, sizeof(dgram_s));
        //set datagram window size
        dgram_s.windowsize = win_size;
        if((sock = readSock(&dgram_r, 1000)) == -1){
            perror("could not read socket\n");
        }
        //if sock < 0, something went wrong, handle all that here
        else if(sock < 0){
            printf("ERROR: %d\n", sock);
            dgram_s.ACK = lastACK;
            writeSock(&dgram_s);
        }
        //if we could read datagram without errors, send ACK
        else if(sock >= 0){
            if(dgram_r.seq != prevPack + 1){
                dgram_s.ACK = prevPack + 1;
            }
            else{
                dgram_s.ACK = dgram_r.seq + 1;
            }
            //lastACK = dgram_r.seq;
            printf("SENDING ACK FOR PACKAGE: %d\n", dgram_s.ACK);
            if(dgram_r.flags == FIN){
                dgram_s.flags = FIN;
            }
            writeSock(&dgram_s);
            //printPacket(dgram_r);
            prevPack = dgram_r.seq;
            //if the recieved datagram has the FIN flag set, return TD to initiate tear-down
            if(dgram_r.flags == FIN){
                printf("FIN\n");
                return TD;
            }
        }

    }
}
/* returns next state-machine to execute */
int TD_loop(){
    printf("---Teardown---\n");
    printf("seqx: %d, seqy: %d\n", seqx, seqy);
    int state = START;
    int sock;
    lastseqx = seqx;
    while(1){
        switch(state){
            /* starting state, if recieve FIN, send FINACK */
            case START:
                lastseqx = hdtemp.seq;
                printf("recieved FIN\n");
                memset(&hdtemp, 0, sizeof(hdtemp));
                //hdtemp.crc = 1;
                hdtemp.ACK = seqx + 1;
                hdtemp.flags = FIN;
                printf("sending ACK+FIN\n");
                writeSock(&hdtemp);
                //printPacket(hdtemp);
                state = W1;
                break;

                /* state waiting for ACK */
            case W1:
                printf("WAITING FOR ACK\n");

                /* read socket and check for errors */
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("could not read socket\n");
                }

                /* checksum error, wait for resend */
                else if(sock == -2){
                    printf("checksum error - waiting for resend\n");
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
                }

                /* timeout error, resend ACK+FIN */
                else if(sock == -3){
                    printf("TIMEOUT\n");
                    if(seqy > 0)
                        seqy--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqx + 1;
                    hdtemp.flags = FIN;
                    printf("resending ACK+FIN\n");
                    writeSock(&hdtemp);
                    //printPacket(hdtemp);
                    state = W1;
                    break;
                }

                /* if no errors */
                else{
                    /* right packet, go to state DONE */
                    if(hdtemp.ACK == seqy + 1){
                        printf("recieved final ACK\n");
                        state = DONE;
                        break;
                    }
                    else if(hdtemp.flags == FIN){
                    if(seqy > 0)
                        seqy--;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqx + 1;
                    hdtemp.flags = FIN;
                    printf("resending ACK+FIN\n");
                    writeSock(&hdtemp);
                    //printPacket(hdtemp);
                    state = W1;
                    break;
                    }

                    /* wrong packet, resend ACK+FIN */
                    else{
                        printf("recieved wrong packet\n");
                        printPacket(hdtemp);
                        if(seqy > 0)
                            seqy--;
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.ACK = seqx + 1;
                        hdtemp.flags = FIN;
                        printf("resending ACK+FIN\n");
                        writeSock(&hdtemp);
                        printPacket(hdtemp);
                        state = W1;
                        break;
                    }
                }
                break;

                /* Teardown done */
            case DONE:
                printf("Teardown complete\n");
                return EXIT;
            default:
                printf("default cd \n");
                break;
        }
    }

}

// Driver code
int main(){
    srand(time(NULL));
    printf("size of hd %d\n", sizeof(hd));
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf(" ---SERVER---\n");
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
    int nextMachine = TWH;
    while(1){
        switch(nextMachine){
            case TWH:
                nextMachine = TWH_loop();
                break;
            case SW:
                nextMachine = SW_loop();
                break;
            case TD:
                nextMachine = TD_loop();
                break;
            case EXIT:
                close(sockfd);
                return 0;
        }
    }
    return 0;
}
