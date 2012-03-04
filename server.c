#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

int main() {

	struct sockaddr_in addr;
	/* здесь инициализация addr */
	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(1212);
	addr.sin_addr.s_addr=INADDR_ANY;//inet_addr("127.0.0.1");
	/* здесь включение tcp-сервера на sock и udp-сервера на u_sock */	
	int sock, c_sock, u_sock, m_sock;	
	sock=socket(PF_INET,SOCK_STREAM,0);
	bind(sock,(struct sockaddr *)&addr,sizeof(addr));
	listen(sock,5);
	u_sock=socket(PF_INET,SOCK_DGRAM,0);
	bind(u_sock,(struct sockaddr *)&addr,sizeof(addr));
	/* определение наборов дескрипторов файлов и их максимального размера */
	fd_set rfds, afds, ufds, cfds, sfds;
	int nfds=getdtablesize();
	FD_ZERO(&ufds);
	FD_ZERO(&afds);
	FD_ZERO(&cfds);
	FD_ZERO(&sfds);
	FD_SET(sock,&afds);
	FD_SET(u_sock,&afds);
	FD_SET(u_sock,&ufds);

	/*Переводим udp в broadcast и бродкастим*/
	int mode=0;
	//	int bully=0;
	int slv_count=0;
	int snd_cnt=0;
	int broad=1;;
	if (setsockopt(u_sock, SOL_SOCKET, SO_BROADCAST, (char *)&broad, sizeof(broad)) == -1) {
		printf ("Error setting broadcast socket\n");
		return -1;
	}
	char buffer[81];
	memset(buffer,'0',81);
	strcpy(buffer,"master");
	addr.sin_addr.s_addr=inet_addr("192.168.11.253");
	if (sendto(u_sock,&buffer,strlen(buffer),0,(struct sockaddr *)&addr,sizeof(addr))<0) perror("sendto");
	puts("broadcast sended");
	/*Побродкастили и хватит*/
	broad=0;
	addr.sin_addr.s_addr=INADDR_ANY;
	if (setsockopt(u_sock, SOL_SOCKET, SO_BROADCAST, (char *)&broad, sizeof(broad)) == -1) {
		printf ("Error setting broadcast socket\n");
		return -1;
	}
	/*Ловим собственное эхо*/
	char r_buffer[81];
	memset(r_buffer,0,81);
	struct sockaddr_in sin;
	unsigned int sinlen=sizeof(sin);
	recvfrom(u_sock,&r_buffer,80,0,(struct sockaddr *)&sin,&sinlen);
	char my_ip[16];
	memset(my_ip,0,16);
	strcpy(my_ip, inet_ntoa(sin.sin_addr));
	printf("My IP %s \n", my_ip);

	/*Ждем 4 секуды эхо*/
	struct timeval tv;
	tv.tv_sec = 4;
	tv.tv_usec = 0;
	struct sockaddr_in master_sin;
	unsigned int msinlen=sizeof(master_sin);
	memcpy(&rfds, &ufds, sizeof(rfds));
	select(nfds, &rfds, NULL,NULL, &tv);

	if (FD_ISSET(u_sock, &rfds)) {	
		char r_buffer[81];
		memset(r_buffer,0,81);
		int size=recvfrom(u_sock,&r_buffer,80,0,(struct sockaddr *)&master_sin,&msinlen);
		char m_ip[16];
		memset(m_ip,0,16);	strcpy(m_ip, inet_ntoa(master_sin.sin_addr));
		printf("Master IP %s \n", m_ip);
		if(strncmp(buffer, r_buffer, size)==0)	mode=1;
	}
	for(;;) {
		if (mode==0){
			puts("master");
			for (;;) {
				tv.tv_sec = 2;
				tv.tv_usec = 0;
				memcpy(&rfds, &afds, sizeof(rfds));
				select(nfds, &rfds, NULL,NULL,&tv);

				if (FD_ISSET(sock, &rfds)) {
					c_sock=accept(sock,NULL,NULL);
					puts("incoming tcp");
					FD_SET(c_sock,&afds);
					slv_count++;
					continue;
				}
				for (int fd=0; fd<nfds; ++fd) if (fd == u_sock && FD_ISSET(u_sock,&rfds)) {
					struct sockaddr from;
					unsigned int len=sizeof(from);
					char buffer[81];
					memset(buffer,0,81);
					int size=recvfrom(u_sock,&buffer,80,0,&from,&len);
					sendto(u_sock,buffer,size,0,&from,len);
					printf("answer udp:%s \n",buffer);	
				} else if (fd != sock && FD_ISSET(fd,&rfds)) {
					char buffer[81];
					memset(buffer,0,81);
					int len=read(fd,buffer,80);
					if (len <=0) {
						puts("connection closed");
						shutdown(fd, 2);
						close(fd);
						FD_CLR(fd, &afds);
						FD_CLR(fd, &sfds);
						if (FD_ISSET(fd, &cfds)) {
							FD_CLR(fd, &cfds);
							slv_count--;
						}
					} else if (!strncmp(buffer, "master", 6)) {
						write(fd, buffer, len);
						puts("master ansver");
					}	
					/*Пересылаем всем ip нового сервера*/
					else if (!strncmp(buffer,"server", 6)){ int i=0;
						for (int fd2=0; fd2<nfds; ++fd2) {
							if (fd2!=sock && fd2!=fd && fd2!=u_sock && FD_ISSET(fd2, &afds)&&!FD_ISSET(fd2, &cfds)) {
								write(fd2,buffer,len);
								++i;
							}		
						}
						printf("%d slave addres received \n",i);
					} 
					/*добавлям клиента*/
					else if (!strncmp(buffer,"client", 6)){
						//FD_CLR(fd, &afds);
						FD_SET(fd, &cfds);
						slv_count--;
						puts("client add");
					} else {
						/*персылаем клиенту данные*/
						for (int fd1=0; fd1<nfds; ++fd1) {
							if (fd1!=sock && fd1!=fd && FD_ISSET(fd1, &cfds)) {
								write(fd1,buffer,len);
								printf("transferred %s",buffer); 
							}
						}
					}
				} // if in afds
				/*шлем собственные данные*/
				if (snd_cnt>=slv_count){
					snd_cnt=1;
					FILE *out=popen("ps aux|wc -l ","r");
					char buffer[81];
					memset(buffer,0,81);
					fgets(buffer,80,out);
					pclose(out);
					char symm[81];
					memset(symm,0,81);
					strcpy(symm, my_ip);
					strcat(symm, "\t");
					strcat(symm, buffer);
					for (int fd1=0; fd1<nfds; ++fd1) {
						if (fd1!=sock && FD_ISSET(fd1, &cfds)) {
							write(fd1,symm,strlen(symm));
							printf("send %s",symm);
						}
					}
				} else snd_cnt++;//отсылка своего
			} // for
		} //if(mode)
		else {
			puts("slave");
			/*Конектимся к мастеру*/
			if((m_sock=socket(PF_INET,SOCK_STREAM,0))==-1) puts("Eror create socket");
			else FD_SET(m_sock,&afds);
			if(connect(m_sock,(struct sockaddr *)&master_sin,msinlen)==0) puts("already connect to master");
			memset(buffer,0,81);
			strcpy(buffer,"server");
			strcat(buffer, my_ip);
			if(write(m_sock,buffer,strlen(buffer))!=-1) puts("send my ip to master");
			/*Начинаем слушать сокеты*/
			for (;mode!=0;) {
				tv.tv_sec = 2;
				tv.tv_usec = 0;
				memcpy(&rfds, &afds, sizeof(rfds));
				select(nfds, &rfds, NULL,NULL,&tv);

				if ( FD_ISSET(sock, &rfds) ) {
					c_sock=accept(sock,NULL,NULL);
					puts("incoming tcp");
					FD_SET(c_sock,&afds);
					slv_count++;
					continue;
				}
				for (int fd=0; mode!=0&&fd<nfds; ++fd)
					if ( fd == u_sock && FD_ISSET(u_sock,&rfds) ) {
						char buffer[81];
						memset(buffer,0,81);
						recvfrom(u_sock,&buffer,80,0,NULL,NULL);
						continue;
					} else if ( fd != sock && FD_ISSET(fd,&rfds) ) {
						char buffer[81];
						memset(buffer,0,81);
						int len=read(fd,buffer,80);
						if (len <=0) {
							puts("connection closed");
							shutdown(fd, 2);
							close(fd);
							FD_CLR(fd, &afds);
							FD_CLR(fd, &sfds);
							FD_CLR(fd, &cfds);
							if (fd==m_sock){
								puts("master died");
								mode=0;
								/*становимся мастером или получаем нового мастера*/
								for (int fdb=0; fdb<nfds; ++fdb) if (FD_ISSET(fdb,&sfds)) mode=1;
								printf("mode %d \n", mode);
								if (mode==1){
									memcpy(&rfds, &sfds, sizeof(rfds));
									select(nfds, &rfds, NULL,NULL,&tv);
									tv.tv_sec = 2;
									tv.tv_usec = 0;

									for (int fdb=0; fdb<nfds; ++fdb) if (FD_ISSET(fdb,&sfds)) {
										char buffer[81];
										memset(buffer,0,81);
										read(fdb,buffer,80);
										if (!strncmp(buffer, "master", 6)) {
											m_sock=fdb;
											FD_CLR(fd, &afds);
											puts("new master");
											continue;
										}	
									}
								} else {/*отправить всем master}*/
									char buffer[81];
								memset(buffer,0,81);
								strcpy(buffer,"master");
								for (int fd2=0; fd2<nfds; ++fd2) {
									if (fd2!=sock && fd2!=u_sock && FD_ISSET(fd2, &afds)) {
										write(fd2,buffer,strlen(buffer));
									}		
								}
							}
						}//end если master daed
					}//end если ошибка сокета
					else  if (!strncmp(buffer,"server", 6)){
						if((c_sock=socket(PF_INET,SOCK_STREAM,0))==-1) puts("error create socket");
						int i=0;
						while(buffer[i]!=0){
							buffer[i]=buffer[i+6];
							i++; 
						}
						printf("%s \n", buffer);
						addr.sin_addr.s_addr=inet_addr(buffer);
						if(connect(c_sock,(struct sockaddr *)&addr,sizeof(addr))==0) {
							printf("already connect to new slave: %s \n", buffer);
							FD_SET(c_sock,&sfds);
							FD_SET(c_sock,&afds);
						} else{printf("error connect to new slave \n %s \n", sys_errlist[errno]);}
					}
				break;
			}//end если в наборе сокетов
			if (mode==1){
				FILE *out=popen("ps aux|wc -l ","r");
				char buffer[81];
				memset(buffer,0,81);
				fgets(buffer,80,out);
				char symm[81];
				memset(symm,0,81);
				strcpy(symm, my_ip);
				strcat(symm, "\t");
				strcat(symm, buffer);
				write(m_sock,symm,strlen(symm));
				printf("%s", symm);
				pclose(out);
			}
		}//end for select
	}//if(mode)
} //for
exit (0);
}
