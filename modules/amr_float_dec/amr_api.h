#ifndef _AMR_API_H
#define _AMR_API_H

/*AMR*/
void Decoder_Interface_Decode( void *st,
#ifndef ETSI
                               unsigned char *bits,
#else
                               short *bits,
#endif
                               short *synth, int bfi );

void *Decoder_Interface_init( void );


void Decoder_Interface_exit( void *state );



/*AMR WB*/


#define NB_SERIAL_MAX   61    /* max serial size      */
#define L_FRAME16k      320   /* Frame size at 16kHz  */

#define _good_frame  0
#define _bad_frame   1
#define _lost_frame  2
#define _no_frame    3

void D_IF_decode(void *st, unsigned char *bits, short *synth, long bfi);
void * D_IF_init(void);
void D_IF_exit(void *state);

#endif //_AMR_API_H