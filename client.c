/* 
 * assignment1  Yan Zhu W1357145
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
#include <signal.h>
#include <sys/time.h>


#define SERVERPORT "1337"    // the port users will be connecting to
#define BUF_SIZE 300
#define PACKET_NUM 11        // 5 correct packets; then 1 correct and 4 erro packets;  then send 1 correct packets but no ACK, use ack_timer
#define HOST "127.0.0.1"

//The first 7 bytes of the packet
struct packet {
    unsigned short int start;
    unsigned short int client_id:8;
    unsigned short int type;
    unsigned short int segment_no:8;
    unsigned short int length:8;
} send_buf = {0xFFFF, 0x01, 0xFFF1, 0x0, 0xFF};

 struct ackpack {
     unsigned short int start;
    unsigned short int client_id:8;
    unsigned short int ack;
    unsigned short int segment_no:8;
    unsigned short int end;
} ackpack = {0xFFFF, 0x0, 0xFFF2, 0x0, 0xFFFF};

struct packetReject {
    unsigned short int start;
    unsigned short int client_id:8;
    unsigned short int type;
    unsigned short int code;
    unsigned short int segment_no:8;
    unsigned short int end;
} rej_packet = {0xFFFF, 0x0, 0xFFF3, 0x0, 0x0, 0xFFFF};

//The ending 2 bytes of the packet
struct endsign {
    unsigned short int end;
} end = {0xFFFF};

//Sending buffer
char buffer[BUF_SIZE];

//Recieving buffer
char recieving[BUF_SIZE];

//Payload buffer
char payload[256];
struct addrinfo hints, *servinfo, *p;
unsigned short int error_code;
unsigned short int reject_code;
int sockfd;

//void sending(int, int, int);

void send_normal(unsigned short int);

void send_error1();

void send_error2();

void send_error3();

void send_error4();

int main() {
    
    int rv;
    int numbytes;
    int packet_num = 0;
    struct timeval timeout;
    int count;
    int code;
    
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    
    if ((rv = getaddrinfo(HOST, SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    
    //Set Recv timer
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
    
    // Send
    unsigned short int k = 1;
    for (packet_num = 0; packet_num < PACKET_NUM; packet_num++) {
        if(packet_num == 0){
            printf("Send five correct packet:\n");
        }
        if(packet_num == 5) {
            printf("Send one correct packet and four error packets:\n");
        }
        printf("Enter your payload:\n");
        bzero(payload, 255);
        gets(payload);
        send_buf.type = 0XFFF1;
        send_buf.length = strlen(payload);

        // send 5 normal packages
        if (packet_num < 5) {
            send_normal(k);
            k++;
        }
        else if (packet_num == 5) send_normal(k);
        else if (packet_num == 6) send_error1();
        else if (packet_num == 7) send_error2();
        else if (packet_num == 8) send_error3();
        else if (packet_num == 9) send_error4();
        else if (packet_num == 10) send_normal(7);
        
        
        rv = recvfrom(sockfd, recieving, BUF_SIZE - 1, MSG_WAITALL, p->ai_addr, &p->ai_addrlen);
        
        //Time out
        count = 0;
        while (rv == -1) {
            if (count == 3) {
                perror("No server response.\n\n");
                exit(1);
            }
            count++;
            printf("Packet %d . No response %d time\n", send_buf.segment_no, count);
            rv = recvfrom(sockfd, recieving, BUF_SIZE - 1, MSG_WAITALL, p->ai_addr, &p->ai_addrlen);
            if (rv > 0) break;
            sendto(sockfd, buffer, sizeof(send_buf) + strlen(payload) + 2, 0, p->ai_addr, p->ai_addrlen);
            
        }
        //only copy the first 5 bytes
        memcpy(&send_buf, recieving, 5);
        printf("From receving packet's type: \"%#X\"\n");

        //  receive ack from server
        if(send_buf.type == 0xFFF2) {
            printf("Reveiced Ack from server %#X \n\n");
        }
        
        //if rej_packet, set rej_packet error code separately.
        if (send_buf.type == 0xFFF3) {
            
            memcpy(&rej_packet, recieving, sizeof(rej_packet));
            
            if (rej_packet.code == 0xFFF4)
                printf("Reject: Packet out of sequence. %#X \n\n", rej_packet.code);
            
            else if (rej_packet.code == 0xFFF5)
                printf("Reject: Packet length does not match. %#X \n\n", rej_packet.code);
            
            else if (rej_packet.code == 0xFFF6)
                printf("Reject: End of packet lost. %#X \n\n", rej_packet.code);
            
            else if (rej_packet.code == 0xFFF7)
                printf("Reject: Duplicate packet. %#X \n\n", rej_packet.code);
            
        }
        
    }
    
    freeaddrinfo(servinfo);
    close(sockfd);
    
    return 0;
}


void send_normal(unsigned short int k) {
    printf("This is a normal package!\n");
    send_buf.type = 0XFFF1;
    send_buf.segment_no = k;
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &send_buf, sizeof(send_buf));
    memcpy(buffer + sizeof(send_buf), payload, strlen(payload));
    memcpy(buffer + sizeof(send_buf) + strlen(payload), &end, 2);
    sendto(sockfd, buffer, sizeof(send_buf) + strlen(payload) + 2, 0, p->ai_addr, p->ai_addrlen);
}

//packet number is out of sequence
void send_error1() {
    printf("send a out-of-sequence package!\n");
    send_buf.segment_no = 15;
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &send_buf, sizeof(send_buf));
    memcpy(buffer + sizeof(send_buf), payload, strlen(payload));
    memcpy(buffer + sizeof(send_buf) + strlen(payload), &end, 2);
    sendto(sockfd, buffer, sizeof(send_buf) + strlen(payload) + 2, 0, p->ai_addr, p->ai_addrlen);
}

//payload length does not match
void send_error2() {
    printf("This is a wrong payload length packeage!\n");
    send_buf.segment_no = 8;
    send_buf.length = 0;
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &send_buf, sizeof(send_buf));
    memcpy(buffer + sizeof(send_buf), payload, strlen(payload));
    memcpy(buffer + sizeof(send_buf) + strlen(payload), &end, 2);
    sendto(sockfd, buffer, sizeof(send_buf) + strlen(payload) + 2, 0, p->ai_addr, p->ai_addrlen);
}

//end of Packet id missing
void send_error3() {
    printf("This is a end_missing package!\n");
    send_buf.segment_no = 9;
    //send_buf.length = 0xFF;
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &send_buf, sizeof(send_buf));
    memcpy(buffer + sizeof(send_buf), payload, strlen(payload));
    //memcpy(buffer + sizeof(send_buf) + strlen(payload));
    sendto(sockfd, buffer, sizeof(send_buf) + strlen(payload) + 2, 0, p->ai_addr, p->ai_addrlen);
}

//duplicated number
void send_error4() {
    printf("This is a duplicated package!\n");
    send_buf.segment_no = 9;
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &send_buf, sizeof(send_buf));
    memcpy(buffer + sizeof(send_buf), payload, strlen(payload));
    memcpy(buffer + sizeof(send_buf) + strlen(payload), &end, 2);
    sendto(sockfd, buffer, sizeof(send_buf) + strlen(payload) + 2, 0, p->ai_addr, p->ai_addrlen);
    
}
