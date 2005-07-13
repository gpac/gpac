This is a port of the XviD decoder for symbian and PocketPC OS. It was found by luck on the net at http://www.lonelycatgames.com/mobile/smartmovie/

Symbian codec system:

//----------------------------
General rules:
- All codecs are located in c:\system\codecs on target device
- Each codec is single native DLL
- One codec may support one or more video formats
- Formats are identified by 32-bit fourcc code

//----------------------------
Codec API:

Codec DLL need to have following functions exported:

Function    Ordinal
InitCodec      1
CloseCodec     2
DecodeFrame    3

Because Symbian DLL doesn't support exporting DLL functions by name, exported functions
must have ordinals as specified in above table.

//----------------------------
Detailed API definition:

void *InitCodec(dword sx, dword sy, dword fcc);
Initialize codec, using provided fcc code and dimensions of video image.
Parameters:
   [IN] sx, sy - resolution of video image (width, height)
   [IN] fcc - Four-cc code of video stream
Return value:
   pointer to decoder handle (passed to other functions), or NULL if fourcc code is not supported by codec

//----------------------------
void CloseCodec(void *handle);
Close codec, unitinialize memory, etc.
Parameters:
   [IN] handle - handle value obtained by InitCodec function

//----------------------------
int DecodeFrame(void *handle, const void *buf, dword sz_in, const byte *&y, const byte *&u, const byte *&v, dword &pitch);
Decode single frame. The frame data may depend on previous frame data passed in by previous call to this function.
It is safe to call this function with data of non-contiguous frame, for example when seeking, provided that
the frame is a keyframe - i.e. contains data for entire image.
Parameters:
   [IN] handle - handle value obtained by InitCodec function
   [IN] buf - pointer to bitstream buffer
   [IN] sz_in - size of 'buf' buffer data
   [OUT] y, u, v - pointer references filled with YUV to YUV data of decoded frame when the call returns; these
      pointers are valid until next call to DecodeFrame or CloseCodec functions
   [OUT] pitch - size of image line, in bytes (for Y array; U and V arrays have half pitch of Y, because UV is
      coded only for each 4x4 pixel block
Return value:
   0 - frame was not decoded (i.e. preroll frame); pointers y, u, v are not filled
   1 - frame was decoded, contains packed YUV data
   2 - frame was decoded, contains interleaved RGB data, returned in 'y' pointer

//----------------------------

Note:
The source project is built in Microsoft Visual C++ 6 environment.
It is assumed that Symbian Series60 SDK is installed in C:\Symbian\6.1\Series60\Epoc32

