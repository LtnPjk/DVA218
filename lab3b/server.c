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

// Driver code
int main() {

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

#define TWH 0
#define SW 1
#define TD 2

hd *recvDataGram;

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

void writeSock(hd dg){
    // calc checksum
    dg.crc = Fletcher16(dg.data, dg.length);
    // send data
    sendto(sockfd, &dg, sizeof(hd),
        MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
            len);

}

// returns number of successfull bytes read, or -1 if recvfrom error, or -2 if checksum err
int readSock(){
    n = recvfrom(sockfd, recvDataGram, MAXLINE, MSG_WAITALL,
            (struct sockaddr *) &cliaddr, &len);
    if(n == -1)
        return -1;
    // do checksum-test
    uint16_t check = Fletcher16(recvDataGram->data, recvDataGram->length);
}
// returns next state-machine to execute
int TWH_loop(){

}
// returns next state-machine to execute
int SW_loop(){

}
// returns next state-machine to execute
int TD_loop(){

}
void logic_loop(){
    while(1){
        int nextMachine = 0;

        hd *dGram;
        /* n = recvfrom(sockfd, dGram, MAXLINE, */
        /*             MSG_WAITALL, ( struct sockaddr *) &cliaddr, */
        /*             &len); */
        /* buffer[n] = '\0'; */
        //Determine machine
        switch (nextMachine){
            case TWH:
                nextMachine = TWH_loop();
                break;
            case SW:
                nextMachine = SW_loop();
                break;
            case TD:
                nextMachine = TD_loop();
                break;
            default:
                break;
        }

        printf("Client : %d\n", dGram->windowsize);
        sendto(sockfd, (const char *)hello, strlen(hello),
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                len);
        printf("Hello message sent.\n");
    }
}
