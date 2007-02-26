#include <gpac/base_coding.h>
#include <gpac/constants.h>
#include <gpac/scene_manager.h>
#include <gpac/bifs.h>
#include <gpac/bifsengine.h>
#include <gpac/network.h>
#include <gpac/internal/ietf_dev.h>
#include "RTP_serv_generator.h"

/* definitions des structures */
struct __tag_bifs_engine
{
	GF_SceneGraph *sg;
	GF_SceneManager	*ctx;
	GF_SceneLoader load;
	void *calling_object;
	GF_StreamContext *sc;
	
	GF_BifsEncoder *bifsenc;
	u32 stream_ts_res;
	/* TODO: maybe the currentAUCount should be a GF_List of u32 
	to capture the number of AU per input BIFS stream */
	u32 currentAUCount;

	char encoded_bifs_config[20];
	u32 encoded_bifs_config_size;
};

int sdp_generator(PNC_CallbackData * data, char *ip_dest, char *sdp_fmt);
