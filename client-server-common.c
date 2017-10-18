#include "client-server-common.h"


/* Insist until all of the data has been read */
ssize_t insist_read(int fd, void *buf, size_t cnt)
{
        ssize_t ret;
        size_t orig_cnt = cnt;

        while (cnt > 0) {
                ret = read(fd, buf, cnt);
                if (ret < 0)
                        return ret;
                buf += ret;
                cnt -= ret;
        }

        return orig_cnt;
}


int fill_urandom_buf(unsigned char *buf, size_t cnt)
{
        int crypto_fd;
        int ret = -1;

        crypto_fd = open("/dev/urandom", O_RDONLY);
        if (crypto_fd < 0)
                return crypto_fd;

        ret = insist_read(crypto_fd, buf, cnt);
        close(crypto_fd);

        return ret;
}


int initCryptodev() {
	
	cfd = open("/dev/crypto", O_RDWR);
	if (cfd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}

	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));

	/*
	 * Get crypto session for AES128
	 */
	sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = key;


	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	/*if (fill_urandom_buf(iv, BLOCK_SIZE) < 0) {
		perror("getting data from /dev/urandom\n");
		return 1;
	}*/
	int i;
	for (i =0 ;i< BLOCK_SIZE; i++) 
		iv[i] = 0x00;

	cryp.ses = sess.ses;
	cryp.iv = iv;

	return 0;
}

int encrypt(char *msg, char ** enc_msg, int len) {
	*enc_msg = (char *) malloc(len * sizeof(char));
	if (*enc_msg == NULL) {
		perror("Error while allocating space\n");
		exit(1);
	}

	cryp.len = len;
	cryp.src = msg;
	cryp.dst = *enc_msg;
	cryp.op = COP_ENCRYPT;

	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}
	return 0;
}

int decrypt(char *enc_msg, char ** msg, int len) {
	*msg = (char *) malloc(len * sizeof(char));
	if (*msg == NULL) {
		perror("Error while allocating space\n");
		exit(1);
	}

	cryp.len = len;
	cryp.src = enc_msg;
	cryp.dst = *msg;
	cryp.op = COP_DECRYPT;
	
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	return 0;
}

void setKey(char * str, int len) {
	int i;
	for (i = 0; i<len ;i++)
		key[i] = str[i];
	for (; i<KEY_SIZE; i++)
		key[i]= 0x00;
}


vector newVector() {
	vector n = (vector) malloc(sizeof(*n));
	n->msg = (char *) malloc(256 * sizeof(char));
	if (n->msg == NULL) {
		perror("Error while allocating space\n");
		exit(1);
	}
	n->length = 0;
	n->size = 256;
	return n;
}

imsg * msgT(char * msg, int length) {
	imsg * res = (imsg *) malloc(sizeof(*res));
	if (res == NULL) {
		perror("Error while allocating memory\n");
		exit(1);
	}

	//encrypt
	char *enc_msg;
	char * ext_msg;
	extend(&ext_msg, msg, length);
	length = BLOCK_SIZE *( length / BLOCK_SIZE) + BLOCK_SIZE;
	encrypt(ext_msg, &enc_msg, length);
	msg = enc_msg;
	free(ext_msg);




	//scan the message one time for special charracters and count them
	int count  =0;
	int i;
	for (i =0 ; i<length; i++)
		if (msg[i] == 0x03 || msg[i] == 0x02) 
			count++;
	res->new_length = length + (2 + count);
	//create a new buffer for the message 
	char * new_msg = (char *) malloc((2 + count + length) * sizeof(char));
	if (new_msg == NULL) {
		perror("there was an error with allocating temporary space\n");
		exit(1);
	}

	new_msg[0] = 0x01;
	for (count =1, i =0; i<length; i++) {
		if (msg[i]== 0x03 || msg[i] == 0x02)
			new_msg[count++] = msg[i];
		if (msg[i] == 0x00) 
			new_msg[count++]=0x02;
		else if (msg[i] == 0x01) 
			new_msg[count++] = 0x03;
		else
			new_msg[count++] = msg[i];
	}
	new_msg[count]= 0x00;
	res->new_msg = new_msg;
	return res;

}

void  extend(char ** dst, char * src, int len) {
	int new_length = BLOCK_SIZE * (len /BLOCK_SIZE) + BLOCK_SIZE;
	*dst = (char *) malloc (new_length * sizeof(char));
	if (*dst == NULL){
		perror("allocating memory\n");
		exit(1);
	}
	int i;
	for (i =0 ; i<len; i++) {
		(*dst)[i] = src[i];
	}
	for (; i<new_length; i++) {
		(*dst)[i] = 0x00;
	}
}

void forget(vector v, int len){
	int i;
	for (i = 0; i< v->length - len; i++) {
		v->msg[i] = v->msg[i + len];
	}
	v->length -= len;
}

int isFinished(vector v) {
	char *msg= v->msg;
	int len = v->length; 
	int i;
	for (i = 1; i< len; i++) { 
		if (msg[i] == 0x01) {
			//we have a new begin obviously we need to discard the message
			forget(v, i);
			return -1; // signal the wrong message
		}
		if (msg[i] == 0x00) {
			//we found the end of the message
			return i;
		}
	}
	return 0; //we don't know yet.
}

imsg * msgE(vector v){
	int end;
	while ((end = isFinished(v))< 0); // ignore all the messages that didn't came through
	if (end == 0)
		return NULL; // we can't do anything
	
	//translate and return 
	imsg * res = (imsg *) malloc(sizeof(*res));
	if (res == NULL) {
		perror("Error while allocating memory\n");
		exit(1);
	}

	//scan the message one time for special charracters and count them
	int count  =0;
	int i;
	for (i =1 ; i<end; i++)
		if (v->msg[i] == 0x02 || v->msg[i] == 0x03) 
			if (v->msg[i] == v->msg[i+1]) {
				count++;
				i++;
			}
	res->new_length = end - 1 - count/2;
	//create a new buffer for the message 
	char * new_msg = (char *) malloc((end - 1 - count/2) * sizeof(char));
	if (new_msg == NULL) {
		perror("there was an error with allocating temporary space\n");
		exit(1);
	}

	for (count =0, i =1; i<end; i++) {
		if (v->msg[i]== 0x02 || v->msg[i] == 0x03){
			if (v->msg[i] == v->msg[i+1]) {
				new_msg[count++] = v->msg[i];
				i++;
			}
			else if (v->msg[i] == 0x02) {
				new_msg[count++] = 0x00;
			}
			else {
				new_msg[count++] = 0x01;
			}
		}
		else {
			new_msg[count++]= v->msg[i];
		}
	}
	res->new_msg = new_msg;
	forget(v, end); // kanonika end + 1 alla gia na exeis sigoura orio end. Tha petaxei meta ena "xazo" paket i isFinished.


	//decrypt
	char * dec_msg;
	decrypt(res->new_msg, &dec_msg, res->new_length);
	char * temp = res->new_msg;
	res->new_msg = dec_msg;
	free(temp);


	return res;

}


void dblcpy(char * dst, char * src, int len) {
	dst = (char *) malloc(2*len*sizeof(char));
	if (dst == NULL) {
		perror("error while allocating space\n");
		exit(1);
	}
	int i;
	for (i =0; i<len; i++) {
		dst[i] = src[i];
	}
	return;
}


void pushToVector(vector v, char * str, int len) {
	if (v->length + len  > v->size) {
		// we need to resize vector;
		char * temp = v->msg;
		dblcpy(v->msg,temp, v->size);
		v->size = v->size << 2;
		free(temp);
	}

	//do the actual push
	int i;
	for (i =0 ; i<len; i++) 
		v->msg[v->length++] = str[i];
}

int attemptRead(int fd, vector v) {

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	fd_set rfds;

	FD_ZERO(&rfds);

	char msg[128];
	int size;

	while (1) {
		// check if reading is possible without blocking
		FD_SET(fd, &rfds);
		select(FD_SETSIZE, &rfds, NULL,NULL,&tv);

		if (FD_ISSET(fd,&rfds)) {
			size = read(fd, msg, 128);
			if (size < 0) {
				perror("Error while reading from stdin\n");
				exit(1);
			}

			if (size == 0) 
				return -1; // close or shutdown;

			
			pushToVector(v, msg,size);

			/*if (fd == 0) 
				pushToVector(v, msg,size);
			else {
				char * dec_msg;
				decrypt(msg, &dec_msg, size);
				pushToVector(v, dec_msg,size);
				free(dec_msg);
			}*/

			imsg * res;
			if (fd == 0  && msg[size - 1] == '\n') 
				return 1; //we read successfully one message 
			else if (fd != 0 && (res = msgE(v)) != NULL) {
				insist_write(1,"Other: ", sizeof("Other: "));
				insist_write(1, res->new_msg, res->new_length);
				free(res->new_msg);
				free(res);
			}
		}
		else 
			return 0; // den teleiosame akoma me to trexon minima.
	}
}

int attemptSend(int fd, char * msg, int len) {
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	fd_set wfds;
	FD_ZERO(&wfds);

	int i =0;
	int size;
	imsg * temp_mess =  msgT(msg, len);

	/*char *enc_msg;
	char * ext_msg;
	extend(&ext_msg, temp_mess->new_msg, temp_mess->new_length);
	temp_mess->new_length= BLOCK_SIZE * (temp_mess->new_length / BLOCK_SIZE) + BLOCK_SIZE;
	encrypt(ext_msg, &enc_msg,temp_mess->new_length);
	insist_write(1, enc_msg, temp_mess->new_length);
	char * temp = temp_mess->new_msg;
	temp_mess->new_msg = enc_msg;
	free(temp);
	free(ext_msg);*/

	while (1) {
		//check that we can write without blocking
		FD_SET(fd, &wfds);
		select(FD_SETSIZE, NULL, &wfds, NULL, &tv);

		//if yes 
		if (FD_ISSET(fd,&wfds)) {
			size = write(fd, temp_mess->new_msg + i, temp_mess->new_length);
			if (temp_mess->new_length != size) {
				// we were not able to send the whole message
				//check for errors
				if (size < 0) {
					perror("There was an error while writing message: attemptSend\n");
					exit(1);
				}

				// update some values and continue if possible
				i += size;
				temp_mess->new_length -= size;

				//now we can try to send the rest of the message.
			}
			else {
				// take care of the temp space
				free(temp_mess->new_msg);
				free(temp_mess);

				// the Send was successful
				return 1;
			}
		}
		else {
			// take care of the temp space
			free(temp_mess->new_msg);
			free(temp_mess);

			// if we try to write we will block. Abort Send
			return 0;
		}
	}
}


/* Insist until all of the data has been written */
ssize_t insist_write(int fd, const void *buf, size_t cnt)
{
	ssize_t ret;
	size_t orig_cnt = cnt;
	
	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}
