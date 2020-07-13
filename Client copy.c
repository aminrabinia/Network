
//  Protocol defined in Server
// -- Amin


#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h> 
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include<pthread.h>
#define BUFLEN 512
#define IP "127.0.0.1"
#define PORT_TCP 10059
#define FILE_NAME "test.mov"

const int NUM_thread=10;
pthread_t tid[NUM_thread],tcptid[NUM_thread];
int CHUNK_NUM, CHUNK_SIZE,PORT_UDP;
int buffer[BUFLEN];
struct sockaddr_in serv_addr, recv_addr ;
int slen=sizeof(serv_addr);
int tcp_sock,udp_sock;
int init_data[3];
int total=0;
int bitmap[100000];

struct chunk{
    int c_id;
    char c_data[4096];
}my_chunk[100000];

pthread_mutex_t my_lock=PTHREAD_MUTEX_INITIALIZER;

void *tcp_worker(void *[]);
void *worker(void *);
void cread_file(void);

int main(int argc, char** argv)
{
    char buf[BUFLEN];
    tcp_connection();
    int recv_size = recv(tcp_sock , init_data , sizeof(init_data) , 0);
    printf("Here are port num, chunk size, and chunk num: \n %d , %d, %d \n", init_data[0],init_data[1],init_data[2]);
    PORT_UDP=init_data[0];      // init the meta-data
    CHUNK_SIZE=init_data[1];
    CHUNK_NUM=init_data[2];
    
    for (int i=0;i<CHUNK_NUM;i++)
        bitmap[i]=0;            // bitmap set to zero
    
    udp_connection();

    int ID[NUM_thread], t;
    for(int i=0;i<NUM_thread;i++){      // activating threads
        ID[i]=i;
        t=pthread_create(&(tid[i]), NULL, worker, (void *)&ID[i]);
    }
    
    for(int i=0;i<NUM_thread;i++){
        pthread_join(tid[i], NULL);
        printf("end of udp threads join ++++++++++++++ %d \n",i);
    }
    for(int i=0;i<NUM_thread;i++){
        pthread_join(tcptid[i], NULL);
        printf("end of tcp threads++++++++++++++++ %d \n",i);
    }
    
    int k=0,j=0;
    while(k<CHUNK_NUM){ // printing the content of bitmap, missing chunks
        if (bitmap[k]==0){
            j++;
            printf("%d is missing \n",k);
        }
        k++;
    }
    
    printf("End of receiving! %d packets received from server. %d packets missing! \n",CHUNK_NUM-j, j); // sending NACK to the Server at the end of receiving process
    send(tcp_sock, &bitmap, sizeof(bitmap), 0);
    
    struct chunk temp_c;
    //for (int i=0; i<j; i++) {   // receiving the missing chunks from Server
    while(recvfrom(udp_sock,&temp_c,sizeof(temp_c), 0, (struct sockaddr *) &serv_addr, &slen)>0){
        my_chunk[temp_c.c_id]=temp_c;
        printf("Chunk %d received \n",temp_c.c_id);
        struct chunk temp_c;
    }
    
    printf("file to be made... \n");
    cread_file();       // create the file
    
    int xx=0;
    send(tcp_sock, &xx, sizeof(xx), 0);
    
    close(tcp_sock);
    close(udp_sock);
    return 0;
}

void *worker(void *My_ID)
{
    int my_id= *(int *)My_ID;
    printf("Thread %d started... \n", my_id);

    int x=CHUNK_NUM;
    int y=NUM_thread;
    int domain=(x / y + (x % y > 0));
    int arr[y];
    struct chunk temp_c;
    //for (int j=0;j<domain;j++){
    
        int j=0;
        do{
            struct chunk temp_c;
            
            j=recvfrom(udp_sock,&temp_c,sizeof(temp_c), 0, (struct sockaddr *) &serv_addr, &slen);
            if(strcmp(temp_c.c_data,"")!=0 && j>0){
            printf("Chunk id: %d \n",temp_c.c_id);       // id of received chunk
            pthread_mutex_lock(&my_lock);
            my_chunk[temp_c.c_id]=temp_c;
            bitmap[temp_c.c_id]=1;  // updata the bitmap
            pthread_mutex_unlock(&my_lock);
            }
        }while (j>0);
        arr[my_id]=temp_c.c_id; // keep record of chunks for SACK
    //}
    
    pthread_create(&(tcptid[my_id]), NULL, tcp_worker, (void *)&arr);
    printf("Thread %d ends. \n", my_id);
    
    
    
    return NULL;
}
void *tcp_worker(void *My_arr[])
{
    send (tcp_sock, &My_arr, sizeof(My_arr),  0) ;// frequently sending SACK to Server
    return NULL;
}


void cread_file(){
    FILE *fp;
    fp = fopen(FILE_NAME,"wb");
    for(int i=0;i<CHUNK_NUM;i++)
        fwrite(&my_chunk[i].c_data,CHUNK_SIZE,1,fp);
    printf("file made!!!\n");
}

struct sockaddr_in sa;
int tcp_connection()
{
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0) ;
    sa.sin_family   = AF_INET ;
    sa.sin_port     = htons(PORT_TCP) ;
    inet_pton(AF_INET, IP , &(sa.sin_addr)) ;
    
    struct timeval tv;
    tv.tv_sec =1;      // setting timeout for TCP as 10 sec
    tv.tv_usec = 0;
    setsockopt(tcp_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    printf("BEFOrE CONNECT\n") ;
    int x = connect(tcp_sock, (struct sockaddr *) &sa, sizeof(sa)) ;
    perror("Connet") ;
    
    printf ("Sending Message to Server!\n") ;
    send (tcp_sock, "TCP Client", 32,  0) ;
    
    return 0;
}

int udp_connection()
{
    char buf[BUFLEN];
    
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0))==-1)
        printf("socket");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT_UDP);
    inet_pton (AF_INET, IP,  &(serv_addr.sin_addr)) ;
    
    struct timeval tv;
    tv.tv_sec = 1;      // setting timeout for UDP as 1 sec
    tv.tv_usec = 0;
    setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    sprintf(buf,"UDP Client to Server!\n") ;
    if (sendto(udp_sock, buf, 32, 0, (struct sockaddr*)&serv_addr, slen)==-1)
        printf("sendto()");
    return 0;
}

