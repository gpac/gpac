#ifdef HAS_AVC_SUPPORT

#ifndef RAW1394UTIL_H
#define RAW1394UTIL_H 1

#include <libraw1394/raw1394.h>

#ifdef __cplusplus
extern "C"
{
#endif

	int raw1394_get_num_ports( void );
	raw1394handle_t raw1394_open( int port );
	void raw1394_close( raw1394handle_t handle );
	int discoverAVC( int * port, octlet_t* guid );
	void reset_bus( int port );

#ifdef __cplusplus
}

#endif
#endif

#endif // HAS_AVC_SUPPORT
