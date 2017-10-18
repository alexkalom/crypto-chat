#ifndef __CLISERVCOM__
#define __CLISERVCOM__

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


#include <crypto/cryptodev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define TCP_PORT 33543
#define TCP_BACKLOG 5

#define BLOCK_SIZE      16
#define KEY_SIZE	16  /* AES128 */

typedef struct {
	char * new_msg; 
	int new_length;
} imsg;

typedef struct _vector {
	char * msg; // the buffer holding the message
	int length; // the actual length of the message
	int size; // how many bytes can msg hold
} * vector;

//int extract_message_num;

int cfd;
struct session_op sess;
struct crypt_op cryp;
char iv[BLOCK_SIZE];
char key[KEY_SIZE];

void  extend(char ** dst, char * src, int len);
void setKey(char * str, int len);
int encrypt(char *msg, char ** enc_msg, int len);
int decrypt(char *enc_msg, char ** msg, int len);
int fill_urandom_buf(unsigned char *buf, size_t cnt);
ssize_t insist_read(int fd, void *buf, size_t cnt);
int initCryptodev();
vector newVector();
void forget(vector v, int len);
imsg * msgT(char * msg, int length);
imsg * msgE(vector v);
int isFinished(vector v);
void dblcpy(char * dst, char * src, int len);
void pushToVector(vector v, char * str, int len);
int attemptRead(int fd, vector v);
int attemptSend(int fd, char * msg, int len);
ssize_t insist_write(int fd, const void *buf, size_t cnt);


#endif