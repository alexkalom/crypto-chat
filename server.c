#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netdb.h>

#include "client-server-common.h"

int main(void) {
	char addrstr[INET_ADDRSTRLEN];
	int sd, newsd;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in sa;

	vector last_message = newVector(); // buffer keeping-building the last/current message.
	/* Make sure a broken connection doesn't kill us */

	char str[15];
	sprintf(str,"alex");
	setKey(str,4);
	initCryptodev();



	signal(SIGPIPE, SIG_IGN);

	sd = socket(PF_INET,SOCK_STREAM,0);
	if (sd < 0){
		printf("error opening socket\n");
		exit(1);
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		exit(1);
	}
	fprintf(stderr, "Bound TCP socket to port %d\n", TCP_PORT);

	/* Listen for incoming connections */
	if (listen(sd, TCP_BACKLOG) < 0) {
		perror("listen");
		exit(1);
	}

	for (;;) {
		fprintf(stderr, "Waiting for an incoming connection...\n");

		/* Accept an incoming connection */
		len = sizeof(struct sockaddr_in);
		if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
			perror("accept");
			exit(1);
		}
		if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
			perror("could not format IP address");
			exit(1);
		}
		fprintf(stderr, "Incoming connection from %s:%d\n",
			addrstr, ntohs(sa.sin_port));


		//we want to be able to send messages and receiving messages at the same
		// time. Suggestion add stdin and incoming messages select method.

		fd_set rfds,wfds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		int retval;
		while (1) {
			FD_SET(newsd, &rfds);
			FD_SET(0, &rfds);
			while ((retval = select(FD_SETSIZE, &rfds, NULL,NULL,NULL)) == 0);
			if (retval < 0) {
				perror("There was an error with select\n");
				exit(1);
			}

			//check which fd contains data for us
			if (FD_ISSET(0, &rfds)) {
				vector v = newVector();
				if (attemptRead(0,v) < 0) {// read message 
					if (shutdown(newsd, SHUT_WR) < 0) {
						perror("shutdown");
						exit(1);
					}
					continue; //nothing to send;
				}
				//now attempt to Send data
				if (attemptSend(newsd, v->msg, v->length) == 0) {
					// Send was aborted.
					// Infor user about that. Lose only one character and 
					// make life easier
					v->msg[v->length-1]='\0';
					printf("Message: %s was not send\n", v->msg);
				}
				free(v->msg);
				free(v);
			}

			if (FD_ISSET(newsd,&rfds)) {
				//read all bytes that were send. and if whole message received print to screen.
				if (attemptRead(newsd, last_message)<0) {
					close(newsd); // close the connection
					break;
				}
			}
			
		}

	}
	return 0;
}