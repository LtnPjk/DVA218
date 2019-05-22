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
//void logic_loop();
char buffer[MAXLINE];
char *hello = "Hello from server";
struct sockaddr_in servaddr, cliaddr;
int len, n;
int seqy = 0;
int seqx = 0;
int win_size;

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

void writeSock(hd *dg){
    // calc checksum
    /* dg->crc = Fletcher16(dg->data, dg->length); */
    /* //incr seq */
    seqy++;
    // send data
    if(sendto(sockfd, dg, sizeof(*dg),
                MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                sizeof(cliaddr)) == -1){
        printf("failed to send - %s\n", strerror(errno));
    }
}

// returns -1 if error, -2 if checksum error, -3 if timeout
int readSock(hd *recvDataGram, int timeout){
    struct pollfd fd;
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
            n = recvfrom(sockfd, recvDataGram, sizeof(*recvDataGram), MSG_WAITALL,
                    (struct sockaddr *) &cliaddr, &len);
            seqx = recvDataGram->seq;
            if(n == -1)
                return -1;
            // do checksum-test
             uint16_t check;
             if(check != recvDataGram->crc){
                return -2;
            }
            else
                return -4;
    }
}

int makePkt(){

}
int TWH_loop(){
    int state = START;
    int sock;
    hd hdtemp;
    memset(&hdtemp, 0, sizeof(hdtemp));
    while(1){
        switch(state){
            // starting state, waiting to recieve SYN
            case START:
                printf(">state: START\n");
                // reading socket and checking for errors
                if((sock = readSock(&hdtemp, -1)) == -1){ // -1 argument = infinite timeout
                    perror("Could not read socket\n");
                }
                else if(sock == -2){
                    printf("checksum error\n");
                    state = START;
                    break;
                }
                // if no errors, send SYN+ACK and go to state R_WAIT
                else{
                    if(hdtemp.flags == SYN && hdtemp.seq == 1){
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        /* hdtemp.data = "hejhej hahaaaaa\0"; */
                        /* hdtemp.length = sizeof(hdtemp.data); */
                        hdtemp.ACK = seqx + 1;
                        hdtemp.flags = SYN;
                        writeSock(&hdtemp);
                        state = R_WAIT;
                    }
                }
                printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc);
                break;
                // state waiting for ACK+WIN_SIZE
            case R_WAIT:
                printf(">state: R_WAIT\n");
                // reading socket and checking for errors
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("Could not read socket\n");
                }
                else if(sock == -2){
                    printf("checksum error\n");
                    seqy = 0;
                    break;
                }
                // if error is timeout, resend previous packet and stay on this state
                else if(sock == -3){
                    seqy = 0;
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    hdtemp.ACK = seqx + 1;
                    hdtemp.flags = SYN;
                    writeSock(&hdtemp);
                    state = R_WAIT;
                    printf("Resend-seqx = %d\nResend-seqy = %d\n", seqx, seqy);
                    break;
                }
                // if no errors, send ACK+WIN_SIZE and go to state R_WAIT2
                else{
                    if(hdtemp.ACK == seqy + 1 && hdtemp.windowsize != 0){
                        win_size = hdtemp.windowsize;
                        memset(&hdtemp, 0, sizeof(hdtemp));
                        hdtemp.windowsize = win_size;
                        hdtemp.ACK = seqx + 1;
                        writeSock(&hdtemp);
                        state = R_WAIT2;
                    }
                }
                printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc);

                break;
                // state waiting for final ACK from the client
            case R_WAIT2:
                printf(">state: R_WAIT2\n");
                // read socket and check for errors
                if((sock = readSock(&hdtemp, 1000)) == -1){
                    perror("Could not read socket\n");
                }
                else if(sock == -2){
                    printf("checksum error\n");
                    seqy = 1;
                    break;
                }
                // if error is timeout, resend previous packet and stay on this state
                else if(sock == -3){
                    memset(&hdtemp, 0, sizeof(hdtemp));
                    win_size = hdtemp.windowsize;
                    hdtemp.ACK = seqx + 1;
                    writeSock(&hdtemp);
                    state = R_WAIT2;
                    printf("Resend-seqx = %d\nResend-seqy = %d\n", seqx, seqy);
                    break;
                }
                // if no erros and we recieve right ACK, go to state DONE
                else{
                    if(hdtemp.ACK == seqy + 1){
                        state = DONE;
                    }
                }
                printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc);
                break;
                // state DONE, there should now be a connection
            case DONE:
                printf(">Three Way Handshake Done\n");
                printf("seqx = %d\nseqy = %d\ncrc = %u\n", seqx, seqy, (unsigned int)hdtemp.crc);
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
/* void logic_loop(){ */
/*     int nextMachine = 0; */
/*     while(1){ */
/*         printf("----SERVER----\n"); */
/*         hd *dGram; */
/*         /1* n = recvfrom(sockfd, dGram, MAXLINE, *1/ */
/*         /1*             MSG_WAITALL, ( struct sockaddr *) &cliaddr, *1/ */
/*         /1*             &len); *1/ */
/*         /1* buffer[n] = '\0'; *1/ */
/*         //Determine machine */
/*         switch (nextMachine){ */
/*             case TWH: */
/*                 nextMachine = TWH_loop(); */
/*                 break; */
/*             case SW: */
/*                 nextMachine = SW_loop(); */
/*                 break; */
/*             case TD: */
/*                 nextMachine = TD_loop(); */
/*                 break; */
/*             default: */
/*                 printf("DEFAULT\n"); */
/*                 sleep(2); */
/*                 break; */
/*         } */

/*         /1* printf("Client : %d\n", dGram->windowsize); *1/ */
/*         /1* sendto(sockfd, (const char *)hello, strlen(hello), *1/ */
/*         /1*         MSG_CONFIRM, (const struct sockaddr *) &cliaddr, *1/ */
/*         /1*         len); *1/ */
/*         /1* printf("Hello message sent.\n"); *1/ */
/*     } */
/* } */
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
        }
    }
    return 0;
}
