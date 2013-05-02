#ifndef OPEN_HEVC_WRAPPER_H
#define OPEN_HEVC_WRAPPER_H

#define NV_VERSION  "1.2" ///< Current software version

int  libOpenHevcInit(int nb_pthreads);
int  libOpenHevcDecode(unsigned char *buff, int nal_len);
void libOpenHevcGetPictureSize(unsigned int *width, unsigned int *height, unsigned int *stride);
void libOpenHevcGetPictureSize2(unsigned int *width, unsigned int *height, unsigned int *stride);
int  libOpenHevcGetOutput(int got_picture, unsigned char **Y, unsigned char **U, unsigned char **V);
int  libOpenHevcGetOutputCpy(int got_picture, unsigned char *Y, unsigned char *U, unsigned char *V);
void libOpenHevcSetCheckMD5(int val);
void libOpenHevcClose();
void libOpenHevcFlush();
const char *libOpenHevcVersion();

#endif // OPEN_HEVC_WRAPPER_H
