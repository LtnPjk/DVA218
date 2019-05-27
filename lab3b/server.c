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
    // calc checksum
    dg->crc = checksum16((uint16_t*)&(dg->flags), (sizeof(hd)-sizeof(uint16_t))/2);
    printf("crc: %u\n", (unsigned int)dg->crc);
    //incr seq
    seqy++;
    dg->seq = seqy;
    // send data
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
            uint16_t check = checksum16((uint16_t*)&(recvDataGram->flags), (sizeof(hd)-sizeof(uint16_t))/2);
            printf("crc: %u\n", (unsigned int)check);
            //uint16_t check = 0;
            if(check != recvDataGram->crc){
                return -2;
            }
            else
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
                    /* wrong package, wait for resend */
                    else{
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
                return TD;
                default:
                break;
        }
    }
}
// returns next state-machine to execute
int SW_loop(){
    printf("in SW\n");
    int sock;
    int state = 0;
    hd dgram_s;
    hd dgram_r;
    int lastACK = seqx;

    while(1){
        memset(&dgram_s, 0, sizeof(dgram_s));
        if((sock = readSock(&dgram_r, 1000)) == -1){
            perror("could not read socket\n");
        }
        else if(sock < 0){
            printf("ERROR: %d\n", sock);
            dgram_s.ACK = lastACK;
        }
        else if(sock >= 0){
            lastACK = dgram_r.seq;
            dgram_s.ACK = dgram_r.seq + 1;
            writeSock(&dgram_r);

            if(dgram_r.flags == FIN)
                return TD;
        }

    }
}
/* returns next state-machine to execute */
int TD_loop(){
    printf("---Teardown---\n");
    int state = START;
    int sock;
    lastseqx = seqx;
    while(1){
        switch(state){
            /* starting state, if recieve FIN, send FINACK */
            case START:
                printf("WAITING FOR FIN\n");
                /* read socket and check for errors */
                if((sock = readSock(&hdtemp, -1)) == -1){
                    perror("could not read socket\n");
                }

                /* checksum error, wait for resend */
                else if(sock == -2){
                    printf("checksum error - waiting for resend\n");
                    break;
                }

                /* if no errors */
                else{
                    /* right packet, send ACK+FIN */
                    if(hdtemp.flags == FIN && hdtemp.seq == lastseqx + 1){
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
                    }

                    /* wrong packet, wait for resend */
                    else{
                        printf("recieved wrong packet - waiting for resend\n");
                        break;
                    }
                }

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
                    if(hdtemp.ACK == seqy + 1 && hdtemp.seq == lastseqx + 1){
                        printf("recieved final ACK\n");
                        state = DONE;
                        break;
                    }

                    /* wrong packet, resend ACK+FIN */
                    else{
                        printf("recieved wrong packet\n");
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
                }

                /* Teardown done */
            case DONE:
                printf("Teardown complete\n");
                return EXIT;
        }
    }

}

// Driver code
int main(){
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
