//----------------------------
// Symbian codec API
//
//----------------------------
#include "rules.h"
#include "global.h"
#include "xvid.h"

#ifdef __SYMBIAN32__
#include <e32std.h>
#include <e32base.h>
//GLDEF_C TInt E32Dll(TDllReason){ return KErrNone; }
#else
#endif


//----------------------------

struct S_decoder {
	xvid_dec_frame_t xvid_dec_frame;
	dword size_x;
	void *dec_handle;
};

#define FCC(a, b, c, d) dword((d<<24) | (c<<16) | (b<<8) | a)

//----------------------------

void * InitCodec(dword sx, dword sy, dword fcc) {

	switch(fcc) {
	case FCC('x', 'v', 'i', 'd'):
	case FCC('X', 'V', 'I', 'D'):
	case FCC('D', 'I', 'V', 'X'):
	case FCC('d', 'i', 'v', 'x'):
	case FCC('D', 'X', '5', '0'):
	case FCC('3', 'I', 'V', '2'):
	case FCC('3', 'i', 'v', '2'):
	case FCC('3', 'I', 'V', 'X'):
		break;
	default:
		return NULL;
	}
	S_decoder *dec = new S_decoder;
	if (!dec) return NULL;
	dec->size_x = sx;

	xvid_dec_frame_t &xvid_dec_frame = dec->xvid_dec_frame;
	MemSet(&xvid_dec_frame, 0, sizeof(xvid_dec_frame));
	xvid_dec_frame.version = XVID_VERSION;
	//xvid_dec_frame.general = 0;
	//convert into true-color, we'll perform convertion to dest format ourselves
	//xvid_dec_frame.output.csp = XVID_CSP_BGR;

	xvid_gbl_init_t xvid_gbl_init;
	MemSet(&xvid_gbl_init, 0, sizeof(xvid_gbl_init));
	xvid_gbl_init.version = XVID_VERSION;
	xvid_gbl_init.cpu_flags = 0;
	//xvid_gbl_init.debug = XVID_DEBUG_ERROR | XVID_DEBUG_STARTCODE | XVID_DEBUG_HEADER;
	xvid_global(NULL, 0, &xvid_gbl_init, NULL);

	xvid_dec_create_t xvid_dec_create;
	MemSet(&xvid_dec_create, 0, sizeof(xvid_dec_create));
	xvid_dec_create.version = XVID_VERSION;
	xvid_dec_create.width = 0;
	xvid_dec_create.height = 0;
#ifdef PROFILE
	xvid_dec_create.prof = &prof;
#endif
	xvid_dec_create.width = sx;
	xvid_dec_create.height = sy;

	int ret = xvid_decore(NULL, XVID_DEC_CREATE, &xvid_dec_create, NULL);
	if(ret) {
		delete dec;
		return NULL;
	}
	dec->size_x = sx;
	dec->dec_handle = xvid_dec_create.handle;

	return dec;
}

//----------------------------

void CloseCodec(void *handle) {
	S_decoder *dec = (S_decoder*)handle;
	xvid_decore(dec->dec_handle, XVID_DEC_DESTROY, NULL, NULL);
	delete dec;
}

//----------------------------

int DecodeFrame(void *handle, const void *buf, dword sz_in, byte *&y, byte *&u, byte *&v, dword &pitch) {

	S_decoder *dec = (S_decoder*)handle;

	dec->xvid_dec_frame.bitstream = buf;
	dec->xvid_dec_frame.length = sz_in;
	xvid_decore(dec->dec_handle, XVID_DEC_DECODE, &dec->xvid_dec_frame, NULL);

	const C_xvid_image *img = dec->xvid_dec_frame.img_out;
	if(!img)
		return 0;

	y = img->y;
	u = img->u;
	v = img->v;
	int mb_width = (dec->size_x + 15) / 16;
	pitch = 16 * mb_width + 2 * EDGE_SIZE;
	return 1;
}

//----------------------------
