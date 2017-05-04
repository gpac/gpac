/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#include <gpac/thread.h>
#include <gpac/filters.h>

#include <gpac/terminal.h>
#include <gpac/term_info.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/mpeg4_odf.h>

#include <gpac/mediaobject.h>
#include <gpac/bifs.h>

typedef struct _gf_scene GF_Scene;
typedef struct _gf_addon_media GF_AddonMedia;
typedef struct _object_clock GF_Clock;

/*forwards all clocks of the given amount of time. Can only be used when terminal is in paused mode
this is mainly designed for movie dumping in MP4Client*/
GF_Err gf_term_step_clocks(GF_Terminal * term, u32 ms_diff);

u32 gf_term_sample_clocks(GF_Terminal *term);


struct _tag_terminal
{
	GF_User *user;
	GF_Compositor *compositor;
	GF_FilterSession *fsess;
};


typedef struct
{
	/*od_manager owning service, NULL for services created for remote channels*/
	struct _od_manager *owner;

	/*service url*/
	char *url;
	GF_Filter *source_filter;

	/*number of attached remote channels ODM (ESD URLs)*/
//	u32 nb_ch_users;
	/*number of attached remote ODM (OD URLs)*/
	u32 nb_odm_users;

	/*clock objects. Kept at service level since ESID namespace is the service one*/
	GF_List *Clocks;

	Fixed set_speed;
	Bool connect_ack;
} GF_SceneNamespace;



/*opens service - performs URL concatenation if parent service specified*/
GF_SceneNamespace *gf_scene_ns_new(GF_Scene *scene, GF_ObjectManager *owner, const char *url, const char *parent_url);
/*destroy service*/
void gf_scene_ns_del(GF_SceneNamespace *ns, GF_Scene *scene);

void gf_scene_ns_connect_object(GF_Scene *scene, GF_ObjectManager *odm, char *serviceURL, char *parent_url);



/*
		GF_Scene object. This is the structure handling all scene management, mainly:
			- list of resources (media objects) used
			- scene time management
			- reload/seek
			- Dynamic scene (without a scene description)*
		Each scene registers itself as the private data of its associated scene graph.

		The scene object is also used by Inline nodes and all media resources of type "scene", eg <foreignObject>, <animation>
*/
struct _gf_scene
{
	/*root OD of the subscene, ALWAYS namespace of the parent scene*/
	struct _od_manager *root_od;

	/*all sub resources of this scene (eg, list of GF_ObjectManager), namespace of this scene. This includes
	both external resources (urls) and ODs sent in MPEG-4 systems*/
	GF_List *resources;

	/*list of GF_MediaObject - these are the links betwwen scene nodes (URL, xlink:href) and media resources.
	We need this link because of MPEG-4 Systems, where an OD (media resource) can be removed or replaced by the server
	without the scene being modified*/
	GF_List *scene_objects;

	/*list of extra scene graphs (text streams, generic OSDs, ...)*/
	GF_List *extra_scenes;
	/*scene graph*/
	GF_SceneGraph *graph;
	/*graph state - if not attached, no traversing of inline
	0: not attached
	1: attached
	2: temp graph attached. The temp graph is generated when waiting for the first scene AU to be processed
	*/
	u32 graph_attached;

	//indicates a valid object is attached to the scene
	Bool object_attached;

	/*set to 1 to force all sub-resources to share the timeline of this scene*/
	Bool force_single_timeline;

	/*set to 1 once the forced size has been sez*/
	Bool force_size_set;

	/*callback to call to dispatch SVG MediaEvent - this is a pointer to function only because of linking issues
	with static libgpac (avoids depending on SpiderMonkey and OpenGL32 if not needed)*/
	void (*on_media_event)(GF_Scene *scene, u32 type);

	/*duration of inline scene*/
	u64 duration;


	u32 nb_buffering;

	/*max timeshift of all objects*/
	u32 timeshift_depth;

	/*WorldInfo node or <title> node*/
	void *world_info;

	/*current IRI fragment if any*/
	char *fragment_uri;

	/*secondary resource scene - this is specific to SVG: a secondary resource scene is <use> or <fonturi>, and all resources
	identified in a secondary resource must be checked for existence and created in the primary resource*/
	Bool secondary_resource;

	/*needed by some apps in GPAC which manipulate the scene tree in different locations as the resources*/
	char *redirect_xml_base;


	/*FIXME - Dynamic scenes are only supported through VRML/BIFS nodes, we should add support for SVG scene graph
	generation if needed*/
	Bool is_dynamic_scene;
	/*for MPEG-2 TS, indicates the current serviceID played from mux*/
	u32 selected_service_id;

	/*URLs of current video, audio and subs (we can't store objects since they may be destroyed when seeking)*/
	SFURL visual_url, audio_url, text_url, dims_url;

	Bool is_tiled_srd;
	u32 srd_type;
	s32 srd_min_x, srd_max_x, srd_min_y, srd_max_y;


	Bool end_of_scene;
#ifndef GPAC_DISABLE_VRML
	/*list of externproto libraries*/
	GF_List *extern_protos;

	/*togles inline restart - needed because the restart may be triggered from inside the scene or from
	parent scene, hence 2 render passes must be used
	special value 2 means scene URL changes (for anchor navigation*/
	u32 needs_restart;

	/*URL of the current parent Inline node, only set during traversal*/
	MFURL *current_url;


	/*list of M_Storage nodes active*/
	GF_List *storages;

	/*list of M_KeyNavigator nodes*/
	GF_List *keynavigators;
#endif

	Bool disable_hitcoord_notif;

	u32 addon_position, addon_size_factor;

	GF_List *declared_addons;
	//set when content is replaced by an addon (DASH PVR mode)
	Bool main_addon_selected;
	u32 sys_clock_at_main_activation, obj_clock_at_main_activation;

	//0: no pause - 1: paused and trigger pause command to net, 2: only clocks are paused but commands not sent
	u32 first_frame_pause_type;
	u32 vr_type;

//	GF_Scene *parent_scene;
	GF_Compositor *compositor;
	
	//only for root scene
	GF_List *namespaces;
};

GF_Scene *gf_scene_new(GF_Compositor *compositor, GF_Scene *parentScene);
void gf_scene_del(GF_Scene *scene);

struct _od_manager *gf_scene_find_odm(GF_Scene *scene, u16 OD_ID);
void gf_scene_disconnect(GF_Scene *scene, Bool for_shutdown);

GF_Scene *gf_scene_get_root_scene(GF_Scene *scene);
Bool gf_scene_is_root(GF_Scene *scene);

void gf_scene_remove_object(GF_Scene *scene, GF_ObjectManager *odm, u32 for_shutdown);
/*browse all (media) channels and send buffering info to the app*/
void gf_scene_buffering_info(GF_Scene *scene);
void gf_scene_attach_to_compositor(GF_Scene *scene);
struct _mediaobj *gf_scene_get_media_object(GF_Scene *scene, MFURL *url, u32 obj_type_hint, Bool lock_timelines);
struct _mediaobj *gf_scene_get_media_object_ex(GF_Scene *scene, MFURL *url, u32 obj_type_hint, Bool lock_timelines, struct _mediaobj *sync_ref, Bool force_new_if_not_attached, GF_Node *node_ptr);
void gf_scene_setup_object(GF_Scene *scene, GF_ObjectManager *odm);
/*updates scene duration based on sub objects*/
void gf_scene_set_duration(GF_Scene *scene);
/*updates scene timeshift based on sub objects*/
void gf_scene_set_timeshift_depth(GF_Scene *scene);
/*locate media object by ODID (non dynamic ODs) or URL (dynamic ODs)*/
struct _mediaobj *gf_scene_find_object(GF_Scene *scene, u16 ODID, char *url);
/*returns scene time in sec - exact meaning of time depends on standard used*/
Double gf_scene_get_time(void *_is);
/*register extra scene graph for on-screen display*/
void gf_scene_register_extra_graph(GF_Scene *scene, GF_SceneGraph *extra_scene, Bool do_remove);
/*forces scene size info (without changing pixel metrics) - this may be needed by modules using extra graphs (like timedtext)*/
void gf_scene_force_size(GF_Scene *scene, u32 width, u32 height);
/*regenerate a scene graph based on available objects - can only be called for dynamic OD streams*/
void gf_scene_regenerate(GF_Scene *scene);
/*selects given ODM for dynamic scenes*/
void gf_scene_select_object(GF_Scene *scene, GF_ObjectManager *odm);
/*restarts dynamic scene from given time: scene graph is not reseted, objects are just restarted
instead of closed and reopened. If a media control is present on inline, from_time is overriden by MC range*/
void gf_scene_restart_dynamic(GF_Scene *scene, s64 from_time, Bool restart_only, Bool disable_addon_check);

void gf_scene_insert_pid(GF_Scene *scene, GF_SceneNamespace *sns, GF_FilterPid *pid, Bool is_in_iod);

/*exported for compositor: handles filtering of "self" parameter indicating anchor only acts on container inline scene
not root one. Returns 1 if handled (cf user.h, navigate event)*/
Bool gf_scene_process_anchor(GF_Node *caller, GF_Event *evt);
void gf_scene_force_size_to_video(GF_Scene *scene, GF_MediaObject *mo);

//check clock status.
//If @check_buffering is 0, returns 1 if all clocks have seen eos, 0 otherwise
//If @check_buffering is 1, returns 1 if no clock is buffering, 0 otheriwse
Bool gf_scene_check_clocks(GF_SceneNamespace *ns, GF_Scene *scene, Bool check_buffering);

void gf_scene_notify_event(GF_Scene *scene, u32 event_type, GF_Node *n, void *dom_evt, GF_Err code, Bool no_queueing);

void gf_scene_mpeg4_inline_restart(GF_Scene *scene);
void gf_scene_mpeg4_inline_check_restart(GF_Scene *scene);


GF_Node *gf_scene_get_subscene_root(GF_Node *inline_node);

void gf_scene_select_main_addon(GF_Scene *scene, GF_ObjectManager *odm, Bool set_on, u32 current_clock_time);
void gf_scene_reset_addons(GF_Scene *scene);

#ifndef GPAC_DISABLE_VRML

/*extern proto fetcher*/
typedef struct
{
	MFURL *url;
	GF_MediaObject *mo;
} GF_ProtoLink;

/*returns true if the given node DEF name is the url target view (eg blabla#myview)*/
Bool gf_inline_is_default_viewpoint(GF_Node *node);

GF_SceneGraph *gf_inline_get_proto_lib(void *_is, MFURL *lib_url);
Bool gf_inline_is_protolib_object(GF_Scene *scene, GF_ObjectManager *odm);

/*restarts inline scene - care has to be taken not to remove the scene while it is traversed*/
void gf_inline_restart(GF_Scene *scene);

#endif

/*compares object URL with another URL - ONLY USE THIS WITH DYNAMIC ODs*/
Bool gf_mo_is_same_url(GF_MediaObject *obj, MFURL *an_url, Bool *keep_fragment, u32 obj_hint_type);

void gf_mo_update_caps(GF_MediaObject *mo);


const char *gf_scene_get_fragment_uri(GF_Node *node);
void gf_scene_set_fragment_uri(GF_Node *node, const char *uri);


#if FILTER_FIXME


/*URI relocators are used for containers like zip or ISO FF with file items. The relocator
is in charge of translating the URI, potentially extracting the associated resource and sending
back the new (local or not) URI. Only the interface is defined, URI translators are free to derive from them

relocate a URI - if NULL is returned, this relocator is not concerned with the URI
otherwise returns the translated URI
*/

#define GF_TERM_URI_RELOCATOR	\
	Bool (*relocate_uri)(void *__self, const char *parent_uri, const char *uri, char *out_relocated_uri, char *out_localized_uri);		\
 
typedef struct __gf_uri_relocator GF_URIRelocator;

struct __gf_uri_relocator
{
	GF_TERM_URI_RELOCATOR
};

typedef struct
{
	GF_TERM_URI_RELOCATOR
	GF_Terminal *term;
	char *szAbsRelocatedPath;
} GF_TermLocales;

#define	MAX_SHORTCUTS	200

typedef struct
{
	u8 code;
	u8 mods;
	u8 action;
} GF_Shortcut;

typedef struct
{
	void *udta;
	/*called when an event should be filtered
	*/
	Bool (*on_event)(void *udta, GF_Event *evt, Bool consumed_by_compositor);
} GF_TermEventFilter;

GF_Err gf_term_add_event_filter(GF_Terminal *terminal, GF_TermEventFilter *ef);
GF_Err gf_term_remove_event_filter(GF_Terminal *terminal, GF_TermEventFilter *ef);

void gf_scene_register_associated_media(GF_Scene *scene, GF_AssociatedContentLocation *addon_info);
void gf_scene_notify_associated_media_timeline(GF_Scene *scene, GF_AssociatedContentTiming *addon_time);

#endif


/*clock*/
struct _object_clock
{
	u16 clockID;
	GF_Compositor *compositor;
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
	u32 data_timeout;
	Bool probe_ocr;
	Bool broken_pcr;
	u32 last_TS_rendered;
	u32 service_id;

	//media time in ms corresponding to the init tmiestamp of the clock
	u32 media_time_at_init;
	Bool has_media_time_shift;

	u16 ocr_on_esid;

	u64 ts_shift;
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
GF_Clock *gf_clock_attach(GF_List *clocks, GF_Scene *scene, u16 OCR_ES_ID, u16 ES_ID, s32 hasOCR);
/*reset clock (only called by channel owning clock)*/
void gf_clock_reset(GF_Clock *ck);
/*return clock time in ms*/
u32 gf_clock_time(GF_Clock *ck);
/*return media time in ms*/
u32 gf_clock_media_time(GF_Clock *ck);

/*return time in ms since clock started - may be different from clock time when seeking or live*/
u32 gf_clock_elapsed_time(GF_Clock *ck);

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



/*OD manager*/

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
	/*flag set if object is an entry point of the network service*/
	GF_ODM_SERVICE_ENTRY = (1<<7),

	/*flag set if object has been started before any start request from the scene*/
	GF_ODM_PREFETCH = (1<<8),

	/*flag set if object has been deleted*/
	GF_ODM_DESTROYED = (1<<9),
	/*dynamic flags*/

	/*flag set if associated subscene must be regenerated*/
	GF_ODM_REGENERATE_SCENE = (1<<10),

	/*flag set for first play request*/
	GF_ODM_INITIAL_BROADCAST_PLAY = (1<<11),

	/*flag set until ODM is setup*/
	GF_ODM_NOT_SETUP = (1<<12),

	/*flag set when ODM is setup*/
	GF_ODM_PAUSED = (1<<13),

	/*flag set when ODM is setup*/
	GF_ODM_PAUSE_QUEUED = (1<<14),
};

enum
{
	GF_ODM_STATE_STOP,
	GF_ODM_STATE_PLAY,
};

enum
{
	GF_ODM_ACTION_PLAY,
	GF_ODM_ACTION_STOP,
	GF_ODM_ACTION_DELETE,
	GF_ODM_ACTION_SCENE_DISCONNECT,
	GF_ODM_ACTION_SCENE_RECONNECT,
	GF_ODM_ACTION_SCENE_INLINE_RESTART,
	GF_ODM_ACTION_SETUP
};

typedef struct
{
	u32 pid_id; //esid for clock solving
	u32 state;
	Bool has_seen_eos;
	GF_FilterPid *pid;
} GF_ODMExtraPid;


struct _od_manager
{
	/*parent scene or NULL for root scene*/
	GF_Scene *parentscene;
	/*sub scene for inline or NULL */
	GF_Scene *subscene;

	/*namespace for clocks*/
	GF_SceneNamespace *scene_ns;

	/*clock for this object*/
	GF_Clock *ck;
	//one of the pid contributing to this object acts as the clock reference
	Bool owns_clock;


	u32 nb_dropped;
	Bool low_latency_mode;
	
	GF_FilterPid *pid;
	//object ID for linking with mediaobjects
	u32 ID;
	u32 pid_id; //esid for clock solving
	
	//parent service ID as defined from input
	u32 ServiceID;
	Bool hybrid_layered_coded;

	Bool has_seen_eos;
	GF_List *extra_pids;

	Bool clock_inherited;
	//0 or 1, except for IOD where we may have several BIFS/OD streams
	u32 nb_buffering;
	u32 buffer_max_us, buffer_min_us, buffer_playout_us;

	//internal hash for source allowing to distinguish input PIDs sources
	u32 source_id;

	//media type for this object
	u32 type, original_oti;
	
	u32 flags;

	GF_MediaObject *sync_ref;

	/*interface with scene rendering*/
	struct _mediaobj *mo;

	/*number of channels with connection not yet acknowledge*/
	u32 pending_channels;
	u32 state;
	/* during playback: timing as evaluated by the composition memory or the scene codec - this is the timestamp + media time at clock init*/
	u32 media_current_time;
	/*full object duration 0 if unknown*/
	u64 duration;
	/*
	upon start: media start time as requested by scene compositor (eg not media control)
	set to -1 upon stop to postpone stop request
	*/
	u64 media_start_time;
	s64 media_stop_time;

	/*full object timeshift depth in ms, 0 if no timeshift, (u32) -1  is infinity */
	u32 timeshift_depth;

	u32 action_type;

	Fixed set_speed;
	Bool disable_buffer_at_next_play;

//	u32 raw_media_frame_pending;
	GF_Semaphore *raw_frame_sema;

#ifndef GPAC_DISABLE_VRML
	/*the one and only media control currently attached to this object*/
	struct _media_control *media_ctrl;
	/*the list of media control controling the object*/
	GF_List *mc_stack;
	/*the media sensor(s) attached to this object*/
	GF_List *ms_stack;
#endif

	//only set on root OD of addon subscene, which gather all the hybrid resources
	GF_AddonMedia *addon;
	//set to true if this is a scalable addon for an existing object
	Bool scalable_addon;

	//for a regular ODM, this indicates that the current scalable_odm associated
	struct _od_manager *upper_layer_odm;
	//for a scalable ODM, this indicates the lower layer odm associated
	struct _od_manager *lower_layer_odm;

	s32 last_drawn_frame_ntp_diff;
};

GF_ObjectManager *gf_odm_new();
void gf_odm_del(GF_ObjectManager *ODMan);

/*setup OD*/
void gf_odm_setup_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, GF_FilterPid *for_pid);

void gf_odm_setup_remote_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, char *remote_url);

/*disctonnect OD and removes it if desired (otherwise only STOP is propagated)*/
void gf_odm_disconnect(GF_ObjectManager *odman, u32 do_remove);
/*setup PID attached to this object*/
GF_Err gf_odm_setup_pid(GF_ObjectManager *odm, GF_FilterPid *pid);

//if register_only is false, calls gf_odm_setup_object on the given PID
void gf_odm_register_pid(GF_ObjectManager *odm, GF_FilterPid *pid, Bool register_only);

Bool gf_odm_check_buffering(GF_ObjectManager *odm, GF_FilterPid *pid);

/*signals end of stream on channels*/
void gf_odm_on_eos(GF_ObjectManager *odm, GF_FilterPid *pid);

/*set time shift buffer duration */
void gf_odm_set_timeshift_depth(GF_ObjectManager *odm, u32 time_shift_ms);

/*send PLAY request on associated PID*/
void gf_odm_start(GF_ObjectManager *odm);
/*stop OD streams*/
void gf_odm_stop(GF_ObjectManager *odm, Bool force_close);
/*send PLAY request to network - needed to properly handle multiplexed inputs
ONLY called by service handler (media manager thread)*/
void gf_odm_play(GF_ObjectManager *odm);

/*pause object (mediaControl use only)*/
void gf_odm_pause(GF_ObjectManager *odm);
/*resume object (mediaControl use only)*/
void gf_odm_resume(GF_ObjectManager *odm);
/*set object speed*/
void gf_odm_set_speed(GF_ObjectManager *odm, Fixed speed, Bool adjust_clock_speed);
/*returns the clock of the media stream (video, audio or bifs), NULL otherwise */
struct _object_clock *gf_odm_get_media_clock(GF_ObjectManager *odm);
/*adds segment descriptors targeted by the URL to the list and sort them - the input list must be empty*/
void gf_odm_init_segments(GF_ObjectManager *odm, GF_List *list, MFURL *url);
/*returns true if this OD depends on the given clock*/
Bool gf_odm_shares_clock(GF_ObjectManager *odm, struct _object_clock *ock);

GF_Segment *gf_odm_find_segment(GF_ObjectManager *odm, char *descName);

void gf_odm_stop_or_destroy(GF_ObjectManager *odm);

void gf_odm_signal_eos_reached(GF_ObjectManager *odm);

void gf_odm_reset_media_control(GF_ObjectManager *odm, Bool signal_reset);

void gf_odm_setup_task(GF_ObjectManager *odm);

/*GF_MediaObject: link between real object manager and scene. although there is a one-to-one mapping between a
MediaObject and an ObjectManager, we have to keep them separated in order to handle OD remove commands which destroy
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

	//current packet
	GF_FilterPacket *pck;

	u32 frame_dur;
	
	//number of bytes read in the current packet
	u32 RenderedLength;

	/*shared object info: if 0 a new frame will be checked, otherwise current is returned*/
	u32 nb_fetch;
	/*frame presentation time*/
	u32 timestamp;
	/*time in ms until next frame shall be presented*/
	s32 ms_until_next;
	s32 ms_until_pres;
	/*data frame size*/
	u32 size, framesize;
	/*pointer to data frame */
	char *frame;
	/* Objects implementing the DOM Event Target interface
	   used to dispatch HTML5 Media and Media Source Events */
	GF_List *evt_targets;
	/*pointer to the node responsible for the creation of this media object
	ONLY used for scene media type (animationStreams)
	Reset upon creation of the decoder.
	*/
	void *node_ptr;

	Bool is_eos;
	Bool config_changed;

	/*currently valid properties of the object*/
	u32 width, height, stride, pixel_ar, pixelformat;
	Bool is_flipped;
	u32 sample_rate, num_channels, bits_per_sample, channel_config, bytes_per_sec;
	u32 srd_x, srd_y, srd_w, srd_h, srd_full_w, srd_full_h;
	
	u32 quality_degradation_hint;
	u32 nb_views;
	u32 nb_layers;
	u32 view_min_x, view_max_x, view_min_y, view_max_y;
	GF_MediaDecoderFrame *media_frame;
};

GF_MediaObject *gf_mo_new();


/*media access events */
void gf_odm_service_media_event(GF_ObjectManager *odm, GF_EventType event_type);
void gf_odm_service_media_event_with_download(GF_ObjectManager *odm, GF_EventType event_type, u64 loaded_size, u64 total_size, u32 bytes_per_sec);

/*checks the URL and returns the ODID (MPEG-4 od://) or GF_MEDIA_EXTERNAL_ID for all regular URLs*/
u32 gf_mo_get_od_id(MFURL *url);

void gf_scene_generate_views(GF_Scene *scene, char *url, char *parent_url);
//sets pos and size of addon
//	size is 1/2 height (0), 1/3 (1) or 1/4 (2)
//	pos is bottom-left(0), top-left (1) bottom-right (2) or top-right (3)
void gf_scene_set_addon_layout_info(GF_Scene *scene, u32 position, u32 size_factor);


void gf_scene_message(GF_Scene *scene, const char *service, const char *message, GF_Err error);
void gf_scene_message_ex(GF_Scene *scene, const char *service, const char *message, GF_Err error, Bool no_filtering);

//returns media time in sec for the addon - timestamp_based is set to 1 if no timeline has been found (eg sync is based on direct timestamp comp)
Double gf_scene_adjust_time_for_addon(GF_AddonMedia *addon, Double clock_time, u8 *timestamp_based);
s64 gf_scene_adjust_timestamp_for_addon(GF_AddonMedia *addon, u64 orig_ts);
void gf_scene_select_scalable_addon(GF_Scene *scene, GF_ObjectManager *odm);
/*check if the associated addon has to be restarted, based on the timestamp of the main media (used for scalable addons only). Returns 1 if the addon has been restarted*/
Bool gf_scene_check_addon_restart(GF_AddonMedia *addon, u64 cts, u64 dts);

/*callbacks for scene graph library so that all related ESM nodes are properly instanciated*/
void gf_scene_node_callback(void *_is, u32 type, GF_Node *node, void *param);

void gf_scene_switch_quality(GF_Scene *scene, Bool up);

//exported for gpac.js, resumes to main content
void gf_scene_resume_live(GF_Scene *subscene);

enum
{
	//addon to overlay
	GF_ADDON_TYPE_ADDITIONAL = 0,
	//main content duplicated for PVR purposes
	GF_ADDON_TYPE_MAIN,
	//scalable decoding - reassembly before the decoder(s)
	GF_ADDON_TYPE_SCALABLE,
	//multiview reconstruction - reassembly after the decoder(s)
	GF_ADDON_TYPE_MULTIVIEW,
};

struct _gf_addon_media
{
	char *url;
	GF_ObjectManager *root_od;
	s32 timeline_id;
	u32 is_splicing;
	//in scene time
	Double activation_time;

	Bool enabled;
	//object(s) have been started (PLAY command sent) - used to filter out AUs in scalabmle addons
	Bool started;
	Bool timeline_ready;

	u32 media_timescale;
	u64 media_timestamp;
	u64 media_pts;

	//in case we detect a loop, we store the value of the mediatime in the loop until we actually loop the content
	u32 past_media_timescale;
	u64 past_media_timestamp;
	u64 past_media_pts, past_media_pts_scaled;
	Bool loop_detected;

	u32 addon_type;
};

void gf_scene_toggle_addons(GF_Scene *scene, Bool show_addons);


void gf_scene_set_service_id(GF_Scene *scene, u32 service_id);


/*post extended user mouse interaction to terminal
	X and Y are point coordinates in the display expressed in 2D coord system top-left (0,0), Y increasing towards bottom
	@xxx_but_down: specifiy whether the mouse button is down(2) or up (1), 0 if unchanged
	@wheel: specifiy current wheel inc (0: unchanged , +1 for one wheel delta forward, -1 for one wheel delta backward)
*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_sc_input_sensor_mouse_input(GF_Compositor *compositor, GF_EventMouse *event);

/*post extended user key interaction to terminal
	@key_code: GPAC DOM code of input key
	@hw_code: hardware code of input key
	@isKeyUp: set if key is released
*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
Bool gf_sc_input_sensor_keyboard_input(GF_Compositor *compositor, u32 key_code, u32 hw_code, Bool isKeyUp);

/*post extended user character interaction to terminal
	@character: unicode character input
*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_sc_input_sensor_string_input(GF_Compositor *compositor, u32 character);

GF_Err gf_input_sensor_setup_object(GF_ObjectManager *odm, GF_ESD *esd);
void gf_input_sensor_delete(GF_ObjectManager *odm);

void InitInputSensor(GF_Scene *scene, GF_Node *node);
void InputSensorModified(GF_Node *n);
void InitKeySensor(GF_Scene *scene, GF_Node *node);
void InitStringSensor(GF_Scene *scene, GF_Node *node);


GF_Err gf_odm_get_object_info(GF_ObjectManager *odm, GF_MediaInfo *info);


void gf_sc_connect(GF_Compositor *compositor, const char *URL);
void gf_sc_disconnect(GF_Compositor *compositor);

#ifdef __cplusplus
}
#endif


#endif	/*_GF_TERMINAL_DEV_H_*/


