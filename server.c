/* 
 * assignment1  Server  
    Yan Zhu W1357145
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MYPORT "1337"    // the port users will be connecting to
#define MAXBUFLEN 300
#define PACKET_NUM 10

// ackpack
struct ackpack {
    unsigned short int start;
    unsigned short int client_id:8;
    unsigned short int ack;
    unsigned short int segment_no:8;
    unsigned short int end;
} ackpack = {0xFFFF, 0x0, 0xFFF2, 0x0, 0xFFFF};

//REJECT packet
struct reject_packet {
    unsigned short int start;
    unsigned short int client_id:8;
    unsigned short int type;
    unsigned short int code;
    unsigned short int segment_no:8;
    unsigned short int end;
} reject_packet = {0xFFFF, 0x0, 0xFFF3, 0x0, 0x0, 0xFFFF};


//First 7 bytes of the packet
struct packet {
    unsigned short int start;
    unsigned short int client_id:8;
    unsigned short int packet_type;
    unsigned short int segment_no:8;
    unsigned short int length:8;
} rec_buf = {0x0, 0x0, 0x0, 0x0, 0x00};

//End 2 bytes of the packet
struct endsign {
    unsigned short int end;
} end = {0x0};

//Payload buffers
char payload[256];

//Recieving buffer
char recive_buffer[MAXBUFLEN];

struct addrinfo hints, *servinfo, *p;
struct sockaddr_storage their_addr;
socklen_t addr_len;
char s[INET6_ADDRSTRLEN];


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int main(void) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_storage their_addr;
    int packet_num;
    int pre_segment_no = -1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }
    
    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("Server failed to bind socket.\n");
            continue;
        }
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server failed to bind.\n");
            continue;
        }
        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "Server failed to bind socket.\n");
        return 2;
    }
    
    freeaddrinfo(servinfo);
    printf("Server is ready to receive...\n");
    addr_len = sizeof their_addr;
    
    for (packet_num = 0; packet_num < PACKET_NUM; packet_num++) {
        int rec;
        bzero(recive_buffer, MAXBUFLEN);
        bzero(payload, 256);
        memset(&rec_buf, 0, sizeof(rec_buf));
        //receive from client
        rec = recvfrom(sockfd, recive_buffer, MAXBUFLEN - 1, 0, (struct sockaddr *) &their_addr, &addr_len);
        
        if (rec == -1) {
            perror("Failed to receive!\n");
            exit(1);
        }
        //copy the first 7 bytes into address of rec_buf.
        memcpy(&rec_buf, recive_buffer, sizeof(rec_buf));
        //get the payload
        memcpy(payload, recive_buffer + sizeof(rec_buf), rec - sizeof(rec_buf) - 2);
        payload[strlen(payload)] = '\0';
        //get the end
        memcpy(&end, recive_buffer + rec - 2, 2);
        //check and show each parts
        printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *) &their_addr), s, sizeof s));
        //printf("listener: packet is %d bytes long\n", rec);
        printf("This is packet number %d \n", rec_buf.segment_no);
        //printf("start \"%#X\"\n", rec_buf.start);
        //printf("client Id \"%#X\"\n", rec_buf.client_id);
        //printf("packet type \"%#X\"\n", rec_buf.packet_type);
        printf("segment number \"%d\"\n", rec_buf.segment_no);
        printf("expected segment number \"%d\"\n", packet_num + 1);
        //printf("pre segment number \"%d\"\n", pre_segment_no);
        printf("length of payload \"%hu\"\n", rec_buf.length);
        printf("expected length of payload \"%lu\"\n", strlen(payload));
        printf("Payload: \"%s\"\n", payload);
        printf("Packet Ending  \"%#X\"\n", end.end);
        
        //complete other parts of REJECT packet or ACK packet, and send ACK or REJECT packets to client.
        bzero(recive_buffer, MAXBUFLEN);
        if (end.end != 0xFFFF) {
            printf("Reject End of Packet missing\n\n");
            reject_packet.code = 0xFFF6;
            reject_packet.client_id = rec_buf.client_id;
            reject_packet.segment_no = rec_buf.segment_no;
            memcpy(recive_buffer, &reject_packet, sizeof(reject_packet));
        } else if (pre_segment_no == rec_buf.segment_no) {
            printf("Reject Duplicated packets.\n\n");
            reject_packet.code = 0xFFF7;
            reject_packet.client_id = rec_buf.client_id;
            reject_packet.segment_no = rec_buf.segment_no;
            memcpy(recive_buffer, &reject_packet, sizeof(reject_packet));
        } else if (packet_num + 1 != rec_buf.segment_no) {
            printf("Reject out of sequence.\n\n");
            reject_packet.code = 0xFFF4;
            reject_packet.client_id = rec_buf.client_id;
            reject_packet.segment_no = rec_buf.segment_no;
            memcpy(recive_buffer, &reject_packet, sizeof(reject_packet));
        } else if (rec_buf.length != strlen(payload)) {
            printf("Reject length mismatch.\n\n");
            reject_packet.code = 0xFFF5;
            reject_packet.client_id = rec_buf.client_id;
            reject_packet.segment_no = rec_buf.segment_no;
            memcpy(recive_buffer, &reject_packet, sizeof(reject_packet));
        } else {

            // rec_buf.packet_type = 0xFFF2;
            // memcpy(recive_buffer, &rec_buf, sizeof(rec_buf) - 1);
            // end.end = 0xFFFF;
            // memcpy(recive_buffer + sizeof(rec_buf), &end, 2);

            printf("Ack \n\n");
            ackpack.client_id = rec_buf.client_id;
            ackpack.segment_no = rec_buf.segment_no;
            memcpy(recive_buffer, &ackpack, sizeof(ackpack));
        }
        
        //save the pre receiving segment number
        pre_segment_no = rec_buf.segment_no;
        
        if (sendto(sockfd, recive_buffer, sizeof(rec_buf) + 1, 0, (struct sockaddr *) &their_addr, addr_len) == -1) {
            perror("Failed to send!\n");
            exit(1);
        }
        
    }
    
    close(sockfd);
    return 0;
}
