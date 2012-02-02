#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PUERTO "3737"
#define BACKLOG 9
//7 Clientes + 1 Servidor + 1 Entrada de teclado
#define REFRESCO 5
//Pongo refresco de 5 segundos pero seria 15*60


#define STDIN 0

int crear_servidor()
{
	struct addrinfo hints,*servinfo,*p;
	int sockfd,yes=1;

	//Modificamos hints
	memset(&hints,0,sizeof hints);						//La borramos
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;					//TCP
	hints.ai_flags = AI_PASSIVE;						//Usa mi ip

	if (getaddrinfo(NULL, PUERTO, &hints, &servinfo) != 0) return 0;	//Recogemos informacion nuestra en servinfo
	
	//Ahora intentamos crear un socket en nuestra máquina
	for (p=servinfo;p!=NULL;p=p->ai_next)
	{
		if ((sockfd=socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1) continue;

		//Ajustamos opciones de socket
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) return 0;
		//Lo bindeamos
		if (bind(sockfd,p->ai_addr,p->ai_addrlen) == -1)
		{
			close(sockfd);
			continue;
		}
		break;
	}

	if (p==NULL) return 0;

	freeaddrinfo(servinfo);

	//Ponemos a la escucha
	if (listen(sockfd, BACKLOG) == -1) return 0;
	return sockfd;
}

void sigchld_handler(int s)
{
	while(waitpid(-1,NULL,WNOHANG)>0);
}
int procesos_muertos(void)
{
	struct sigaction sa;
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD,&sa,NULL) == -1) return 0;
	return 1;
}

int main(void)
{
	int i=0,conectados=0,nbytes;
	char buffer[256];
	int ciclos=0;	//Ciclos del timeval, cuando llegue a 15*60 seran 15 minutos approx

	time_t hora_actual,hora_inicio;

	int server_fd,temp_fd;

	struct timeval tv;
	fd_set masterset,readset;
	int fd_max=0;

	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;

	tv.tv_sec=1;
	tv.tv_usec=0;

	//Inicializamos readfds
	FD_ZERO(&masterset);
	//Añadimos teclado
	FD_SET(STDIN,&masterset);
	fd_max=STDIN;	//En este momento como es el único, es el máximo

	printf("#Servidor para 7 sensores\n");

	printf("[!] Creando servidor en puerto %s... ",PUERTO);
	if (server_fd=crear_servidor()) printf("OK\n");
	else { printf("Error\n"); exit(1); }
	printf("[!] Procesos muertos... ");
	if (procesos_muertos) printf("OK\n");
	else { printf("Error\n"); exit(1); }

	//Añadimos servidor
 	FD_SET(server_fd,&masterset);
	if (server_fd>fd_max) fd_max=server_fd;

	hora_inicio = time(NULL);
	//Bucle principal
	while (1)
	{
		//Copiamos de master a read
		readset = masterset;
		//Temporizador de retardo a 1 segundo
		tv.tv_sec=1;
		tv.tv_usec=0;
		//Hacemos select (retardo maximo 1 segundo)
		if (select(fd_max+1, &readset, NULL, NULL, &tv) == -1)
		{
			printf("[!] Error en select\n");
			return 1;
		}

		hora_actual = time(NULL);
		//Veamos si alguna llamó la atención
		for (i=0;i<=fd_max;i++)
		{
			if (FD_ISSET(i,&readset))
			{
				//Ahora vemos cual es
				if (i == server_fd)
				{
					//Nos piden que les aceptemos
					//Comprobemos que no estamos llenos
					if (conectados<7)
					{
						addrlen = sizeof remoteaddr;
						temp_fd = accept(
								server_fd,
								(struct sockaddr *)&remoteaddr,
								&addrlen);
						if (temp_fd == -1)
							printf("[!] Error al aceptar usuario\n");
						else
						{
							FD_SET(temp_fd,&masterset);
							if (temp_fd>fd_max) fd_max=temp_fd;
							printf("[!] Conectado cliente\n");
						}
					}
				}
				else
				{
					if (i == STDIN)
					{
						//Teclado
						fgets(buffer,255,stdin);
						if (buffer[strlen(buffer)-1]=='\n') buffer[strlen(buffer)-1]=0;
						if (strcmp(buffer,"exit") == 0)
						{
							printf("[!] Cerrando sockets\n");
							for (i=0;i<=fd_max;i++) close(i);
							printf("[!] Saliendo\n");
							return 0;
						}
					}
					else
					{
						//Un cliente ha hablado
						nbytes=recv(i,buffer,256,0);
						if (nbytes <= 0)
						{
							printf("[!] Cliente desconectado\n");
							conectados--;
							FD_CLR(i,&masterset);
						}
						else
						{
							buffer[nbytes]=0;
							printf("%s",buffer);
						}
					}
				}
			}
		}
		//Para mostrar en pa
		if (difftime(hora_actual,hora_inicio)>=REFRESCO)
		{
			hora_inicio=hora_actual;
			printf("[i] Pasaron %d segundos\n",REFRESCO);
		}
	}
	return 0;
}
