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

#define PORT 5555
#define message 1024
#define MAXLINE 1024

#define START 0
#define R_WAIT 1
#define W_WAIT 2
#define DONE 3

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

hd ok;
int sockfd;

int TWH_loop(){
    int state = START;
    while(1){
        switch(state){
            case START:
                /* send SYN=x to server */
                break;
            case R_WAIT:
                /* if we recieve SYN=y and ACK=x+1, send ACK=y+1 and WIN_SIZE to server */
                /* if timeout -> resend the SYN=x */
                break;
            case W_WAIT:
                /* if recieve WIN_SIZE and id -> send ACK=x+2 */
                /* if timeout -> resend ACK=y+1 and WIN_SIZE */
                break;
            case DONE:
                /* if we entered the DONE state we want to go to the SW state */
                return 1;

            default:
                break;

        }
    }
}

int SW_loop(){

}

int TD_loop(){

}

int main(int argc, char *argv[]) {
#define TWH 0
#define SW 1
#define TD 2

    ok.flags = 0;
    ok.ACK = 0;
    ok.seq = 0;
    ok.windowsize = 4;
    ok.crc = 0;
    ok.data = "";
    char buffer[MAXLINE]; 
    char *hello = "Hello from client"; 
    struct sockaddr_in     servaddr; 
    int state = 0;

    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 

    memset(&servaddr, 0, sizeof(servaddr)); 

    // Filling server information 
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

