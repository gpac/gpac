/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Stream Management sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#ifndef _GF_TERMINAL_DEV_H_
#define _GF_TERMINAL_DEV_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <gpac/terminal.h>
#include <gpac/mpeg4_odf.h>

#include <gpac/modules/service.h>
#include <gpac/modules/codec.h>
#include <gpac/modules/ipmp.h>
#include <gpac/mediaobject.h>
#include <gpac/thread.h>

typedef struct _inline_scene GF_InlineScene;
typedef struct _media_manager GF_MediaManager;
typedef struct _object_clock GF_Clock;
typedef struct _es_channel GF_Channel;
typedef struct _generic_codec GF_Codec;
typedef struct _composition_memory GF_CompositionMemory;


struct _net_service
{
	/*the module handling this service - must be declared first to typecast with GF_DownlaodSession upon deletion*/
	GF_InputService *ifce;

	/*the terminal*/
	struct _tag_terminal *term;
	/*service url*/
	char *url;
	/*od_manager owning service, NULL for services created for remote channels*/
	struct _od_manager *owner;
	/*number of attached remote channels ODM (ESD URLs)*/
	u32 nb_ch_users;
	/*number of attached remote ODM (OD URLs)*/
	u32 nb_odm_users;
	
	/*clock objects. Kept at service level since ESID namespace is the service one*/
	GF_List *Clocks;
	/*all downloaders objects used in this service*/
	GF_List *dnloads;
	/*cache asscoiated with service, if any*/
	GF_StreamingCache *cache;
};


/*opens service - performs URL concatenation if parent service specified*/
GF_ClientService *gf_term_service_new(GF_Terminal *term, GF_ObjectManager *owner, const char *url, GF_ClientService *parent_service, GF_Err *ret_code);
/*destroy service*/
void gf_term_service_del(GF_ClientService *nets);

/*access to the module interfaces - cf net_api.h GF_InputService for details*/
GF_Err gf_term_service_command(GF_ClientService *ns, GF_NetworkCommand *com);
Bool gf_term_service_can_handle_url(GF_ClientService *ns, char *url);

GF_Err gf_term_channel_get_sl_packet(GF_ClientService *ns, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *is_compressed, GF_Err *out_reception_status, Bool *is_new_data);
GF_Err gf_term_channel_release_sl_packet(GF_ClientService *ns, LPNETCHANNEL channel);

/*cache open/close*/
GF_Err gf_term_service_cache_load(GF_ClientService *ns);
GF_Err gf_term_service_cache_close(GF_ClientService *ns, Bool no_save);

/*forwards all clocks of the given amount of time. Can only be used when terminal is in paused mode
this is mainly designed for movie dumping in MP4Client*/
GF_Err gf_term_step_clocks(GF_Terminal * term, u32 ms_diff);

void gf_term_sample_clocks(GF_Terminal *term);

/*
		Inline scene stuff
*/
struct _inline_scene
{
	/*root OD of the subscene, ALWAYS namespace of the parent scene*/
	struct _od_manager *root_od;
	/*scene codec: top level decoder decoding/generating the scene - can be BIFS, VRML parser, etc*/
	struct _generic_codec *scene_codec;
	/*OD codec - specific to MPEG-4, only present at the inline level (media ressources are always scoped here)*/
	struct _generic_codec *od_codec;

	/*struct _od_managers used, namespace of this scene. The chain does not have the root_od
	it only contains OD sent through OD UPDATE in the OD stream(s) attached 
	to this scene. Remote ODs are not added, only there parents are*/
	GF_List *ODlist;
	/*list of MOs (links between OD and nodes)*/
	GF_List *media_objects;
	/*list of externproto libraries*/
	GF_List *extern_protos;
	/*list of nodes using this inline*/
	GF_List *inline_nodes;
	/*list of extra scene graphs (text streams, generic OSDs, ...)*/
	GF_List *extra_scenes;
	/*inline scene graph*/
	GF_SceneGraph *graph;
	/*graph state - if not attached, no traversing of inline
	0: not attached
	1: attached
	2: temp graph attached. The temp graph is generated when waiting for the first scene AU to be processed
	*/
	u32 graph_attached;
	/*togles inline restart - needed because the restart may be triggered from inside the scene or from
	parent scene, hence 2 render passes must be used
	special value 2 means scene URL changes (for anchor navigation*/
	u32 needs_restart;
	/*duration of inline scene*/
	u64 duration;
	/*if not 0, all objects in the scene will run on this clock. Needed in GPAC when clock references do not
	respect object graph (eg IOD depending on external stream for clock)*/
	u16 force_sub_clock_id;
	/*world info node or title node*/
	void *world_info;

	Bool is_dynamic_scene;
	/*clock for dynamic scene - current assumption is that all selected streams are synchronized in the dyn scene*/
	GF_Clock *dyn_ck;
	/*URLs of current video, audio and subs (we can't store objects since they may be destroyed when seeking)*/
	SFURL visual_url, audio_url, text_url;
	/*set to 1 when single time-line presentation with ONE OD AU is detected - the goal is to prevent
	OD shutdown/startup when seeking. This will also remove unneeded net traffic for AddChannel/RemoveChannel
	like RTSP TEARDOWN/SETUP*/
	Bool static_media_ressources;

	/*current simulation time of the compositor*/
	Double simulation_time;

	/*current IRI fragment if any*/
	char *fragment_uri;

	/*secondary resource scene*/
	Bool secondary_resource;

	char *redirect_xml_base;
};

GF_InlineScene *gf_inline_new(GF_InlineScene *parentScene);
void gf_inline_del(GF_InlineScene *is);
struct _od_manager *gf_inline_find_odm(GF_InlineScene *is, u16 OD_ID);
void gf_inline_disconnect(GF_InlineScene *is, Bool for_shutdown);
void gf_inline_remove_object(GF_InlineScene *is, GF_ObjectManager *odm, Bool for_shutdown);
/*browse all (media) channels and send buffering info to the app*/
void gf_inline_buffering_info(GF_InlineScene *is);
void gf_inline_attach_to_compositor(GF_InlineScene *is);
struct _mediaobj *gf_inline_get_media_object(GF_InlineScene *is, MFURL *url, u32 obj_type_hint, Bool lock_timelines);
struct _mediaobj *gf_inline_get_media_object_ex(GF_InlineScene *is, MFURL *url, u32 obj_type_hint, Bool lock_timelines, struct _mediaobj *sync_ref, Bool always_load_new, GF_Node *node_ptr);
void gf_inline_setup_object(GF_InlineScene *is, GF_ObjectManager *odm);
/*restarts inline scene - care has to be taken not to remove the scene while it is traversed*/
void gf_inline_restart(GF_InlineScene *is);
/*updates scene duration based on settings*/
void gf_inline_set_duration(GF_InlineScene *is);
/*locate media object by ODID (non dynamic ODs) or URL (dynamic ODs)*/
struct _mediaobj *gf_inline_find_object(GF_InlineScene *is, u16 ODID, char *url);
/*returns scene time in sec - exact meaning of time depends on standard used*/
Double gf_inline_get_time(void *_is);
/*returns true if the given node DEF name is the url target view (eg blabla#myview)*/
Bool gf_inline_default_scene_viewpoint(GF_Node *node);
/*register extra scene graph for on-screen display*/
void gf_inline_register_extra_graph(GF_InlineScene *is, GF_SceneGraph *extra_scene, Bool do_remove);
/*forces scene size info (without changing pixel metrics) - this may be needed by modules using extra graphs (like timedtext)*/
void gf_inline_force_scene_size(GF_InlineScene *is, u32 width, u32 height);
/*regenerate a scene graph based on available objects - can only be called for dynamic OD streams*/
void gf_inline_regenerate(GF_InlineScene *is);
/*selects given ODM for dynamic scenes*/
void gf_inline_select_object(GF_InlineScene *is, GF_ObjectManager *odm);
/*restarts dynamic scene from given time: scene graph is not reseted, objects are just restarted
instead of closed and reopened. If a media control is present on inline, from_time is overriden by MC range*/
void gf_inline_restart_dynamic(GF_InlineScene *is, u64 from_time);
/*owner inline node has been modified*/
void gf_inline_on_modified(GF_Node *node);
/*returns scene graph associated with an externProto lib - exported for VRML/X3D loaded*/
GF_SceneGraph *gf_inline_get_proto_lib(void *_is, MFURL *lib_url);
/*exported for compositor: handles filtering of "self" parameter indicating anchor only acts on container inline scene
not root one. Returns 1 if handled (cf user.h, navigate event)*/
Bool gf_inline_process_anchor(GF_Node *caller, GF_Event *evt);
/*extern proto fetcher*/
GF_SceneGraph *gf_inline_get_proto_lib(void *SceneCallback, MFURL *lib_url);
void gf_inline_force_scene_size_video(GF_InlineScene *is, GF_MediaObject *mo);
void gf_inline_sample_time(GF_InlineScene *is);
/*compares object URL with another URL - ONLY USE THIS WITH DYNAMIC ODs*/
Bool gf_mo_is_same_url(GF_MediaObject *obj, MFURL *inline_url);

void gf_mo_update_caps(GF_MediaObject *mo);


GF_Node *gf_inline_get_subscene_root(GF_Node *inline_node);
GF_Node *gf_inline_get_parent_node(GF_Node *node, u32 idx);

const char *gf_inline_get_fragment_uri(GF_Node *node);
void gf_inline_set_fragment_uri(GF_Node *node, const char *uri);

enum
{
	/*threading up to decoder*/
	GF_TERM_THREAD_FREE,
	/*single thread for all decoders*/
	GF_TERM_THREAD_SINGLE,
	/*all media (image, video, audio) decoders are threaded*/
	GF_TERM_THREAD_MULTI,
};

enum
{
	GF_TERM_RUNNING= 1,
	GF_TERM_DEAD = 1<<1,
	GF_TERM_SINGLE_THREAD = 1<<2,
	GF_TERM_MULTI_THREAD = 1<<3,
	GF_TERM_SYSDEC_RESYNC = 1<<4,
	GF_TERM_SINGLE_CLOCK = 1<<5,
	GF_TERM_DRAW_FRAME = 1<<6
};



struct _tag_terminal
{
	u32 flags;

	/*callback to user application*/	
	GF_User *user;
	/*scene compositor*/
	struct __tag_compositor *compositor;
	/*file downloader*/
	GF_DownloadManager *downloader;
	/*top level scene*/
	GF_InlineScene *root_scene;

	/*Media manager*/
	GF_List *codecs;
	/*mutex for decoder access*/
	GF_Mutex *mm_mx;
	/*decoding thread*/
	GF_Thread *mm_thread;
	/*last codec used in mm loop*/
	u32 last_codec;
	/*thread priority*/
	s32 priority;
	u32 cumulated_priority;
	/*frame duration*/
	u32 frame_duration;

	/*net services*/
	GF_List *net_services;
	/*net services to be destroyed*/
	GF_List *net_services_to_remove;
	/*channels waiting for service CONNECT ack to be setup*/
	GF_List *channels_pending;
	/*media objects pending for stop/play*/
	GF_List *media_queue;
	/*network lock*/
	GF_Mutex *net_mx;
	/*all X3D key/mouse/string sensors*/
	GF_List *x3d_sensors;
	/*all input stream decoders*/
	GF_List *input_streams;
	
	/*options (cf config doc)*/
	Bool enable_cache;
	/*data timeout for network buffering in ms - if no data is received within this timeout
	the initial buffering aborts. */
	u32 net_data_timeout;

	u32 play_state;

	u32 reload_state;
	char *reload_url;

	/*special list used by nodes needing a call to RenderNode but not in the traverese scene graph (VRML/MPEG-4 protos only). 
	For these nodes, the traverse effect passed will be NULL. This is only used by InputSensor node at the moment*/
	GF_List *nodes_pending;

	/*root node of the user prefs*/
	GF_SceneGraph *dcci_doc;

	GF_List *extensions;
	GF_List *unthreaded_extensions;
};



GF_Err gf_term_init_scheduler(GF_Terminal *term, u32 threading_mode);
void gf_term_stop_scheduler(GF_Terminal *term);
void gf_term_add_codec(GF_Terminal *term, GF_Codec *codec);
void gf_term_remove_codec(GF_Terminal *term, GF_Codec *codec);
void gf_term_start_codec(GF_Codec *codec);
void gf_term_stop_codec(GF_Codec *codec);
void gf_term_set_threading(GF_Terminal *term, u32 mode);
void gf_term_set_priority(GF_Terminal *term, s32 Priority);


/*error report function*/
void gf_term_message(GF_Terminal *app, const char *service, const char *message, GF_Err error);
/*creates service for given OD / URL*/
void gf_term_connect_object(GF_Terminal *app, GF_ObjectManager *odm, char *serviceURL, GF_ClientService *ParentService);
/*creates service for given channel / URL*/
GF_Err gf_term_connect_remote_channel(GF_Terminal *app, GF_Channel *ch, char *URL);

/*called by media manager to perform service maintenance:
servive shutdown: this is needed because service handler may be asynchronous
object Play: this is needed to properly handle multiplexed sources (all channels must be connected before play)
service restart
*/
void gf_term_handle_services(GF_Terminal *app);
/*close service and queue for delete*/
void gf_term_close_services(GF_Terminal *app, GF_ClientService *service);

/*locks net manager*/
void gf_term_lock_net(GF_Terminal *app, Bool LockIt);


/*locks scene compositor*/
void gf_term_lock_compositor(GF_Terminal *app, Bool LockIt);
/*get scene compositor time - FIXME this is not flexible enough for SMIL/Multiple time containers*/
u32 gf_term_get_time(GF_Terminal *term);
/*forces scene composition*/
void gf_term_invalidate_compositor(GF_Terminal *term);

/*callbacks for scene graph library so that all related ESM nodes are properly instanciated*/
void gf_term_node_callback(void *_is, u32 type, GF_Node *node, void *param);

/*add/rem node requiring a call to render without being present in traversed graph (VRML/MPEG-4 protos). 
For these nodes, the traverse effect passed will be NULL.*/
void gf_term_queue_node_traverse(GF_Terminal *term, GF_Node *node);
void gf_term_unqueue_node_traverse(GF_Terminal *term, GF_Node *node);



/*clock*/
struct _object_clock 
{
	u16 clockID;	
	GF_Terminal *term;
	GF_Mutex *mx;
	/*no_time_ctrl : set if ANY stream running on this clock has no time control capabilities - this avoids applying
	mediaControl and others that would break stream dependencies*/
	Bool use_ocr, clock_init, has_seen_eos, no_time_ctrl;
	u32 init_time, StartTime, PauseTime, Paused;
	/*the number of streams buffering on this clock*/
	u32 Buffering;
	/*associated media control if any*/
	struct _media_control *mc;
	/*for MC only (no FlexTime)*/
	Fixed speed;
	u32 discontinuity_time;
	s32 drift;
};

/*destroys clock*/
void gf_clock_del(GF_Clock *ck);
/*finds a clock by ID or by ES_ID*/
GF_Clock *gf_clock_find(GF_List *Clocks, u16 clockID, u16 ES_ID);
/*attach clock returns a new clock or the clock this stream (ES_ID) depends on (OCR_ES_ID)
hasOCR indicates whether the stream being attached carries object clock references
@clocks: list of clocks in ES namespace (service)
@is: inline scene to solve clock dependencies
*/
GF_Clock *gf_clock_attach(GF_List *clocks, GF_InlineScene *is, u16 OCR_ES_ID, u16 ES_ID, s32 hasOCR);
/*reset clock (only called by channel owning clock)*/
void gf_clock_reset(GF_Clock *ck);
/*stops clock (only called for scene clock)*/
void gf_clock_stop(GF_Clock *ck);
/*return clock time in ms*/
u32 gf_clock_time(GF_Clock *ck);
/*return ellapsed time in ms since start of the clock*/
u32 gf_clock_ellapse_time(GF_Clock *ck);
/*sets clock time - FIXME: drift updates for OCRs*/
void gf_clock_set_time(GF_Clock *ck, u32 TS);
/*return clock time in ms without drift adjustment - used by audio objects only*/
u32 gf_clock_real_time(GF_Clock *ck);
/*pause the clock*/
void gf_clock_pause(GF_Clock *ck);
/*resume the clock*/
void gf_clock_resume(GF_Clock *ck);
/*returns true if clock started*/
Bool gf_clock_is_started(GF_Clock *ck);
/*toggles buffering on (clock is paused at the first stream buffering) */
void gf_clock_buffer_on(GF_Clock *ck);
/*toggles buffering off (clock is paused at the last stream restarting) */
void gf_clock_buffer_off(GF_Clock *ck);
/*set clock speed scaling factor*/
void gf_clock_set_speed(GF_Clock *ck, Fixed speed);
/*set clock drift - used to resync audio*/
void gf_clock_adjust_drift(GF_Clock *ck, s32 ms_drift);

enum
{
	/*channel is setup and waits for connection request*/
	GF_ESM_ES_SETUP = 0,
	/*waiting for server reply*/
	GF_ESM_ES_WAIT_FOR_ACK,
	/*connection OK*/
	GF_ESM_ES_CONNECTED,
	/*data exchange on this service/channel*/
	GF_ESM_ES_RUNNING,
	/*deconnection OK - a download channel can automatically disconnect when download is done*/
	GF_ESM_ES_DISCONNECTED,
	/*service/channel is not (no longer) available/found and should be removed*/
	GF_ESM_ES_UNAVAILABLE
};

/*data channel (elementary stream)*/
struct _es_channel 
{
	/*security check on channel*/
	u32 chan_id;
	/*service this channel belongs to*/
	GF_ClientService *service;
	/*stream descriptor*/
	GF_ESD *esd;
	/*parent OD for this stream*/
	struct _od_manager *odm;
	u32 es_state;
	Bool is_pulling;
	u32 media_padding_bytes;
	/*IO mutex*/
	GF_Mutex *mx;
	u32 AU_Count;
	/*decoding buffers for push mode*/
	struct _decoding_buffer * AU_buffer_first, * AU_buffer_last;
	/*static decoding buffer for pull mode*/
	struct _decoding_buffer * AU_buffer_pull;
	/*channel buffer flag*/
	Bool BufferOn;
	/*min level to trigger buffering on, max to trigger it off. */
	u32 MinBuffer, MaxBuffer;
	/*amount of buffered media - this is the DTS of the last received AU minus the onject clock time, to make sure
	we always have MaxBuffer ms ready for composition when resuming the clock*/
	s32 BufferTime;
	/*last received AU time - if exceeding a certain time and buffering is on, buffering is turned off.
	This is needed for streams with very short duration (less than buffer time) and stream with only one AU (BIFS/OD)*/
	u32 last_au_time;
	/*Current reassemnbling buffer - currently packets are NOT reordered, only AUs are*/
	char *buffer;
	u32 len, allocSize;
	/*only for last packet of an AU*/
	u8 padingBits;
	Bool IsEndOfStream;
	/*	SL reassembler	*/
	/*current AU TSs*/
	u32 DTS, CTS;
	/*AU and Packet seq num info*/
	u32 au_sn, pck_sn;
	u32 max_au_sn, max_pck_sn;
	/*the AU length indicated in the SL Header. */
	u32 AULength;
	/*state indicator: 0 OK, 1: not tuned in, 2: has error and needs RAP*/
	u32 stream_state;
	/*the AU in reception is RAP*/
	Bool IsRap;
	/*signal that next AU is an AU start*/
	Bool NextIsAUStart;
	/*if codec resilient, packet drops are not considered as fatal for AU reconstruction (eg no wait for RAP)*/
	Bool codec_resilient;
	/*when starting a channel, the first AU is ALWAYS fetched when buffering - this forces
	BIFS and OD to be decoded and first frame render, in order to detect media objects that would also need
	buffering - note this doesn't affect the clock, it is still paused if buffering*/
	Bool first_au_fetched;

	/* used in Carousel, to skip packets until the end of AU */ 
	Bool skip_carousel_au;
	
	/* TimeStamp to Media Time mapping*/
	/*TS (in TSResolution) corresponding to the SeedTime of the decoder. Delivered by net, otherwise 0*/
	u64 seed_ts;
	/*media time offset corresponding to SeedTS. This is needed when the channel doesn't own the clock*/
	u32 ts_offset;
	/*scaling factors to remap to timestamps in milliseconds*/
	u64 ts_res;
	Double ocr_scale;
	/*clock driving this stream - currently only CTS is supported (no OCR)*/
	struct _object_clock *clock;
	/*flag for clock init. Only a channel owning the clock will set this flag on clock init*/
	Bool IsClockInit;

	/*duration of last received AU if any, 0 if not known (most of the time)*/
	u32 au_duration;
	/*A channel with this flag set considers each incoming packet as a complete AU and assigns timestamps 
	upon reception matching the reception time, then dispatching it into the decoding buffer (only tested
	with audi video). This flag is turned on by setting esd->slconfig->predefined to 'SLPredef_SkipSL' */
	Bool skip_sl;

	/*indicates that decoding can be called directly when receiving a complete AU on this channel
	This is used by systems streams in non-seekable (eg broadcast/multicast, MPEG-2 TS multiplexes) to 
	make sure resources are setup as fast as possible. If the AU is too early, it will be kept in the 
	decoding buffer*/
	Bool dispatch_after_db;

	/*indicates that decoding is called directly when receiving a packet on this channel
	This is used to bypass SL defragmenting and decoding buffer for EIT internal streams*/
	Bool bypass_sl_and_db;

	GF_IPMPTool *ipmp_tool;
	Bool is_protected;

	/*TSs as received from network - these are used for cache storage*/
	u64 net_dts, net_cts;
};

/*creates a new channel for this stream*/
GF_Channel *gf_es_new(GF_ESD *esd);
/*destroys channel*/
void gf_es_del(GF_Channel *ch);
/*(un)locks channel*/
void gf_es_lock(GF_Channel *ch, u32 LockIt);
/*setup channel for reception of data*/
GF_Err gf_es_start(GF_Channel *ch);
/*stop channel from receiving data*/
GF_Err gf_es_stop(GF_Channel *ch);
/*handles reception of an SL PDU*/
void gf_es_receive_sl_packet(GF_ClientService *serv, GF_Channel *ch, char *StreamBuf, u32 StreamLength, GF_SLHeader *header, GF_Err reception_status);
/*signals end of stream on the channel*/
void gf_es_on_eos(GF_Channel *ch);
/*fetches first AU available for decoding on this channel*/
struct _decoding_buffer *gf_es_get_au(GF_Channel *ch);
/*drops first AU on this channel*/
void gf_es_drop_au(GF_Channel *ch);
/*performs final setup upon connection confirm*/
void gf_es_on_connect(GF_Channel *ch);
/*reconfigure SL for this channel*/
void gf_es_reconfig_sl(GF_Channel *ch, GF_SLConfig *slc);
/*hack for streaming: whenever a time map (media time <-> TS time) event is received on the channel reset decoding buffer
this is needed because all server tested resend packets on already running channel*/
void gf_es_map_time(GF_Channel *ch, Bool reset);
/*dummy channels are used by scene decoders which don't use ESM but load directly the scene graph themselves
these channels are ALWAYS pulling ones, and this function will init the channel clock if needed*/
void gf_es_init_dummy(GF_Channel *ch);
/*setup DRM info*/
void gf_es_config_drm(GF_Channel *ch, GF_NetComDRMConfig *isma_cryp);

/*
		decoder stuff
*/
enum
{
	/*stop: the decoder is not playing*/
	GF_ESM_CODEC_STOP	=	0,
	/*stop: the decoder is playing*/
	GF_ESM_CODEC_PLAY	=	1,
	/*End Of Stream: when the base layer signals it's done, this triggers media-specific
	handling of the CB. 
	For video, the output is kept alive, For audio, the output is reseted (don't want audio loop ;)*/
	GF_ESM_CODEC_EOS	=	2,
	/*pause: the decoder is stoped but the CB is kept intact
	THIS IS NOT USED AS A CODEC STATUS, but only for signaling that the CB shouldn't 
	be reseted - the real status of a "paused" decoder is STOP*/
	GF_ESM_CODEC_PAUSE	=	3,
	/*Buffer: transition state: the decoder runs (fetch data/decode) but the clock
	is not running (no composition). This is used for rebuffering channels (rtp...)*/
	GF_ESM_CODEC_BUFFER =	4
};

enum
{
	GF_ESM_CODEC_HAS_UPSTREAM = 1,
	/*the codec uses the interface from another codec (only used by private scene streams to handle
	any intern sprite/animation streams)*/
	GF_ESM_CODEC_IS_USE = 1<<1,
	/*set for OD codec when static (ressources are declared in OD stream esd a la ISMA*/
	GF_ESM_CODEC_IS_STATIC_OD = 1<<2,
};

struct _generic_codec 
{
	/*codec type (streamType from base layer)*/
	u32 type;
	u32 flags;
	/*current decoder interface */
	GF_BaseDecoder *decio;
	/*composition memory for media streams*/
	struct _composition_memory *CB;
	/*input media channles*/
	GF_List *inChannels;
	/*a pointer to the OD that owns the decoder.*/
	struct _od_manager *odm;
	u32 Status;
	Bool Muted;
	struct _object_clock *ck;
	/*priority of this media object. This is ALWAYS the base layer priority
	PriorityBoost is set when the CB is under critical limit (for now only audio uses the feature)
	and results in a bigger time slice for the codec. Only on/off value for now*/
	u32 Priority, PriorityBoost;
	/*last processed DTS - sanity check for scalability*/
	u32 last_unit_dts;
	/*last processed CTS on base layer - seeking detection*/
	u32 last_unit_cts;
	/*in case the codec performs temporal re-ordering itself*/
	Bool is_reordering;
	u32 prev_au_size;
	u32 bytes_per_sec;
	Double fps;

	/*statistics*/
	u32 last_stat_start, cur_bit_size;
	u32 avg_bit_rate, max_bit_rate;
	u32 total_dec_time, nb_dec_frames, max_dec_time;
	/*number of droped frames*/
	u32 nb_droped;

	/*for CTS reconstruction (channels not using SL): we cannot just update timing at each frame, not precise enough 
	since we use ms and not microsec TSs*/
	u32 cur_audio_bytes, cur_video_frames;
};

GF_Codec *gf_codec_new(GF_ObjectManager *odm, GF_ESD *base_layer, s32 PL, GF_Err *e);
void gf_codec_del(GF_Codec *codec);
GF_Err gf_codec_add_channel(GF_Codec *codec, GF_Channel *ch);
/*returns TRUE if stream was present, false otherwise*/
Bool gf_codec_remove_channel(GF_Codec *codec, GF_Channel *ch);
GF_Err gf_codec_process(GF_Codec *codec, u32 TimeAvailable);
GF_Err gf_codec_get_capability(GF_Codec *codec, GF_CodecCapability *cap);
GF_Err gf_codec_set_capability(GF_Codec *codec, GF_CodecCapability cap);
void gf_codec_set_status(GF_Codec *codec, u32 Status);
/*returns a new codec using an existing loaded decoder - only used by private scene to handle != timelines, for 
instance when loading a BT with an animation stream*/
GF_Codec *gf_codec_use_codec(GF_Codec *codec, GF_ObjectManager *odm);

/*OD manager*/


/*all inserted ODs have this ODID*/
#define GF_ESM_DYNAMIC_OD_ID	1050

enum
{
	/*flag set if object cannot be time-controloed*/
	GF_ODM_NO_TIME_CTRL = (1<<1),
	/*flag set if subscene uses parent scene timeline*/
	GF_ODM_INHERIT_TIMELINE = (1<<2),
	/*flag set if object has been redirected*/
	GF_ODM_REMOTE_OD = (1<<3),
	/*flag set if object has profile indications*/
	GF_ODM_HAS_PROFILES = (1<<4),
	/*flag set if object governs profile of inline subscenes*/
	GF_ODM_INLINE_PROFILES = (1<<5),
	/*flag set if object declared by network service, not from OD stream*/
	GF_ODM_NOT_IN_OD_STREAM = (1<<6),

	/*dynamic flags*/
	
	/*flag set if associated subscene must be regenerated*/
	GF_ODM_REGENERATE_SCENE = (1<<10),
};

enum
{
	GF_ODM_STATE_STOP,
	GF_ODM_STATE_PLAY,
	GF_ODM_STATE_IN_SETUP,
	GF_ODM_STATE_BLOCKED,
};

struct _od_manager
{
	/*pointer to terminal*/
	struct _tag_terminal *term;
	/*the service used by this ODM. If the service private data is this ODM, then the service was created for this ODM*/
	GF_ClientService *net_service;
	/*parent scene or NULL for root scene*/
	struct _inline_scene *parentscene;
	/*channels associated with this object (media channels, OCR, IPMP, OCI, etc)*/
	GF_List *channels;
	/*sub scene for inline/animation or NULL */
	struct _inline_scene *subscene;
	/*object codec (media or BIFS for AnimationStream) attached if any*/
	struct _generic_codec *codec;
	/*OCI codec attached if any*/
	struct _generic_codec *oci_codec;
	/*OCR codec attached if any*/
	struct _generic_codec *ocr_codec;

	/*MPEG-4 object descriptor*/
	GF_ObjectDescriptor *OD;

	/*exclusive access is required since rendering and media management don't always take place in the same thread*/
	GF_Mutex *mx;

	u32 flags;

	/*PLs*/
	u8 Audio_PL, Graphics_PL, OD_PL, Scene_PL, Visual_PL;
	
	/*interface with scene rendering*/
	struct _mediaobj *mo;
	
	/*number of channels with connection not yet acknowledge*/
	u32 pending_channels;
	u32 state;
	/* during playback: timing as evaluated by the composition memory or the scene codec */
	u32 current_time;
	/*full object duration 0 if unknown*/
	u64 duration;
	/*
	upon start: media start time as requested by scene compositor (eg not media control)
	set to -1 upon stop to postpone stop request
	*/
	u64 media_start_time, media_stop_time;

	/*the one and only media control currently attached to this object*/
	struct _media_control *media_ctrl;
	/*the list of media control controling the object*/
	GF_List *mc_stack;
	/*the media sensor(s) attached to this object*/
	GF_List *ms_stack;
};


GF_ObjectManager *gf_odm_new();
void gf_odm_del(GF_ObjectManager *ODMan);
void gf_odm_lock(GF_ObjectManager *odm, u32 LockIt);

/*setup service entry point*/
void gf_odm_setup_entry_point(GF_ObjectManager *odm, const char *sub_url);
/*setup OD*/
void gf_odm_setup_object(GF_ObjectManager *odm, GF_ClientService *parent_serv);
/*disctonnect OD and removes it if desired (otherwise only STOP is propagated)*/
void gf_odm_disconnect(GF_ObjectManager *odman, Bool do_remove);
/*setup an ESD*/
GF_Err gf_odm_setup_es(GF_ObjectManager *odm, GF_ESD *esd, GF_ClientService *service, GF_MediaObject *sync_ref);
/*removes an ESD (this destroys associated channel if any)*/
void gf_odm_remove_es(GF_ObjectManager *odm, u16 ES_ID);
/*set stream duration - updates object duration accordingly*/
void gf_odm_set_duration(GF_ObjectManager *odm, GF_Channel *, u64 stream_duration);
/*signals end of stream on channels*/
void gf_odm_on_eos(GF_ObjectManager *odm, GF_Channel *);
/*start Object streams and queue object for network PLAY*/
void gf_odm_start(GF_ObjectManager *odm);
/*stop OD streams*/
void gf_odm_stop(GF_ObjectManager *odm, Bool force_close);
/*send PLAY request to network - needed to properly handle multiplexed inputs 
ONLY called by service handler (media manager thread)*/
void gf_odm_play(GF_ObjectManager *odm);

/*returns 1 if this is a segment switch, 0 otherwise - takes care of object restart if segment switch*/
Bool gf_odm_check_segment_switch(GF_ObjectManager *odm);
/*pause object (mediaControl use only)*/
void gf_odm_pause(GF_ObjectManager *odm);
/*resume object (mediaControl use only)*/
void gf_odm_resume(GF_ObjectManager *odm);
/*set object speed*/
void gf_odm_set_speed(GF_ObjectManager *odm, Fixed speed);
/*returns the clock of the media stream (video, audio or bifs), NULL otherwise */
struct _object_clock *gf_odm_get_media_clock(GF_ObjectManager *odm);
/*adds segment descriptors targeted by the URL to the list and sort them - the input list must be empty*/
void gf_odm_init_segments(GF_ObjectManager *odm, GF_List *list, MFURL *url);
/*returns true if this OD depends on the given clock*/
Bool gf_odm_shares_clock(GF_ObjectManager *odm, struct _object_clock *ock);

GF_Segment *gf_odm_find_segment(GF_ObjectManager *odm, char *descName);
/*locks ODM with destruction check - returns 0 if object manager is not attached to object*/
Bool gf_odm_lock_mo(struct _mediaobj *mo);


/*GF_MediaObject: link between real object manager and scene. although there is a one-to-one mapping between a 
MediaObject and an ObjectManager, we have to keep them seperated in order to handle OD remove commands which destroy
ObjectManagers. */
struct _mediaobj
{
	/*type is as defined in constants.h # GF_MEDIA_OBJECT_* */
	u32 type;
	/*one of the above flags*/
	u32 flags;


	/* private to ESM*/

	/*media object manager - private to the sync engine*/
	struct _od_manager *odm;
	/*OD ID of the object*/
	u32 OD_ID;
	/*OD URL for object not using MPEG4 OD urls*/
	MFURL URLs;
	/*session join*/
	u32 num_open;
	/*shared object restart handling*/
	u32 num_to_restart, num_restart;
	Fixed speed;

	/*shared object info: if 0 a new frame will be checked, otherwise current is returned*/
	u32 nb_fetch;
	/*frame presentation time*/
	u32 timestamp;
	/*data frame size*/
	u32 framesize;
	/*pointer to data frame */
	char *frame;
	/*nodes currently registered with the media object - used to dispatch MediaAccessEvents*/
	GF_List *nodes;
	/*pointer to the node responsible for the creation of this media object
	ONLY used for scene media type (animationStreams) 
	Reset upon creation of the decoder.
	*/
	void *node_ptr;
};

GF_MediaObject *gf_mo_new();


/*used for delayed channel setup*/
typedef struct 
{
	struct _generic_codec *dec;
	struct _es_channel *ch;	
} GF_ChannelSetup;

/*post-poned channel connect*/
GF_Err gf_odm_post_es_setup(struct _es_channel *ch, struct _generic_codec *dec, GF_Err err);

/*
	special entry point: specify directly a service interface for service input
*/
void gf_term_attach_service(GF_Terminal *term, GF_InputService *service_hdl);

/*media access events */
void gf_term_service_media_event(GF_ObjectManager *odm, u32 event_type);

u32 URL_GetODID(MFURL *url);

#ifdef __cplusplus
}
#endif


#endif	/*_GF_TERMINAL_DEV_H_*/


