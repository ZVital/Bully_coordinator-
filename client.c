#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main() {

int sock;
struct sockaddr_in server;
	
sock=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
bind(sock,(struct sockaddr *)&server,sizeof(server));

int b=1;
if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&b, sizeof(b)) == -1) {
printf ("Error setting broadcast socket\n");
return -1;
}
memset(&server,0,sizeof(server));
server.sin_family=AF_INET;
server.sin_port=htons(1212);
//server.sin_addr.s_addr=inet_addr("255.255.255.255");
server.sin_addr.s_addr=inet_addr("192.168.11.253");
//server.sin_addr.s_addr=inet_addr("127.0.0.1");

char buf[80];
//for(;;){
char buffer[80];
memset(buf,0,80);
strcpy(buffer,"client");
if (sendto(sock,&buf,strlen(buf),0,(struct sockaddr *)&server,sizeof(server))<0) perror("sendto");
unsigned int server_addr_size=sizeof(server);
//recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr *)&server,&server_addr_size);
if (recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr *)&server,&server_addr_size)==-1) { 
printf("неверно принят адрес сервера");
return(-1);
}
//printf("%d",server_addr);
b=0;
if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&b, sizeof(b)) == -1) {
printf ("Error setting broadcast socket\n");
return -1;
}
int m_sock=socket(PF_INET,SOCK_STREAM,0);
    if(connect(m_sock,(struct sockaddr *)&server,server_addr_size)==0) puts("already connect to master");
    memset(buffer,0,80);
    strcpy(buffer,"client");
    if(write(m_sock,buffer,strlen(buffer))!=-1) puts("send client packet to master");
for (int brk=0;brk!=1; ){
//while ((recvfrom(sock,buf,80,0,NULL,0))!=-1){
memset(buffer,0,80);
if (read(m_sock,buffer,80)>0) printf("%s \n", buffer);
else brk=1;
}
return 0;
}
