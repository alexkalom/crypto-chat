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


int main(const int argc, const char * argv[]) {
	int newsd, port;
	ssize_t n;
	char buf[100];
	const char *hostname;
	struct hostent *hp;
	struct sockaddr_in sa;

	vector last_message = newVector(); // buffer keeping-building the last/current message.

	char str[15];
	sprintf(str,"alex");
	setKey(str,4);
	initCryptodev();

	if (argc != 3) {
		fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
		exit(1);
	}
	hostname = argv[1];
	port = atoi(argv[2]); /* Needs better error checking */

	/* Create TCP/IP socket, used as main chat channel */
	if ((newsd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");
	
	/* Look up remote hostname on DNS */
	if ( !(hp = gethostbyname(hostname))) {
		printf("DNS lookup failed for host %s\n", hostname);
		exit(1);
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
	fprintf(stderr, "Connecting to remote host... "); fflush(stderr);
	if (connect(newsd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		exit(1);
	}
	fprintf(stderr, "Connected.\n");

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
		}

		if (FD_ISSET(newsd,&rfds)) {
			//read all bytes that were send. and if whole message received print to screen.
			if (attemptRead(newsd, last_message)<0) {
				close(newsd); // close the connection
				break;
			}
		}
		
	}
	return 0;
}