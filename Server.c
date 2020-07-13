


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netdb.h>
#include<pthread.h>

#define FILE_NAME "IMG.MOV"
#define IP "127.0.0.1"
#define PORT_TCP 10059
#define PORT_UDP 10055
#define BUFLEN 4096
#define NUM_thread 10

pthread_t tid[NUM_thread],tcptid[NUM_thread];
struct sockaddr_in my_addr, client_addr;
int socket_desc , client_sock , c , sockfd;
socklen_t slen=sizeof(client_addr);

int init_udp_con[3]={PORT_UDP,0,0}; // the initializing data to be sent to Client (chunk size, number of chunks, port number)

struct chunk{
    int c_id;
    char c_data[BUFLEN];
}my_chunk[100000];

pthread_mutex_t my_lock=PTHREAD_MUTEX_INITIALIZER;
void *tcp_worker(void *);
void *udp_worker(void *);
int tcp_connection(void);
int udp_connection(void);
int read_file(void);

int main()
{
    int chunk_num=read_file();      // read the file and return number of chunks
    printf("chunk counter %d \n",chunk_num);
    init_udp_con[2]=chunk_num;      // complete the initializations
    init_udp_con[1]=BUFLEN;
    
    if(tcp_connection()<1)
        printf("TCP Connection with client failed");
    
    send(client_sock , init_udp_con, sizeof(init_udp_con),0);   // send the init meta-data to Client over TCP
    
    if(udp_connection()<1)
        printf("UDP Connection with client failed");
    
    int ID[NUM_thread], t;
    for(int i=0;i<NUM_thread;i++){      // activating threads
        ID[i]=i;
        t=pthread_create(&(tid[i]), NULL, udp_worker, (void *)&ID[i]);
    }
    
    for(int i=0;i<NUM_thread;i++){
        pthread_join(tid[i], NULL);
        printf("UDP thread join %d  ...\n",i);
    }
    for(int i=0;i<NUM_thread;i++){
        pthread_join(tcptid[i], NULL);
        printf("TCP thread join %d  ...\n",i);
    }
    
    printf("End of transmission! \n");
    
    int missed[100000];
    recv(client_sock, &missed, sizeof(missed), 0);    // receiving NACK from client, bitmap of missing chunks

    int j=0;
    while (j<chunk_num) {   // resend those missing chunks
        if (missed[j]==0){
            printf("%d was missed. Sent now! \n",j);
            sendto(sockfd, &my_chunk[j], sizeof(my_chunk[0]), 0, (struct sockaddr*)&client_addr, slen);
        }
        j++;
    }
    
    int xx=0;
    recv(client_sock, &xx, sizeof(xx), 0);
        
    close(sockfd);
    close(client_sock);
    close(socket_desc);
    return 0;
}

void *udp_worker(void *My_ID)
{
    int my_id= *(int *)My_ID;
    printf("Thread %d started.. \n", my_id);
    //pthread_mutex_lock(&my_lock);
    
    int x=init_udp_con[2];  // number of chunks
    int y=NUM_thread;       // number of threads
    int domain=(x / y + (x % y > 0));   // #chunks devided by #threads rounded up, to find the dimensions of each loop
    for (int j=my_id*domain;j<(my_id+1)*domain;j++){
        if(j<x) {   // if chunk is not empty send it
            sendto(sockfd, &my_chunk[j], sizeof(my_chunk[j]), 0, (struct sockaddr*)&client_addr, slen) ;
            printf("Chunk %d sent\n",my_chunk[j].c_id);}
        else
            break;
    }
    
    pthread_create(&(tcptid[my_id]), NULL, tcp_worker, My_ID);     // receiving SACK from Client before ending thread
    //pthread_mutex_unlock(&my_lock);
    printf("Thread %d terminated.\n", my_id);
    
    return NULL;
}
void *tcp_worker(void *My_domain)
{
    int my_dom;
    if((my_dom = *(int *)My_domain)==0)
        perror("thread error");
    
    int recv_arr[my_dom];
    recv(client_sock, recv_arr, sizeof(recv_arr), 0);    // receiving SACK from Client before ending thread
    
    printf("TCP Thread %d terminated.\n", my_dom);
    
    return NULL;
}

int read_file(){
    FILE *fp;
    fp = fopen(FILE_NAME, "rb");
    if (fp == NULL){
        perror("File Error \n");
        exit(EXIT_FAILURE);
    }
    
    int chunk_counter=0;
    while(fread(my_chunk[chunk_counter].c_data,BUFLEN,1,fp)>0){
        my_chunk[chunk_counter].c_id=chunk_counter;
        printf("Chunk %d is read from file\n",chunk_counter);
        chunk_counter++;
    }
    my_chunk[chunk_counter].c_id=chunk_counter;
    printf("Last chunk %d is read from file\n",chunk_counter);
    
    fclose(fp);
    return chunk_counter+1;
}

int tcp_connection()
{
    struct sockaddr_in server , client ;    //one for listening one for connection with client(s)
    char client_message[20000];
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    client_sock = socket(AF_INET , SOCK_STREAM , 0);
    
    printf("TCP Sockets created\n");
    
    server.sin_family = AF_INET;
    server.sin_port = htons( PORT_TCP );
    inet_pton (AF_INET, IP, &(server.sin_addr)) ;
    
    struct timeval tv;
    tv.tv_sec = 10;      // setting timeout for TCP as 10 sec
    tv.tv_usec = 0;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
        perror("bind failed. Error"); //perror is a very helpful function for tracking down errors
        return 0;
    }
    printf("bind completed \n");
    
    c = sizeof(struct sockaddr_in);
    listen(socket_desc , 1);
    
    printf("Waiting for incoming connections...\n");

    client_sock = accept(socket_desc, (struct sockaddr *)&client,  (socklen_t*)&c);
    //if successful, client is filled in with address of connecting socket
    if (client_sock < 0){
        perror("accept failed");
        return -1;
    }
    
    recv(client_sock , client_message , 32 , 0) ;
    
    printf("Received msg is: %s \n",client_message ) ;
    printf("TCP Connection accepted\n");
    return 1;
}

int udp_connection()
{
    char buf[1024];
    sockfd = socket(AF_INET, SOCK_DGRAM, 0) ;
    printf("UDP Server : Socket() successful\n");
    
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT_UDP);
    inet_pton (AF_INET, IP, &(my_addr.sin_addr)) ;
    
    struct timeval tv;
    tv.tv_sec = 10;      // setting timeout for UDP as 10 sec
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
        printf("Bind error\n") ;
    else
        printf("Server : bind() successful\n");
    
    if (recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &slen)==-1)
        printf("ERROR IN RECVFROM\n") ;
    printf("Received packet from %s:        %d\nData: %s\n\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buf);
    
    return 1;
}


