/*
 * Copyright (C) 2002 Nikos Mavroyanopoulos
 * 
 * This library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Library General Public License as published 
 * by the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gpac/internal/crypt_dev.h>

typedef struct ctr_buf {
	u8* enc_counter;
	u8* c_counter;
	int c_counter_pos;
	int blocksize;
} CTR_BUFFER;

/* CTR MODE */

/* This function will add one to the given number (as a u8 string).
 * has been reached.
 */
static void increase_counter( u8 *x, int x_size) {
register int i, y=0;

	for (i=x_size-1;i>=0;i--) {
		y = 0;
		if ( x[i] == 0xff) {
			x[i] = 0;
			y = 1;
		} else x[i]++;

		if (y==0) break;
	}

	return;
}


/* size holds the size of the IV (counter in this mode).
 * This is the block size.
 */
GF_Err _init_mcrypt( void *buf, void *key, int lenofkey, void *IV, int size)
{
    ((CTR_BUFFER* )buf)->c_counter = ((CTR_BUFFER* )buf)->enc_counter = NULL;

/* For ctr */
    ((CTR_BUFFER* )buf)->c_counter_pos = 0;
    ((CTR_BUFFER* )buf)->blocksize = size;

	((CTR_BUFFER* )buf)->c_counter = (u8 *)malloc(size);
    if (((CTR_BUFFER* )buf)->c_counter==NULL) goto freeall;

	((CTR_BUFFER* )buf)->enc_counter = (u8 *)malloc(size);
    if (((CTR_BUFFER* )buf)->enc_counter==NULL) goto freeall;
    
    if (IV!=NULL) {
	memcpy(((CTR_BUFFER* )buf)->enc_counter, IV, size);
	memcpy(((CTR_BUFFER* )buf)->c_counter, IV, size);
    }

/* End ctr */

	return GF_OK;
	freeall:
		free(((CTR_BUFFER* )buf)->c_counter);
		free(((CTR_BUFFER* )buf)->enc_counter);
		return GF_OUT_OF_MEM;
}

GF_Err _mcrypt_set_state(void *_buf, void *IV, int size)
{
	CTR_BUFFER* buf = (CTR_BUFFER* )_buf;
	((CTR_BUFFER* )buf)->c_counter_pos = ((u8*)IV)[0];
	memcpy(((CTR_BUFFER* )buf)->c_counter, &((u8*)IV)[1], size-1);
	memcpy(((CTR_BUFFER* )buf)->enc_counter, &((u8*)IV)[1], size-1);

	return GF_OK;
}

GF_Err _mcrypt_get_state(void *buf, void *IV, int *size)
{
	if (*size < ((CTR_BUFFER* )buf)->blocksize + 1) {
		*size = ((CTR_BUFFER* )buf)->blocksize + 1;
		return GF_BAD_PARAM;
	}
	*size = ((CTR_BUFFER* )buf)->blocksize + 1;

	((u8 *)IV)[0] = ((CTR_BUFFER* )buf)->c_counter_pos;
	memcpy( & ((u8 *) IV)[1], ((CTR_BUFFER* )buf)->c_counter, ((CTR_BUFFER* )buf)->blocksize);

	return GF_OK;
}


void _end_mcrypt(void *buf) {
	free(((CTR_BUFFER* )buf)->c_counter);
	free(((CTR_BUFFER* )buf)->enc_counter);
}

static GFINLINE
void xor_stuff( CTR_BUFFER *buf, void* akey, void (*func)(void*,void*), u8* plain,  int blocksize, int xor_size) 
{
	void (*_mcrypt_block_encrypt) (void *, void *);

	_mcrypt_block_encrypt = func;

	if (xor_size == blocksize) {
		if (((CTR_BUFFER* )buf)->c_counter_pos == 0) {

			memcpy( ((CTR_BUFFER* )buf)->enc_counter, ((CTR_BUFFER* )buf)->c_counter, blocksize);
			_mcrypt_block_encrypt(akey, ((CTR_BUFFER* )buf)->enc_counter);

			memxor( plain, ((CTR_BUFFER* )buf)->enc_counter, blocksize);

			increase_counter( ((CTR_BUFFER* )buf)->c_counter, blocksize);


		} else {
			int size = blocksize - ((CTR_BUFFER* )buf)->c_counter_pos;

			memxor( plain, &((CTR_BUFFER* )buf)->enc_counter[((CTR_BUFFER* )buf)->c_counter_pos],
				size);
		
			increase_counter( ((CTR_BUFFER* )buf)->c_counter, blocksize);

			memcpy( ((CTR_BUFFER* )buf)->enc_counter, ((CTR_BUFFER* )buf)->c_counter, blocksize);
			_mcrypt_block_encrypt(akey, ((CTR_BUFFER* )buf)->enc_counter);

			memxor( &plain[size], ((CTR_BUFFER* )buf)->enc_counter,
				((CTR_BUFFER* )buf)->c_counter_pos);

			/* ((CTR_BUFFER* )buf)->c_counter_pos remains the same */

		}
	} else { /* xor_size != blocksize */
		if (((CTR_BUFFER* )buf)->c_counter_pos == 0) {
			memcpy( ((CTR_BUFFER* )buf)->enc_counter, ((CTR_BUFFER* )buf)->c_counter, blocksize);
			_mcrypt_block_encrypt(akey, ((CTR_BUFFER* )buf)->enc_counter);

			memxor( plain, ((CTR_BUFFER* )buf)->enc_counter, xor_size);
			((CTR_BUFFER* )buf)->c_counter_pos = xor_size;
		} else {
			int size = blocksize - ((CTR_BUFFER* )buf)->c_counter_pos;
			int min_size =  size < xor_size ? size: xor_size;

			memxor( plain, &((CTR_BUFFER* )buf)->enc_counter[((CTR_BUFFER* )buf)->c_counter_pos],
				min_size); 

			((CTR_BUFFER* )buf)->c_counter_pos += min_size;

			if (min_size >= xor_size)
				return;

			increase_counter( ((CTR_BUFFER* )buf)->c_counter, blocksize);

			memcpy( ((CTR_BUFFER* )buf)->enc_counter, ((CTR_BUFFER* )buf)->c_counter, blocksize);
			_mcrypt_block_encrypt(akey, ((CTR_BUFFER* )buf)->enc_counter);

			memxor( &plain[min_size], ((CTR_BUFFER* )buf)->enc_counter,
				xor_size - min_size);

			((CTR_BUFFER* )buf)->c_counter_pos = xor_size - min_size;

		}
	
	}
	return;
}

GF_Err _mcrypt(void * buf,void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{				/* plaintext can be any size */
	u8 *plain;
	int dlen, j=0;
	int modlen;

	dlen = len / blocksize;
	plain = (u8 *)plaintext;
	for (j = 0; j < dlen; j++) {

		xor_stuff((CTR_BUFFER*) buf, akey, func, plain, blocksize, blocksize);

		plain += blocksize;

/* Put the new register */

	}
	modlen = len % blocksize;
	if (modlen > 0) {
		/* This is only usefull if encrypting the
		 * final block. Otherwise you'll not be
		 * able to decrypt it.
		 */

		xor_stuff( (CTR_BUFFER*)buf, akey, func, plain, blocksize, modlen);

	}
	
	return GF_OK;
}


GF_Err _mdecrypt(void * buf,void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{				/* plaintext can be any size */
	return _mcrypt( buf, plaintext, len, blocksize, akey, func, func2);
}

void gf_crypt_register_ctr(GF_Crypt *td)
{
	td->mode_name = "CTR";
	td->_init_mcrypt = _init_mcrypt;
	td->_end_mcrypt = _end_mcrypt;
	td->_mcrypt = _mcrypt;
	td->_mdecrypt = _mdecrypt;
	td->_mcrypt_get_state = _mcrypt_get_state;
	td->_mcrypt_set_state = _mcrypt_set_state;

	td->has_IV = 1;
	td->is_block_mode = 1;
	td->is_block_algo_mode = 1;
	td->mode_size = sizeof(CTR_BUFFER);
	td->mode_version = 20020307;
}
