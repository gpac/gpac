#ifndef OPEN_HEVC_WRAPPER_H
#define OPEN_HEVC_WRAPPER_H

#define NV_VERSION  "1.0" ///< Current software version

int  libOpenHevcInit();
int  libOpenHevcDecode(unsigned char *buff, int nal_len);
void libOpenHevcGetPictureSize(unsigned int *width, unsigned int *height, unsigned int *stride);
int  libOpenHevcGetOuptut(int got_picture, unsigned char **Y, unsigned char **U, unsigned char **V);
int  libOpenHevcGetOutputCpy(int got_picture, unsigned char *Y, unsigned char *U, unsigned char *V);
void libOpenHevcSetCheckMD5(int val);
void libOpenHevcClose();
const char *libOpenHevcVersion();

#endif // OPEN_HEVC_WRAPPER_H
