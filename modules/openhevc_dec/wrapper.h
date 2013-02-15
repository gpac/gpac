#ifdef __cplusplus
extern "C"
{
#endif
    int libDecoderInit( void );
	int libDecoderDecode(unsigned char *buff, int len, unsigned int *temporal_id);
	void libDecoderGetPictureSize(unsigned int *width, unsigned int *height);
	int libDecoderGetOuptut(unsigned int temporal_id, unsigned char *Y, unsigned char *U, unsigned char *V, int force_flush);
    void libDecoderClose( void );
	const char *libDecoderVersion();
#ifdef __cplusplus
}
#endif