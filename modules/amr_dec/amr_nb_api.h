#ifndef _AMR_NB_API_H
#define _AMR_NB_API_H

enum Mode { MR475 = 0,
            MR515,
            MR59,
            MR67,
            MR74,
            MR795,
            MR102,
            MR122,

            MRDTX,

            N_MODES     /* number of (SPC) modes */

          };

enum RXFrameType { RX_SPEECH_GOOD = 0,
                   RX_SPEECH_DEGRADED,
                   RX_ONSET,
                   RX_SPEECH_BAD,
                   RX_SID_FIRST,
                   RX_SID_UPDATE,
                   RX_SID_BAD,
                   RX_NO_DATA,
                   RX_N_FRAMETYPES     /* number of frame types */
                 };

typedef struct {
	void* decoder_amrState;
	void*  post_state;
	void* postHP_state;
	enum Mode prev_mode;

	int complexityCounter;   /* Only for complexity computation            */
} __Speech_Decode_FrameState;

int Speech_Decode_Frame_init (__Speech_Decode_FrameState **st, char *id);
void Speech_Decode_Frame_exit (__Speech_Decode_FrameState **st);
void Speech_Decode_Frame (__Speech_Decode_FrameState *st, enum Mode mode, short *serial, enum RXFrameType frame_type, short *synth);
int Speech_Decode_Frame_reset (__Speech_Decode_FrameState **st);

s16 decoder_homing_frame_test_first (s16 input_frame[], enum Mode mode);
s16 decoder_homing_frame_test (s16 input_frame[], enum Mode mode);

#ifdef MMS_IO

enum RXFrameType UnpackBits (
    s8  q,              /* i : Q-bit (i.e. BFI)        */
    s16 ft,             /* i : frame type (i.e. mode)  */
    u8 packed_bits[],  /* i : sorted & packed bits    */
    enum Mode *mode,       /* o : mode information        */
    s16 bits[]          /* o : serial bits             */
);
#endif

#define EHF_MASK 0x0008        /* encoder homing frame pattern             */
#define L_FRAME      160       /* Frame size                               */

#endif //_AMR_NB_API_H