/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Rendering sub-project
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

#ifndef _GF_RENDERER_DEV_H_
#define _GF_RENDERER_DEV_H_


/*include scene graph API*/
#include <gpac/renderer.h>
/*include scene graph API*/
#include <gpac/thread.h>
/*bridge between the rendering engine and the systems media engine*/
#include <gpac/mediaobject.h>

/*raster2D API*/
#include <gpac/modules/raster2d.h>
/*font engine API*/
#include <gpac/modules/font.h>
/*AV hardware API*/
#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>

/*SVG properties*/
#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif

#ifdef DANAE
void *getDanaeMediaOjbectFromUrl(void *session, char *url, int dmo_type);
void releaseDanaeMediaObject(void *dmo);
void loadDanaeUrl(void *session, char *url);
#endif

#define GF_SR_EVENT_QUEUE	

/*FPS computed on this number of frame*/
#define GF_SR_FPS_COMPUTE_SIZE	30

enum
{
	GF_SR_CFG_OVERRIDE_SIZE = 1,
	GF_SR_CFG_SET_SIZE = 1<<1,
	GF_SR_CFG_AR = 1<<2,
	GF_SR_CFG_FULLSCREEN = 1<<3,
	/*flag is set whenever we're reconfiguring visual. This will discard all UI
	messages during this phase in order to avoid any deadlocks*/
	GF_SR_IN_RECONFIG = 1<<4,
	/*special flag indicating the set size is actually due to a notif by the plugin*/
	GF_SR_CFG_WINDOWSIZE_NOTIF = 1<<10,
};

struct __tag_base_renderer
{
	/*the main user*/
	GF_User *user;
	/*the terminal running the graph - only used for InputSensor*/
	GF_Terminal *term;
#ifdef DANAE
	void *danae_session;
#endif

	/*audio renderer*/
	struct _audio_render *audio_renderer;
	/*renderer module (cf below)*/
	struct visual_render_module *visual_renderer;
	/*video out*/
	GF_VideoOutput *video_out;
	/*2D rasterizer - now also used by 3D renderer for text texturing & gradients generation*/
	GF_Raster2D *r2d;

	/*visual rendering thread if used*/
	GF_Thread *VisualThread;
	/*0: not init, 1: running, 2: exit requested, 3: done*/
	u32 video_th_state;
	GF_Mutex *mx;

	/*the main scene graph*/
	GF_SceneGraph *scene;
	/*extra scene graphs (OSD, etc), always registered in draw order. That's the module responsability
	to draw them*/
	GF_List *extra_scenes;
	
	/*all time nodes registered*/
	GF_List *time_nodes;
	/*all textures (texture handlers)*/
	GF_List *textures;

#ifdef GF_SR_EVENT_QUEUE
	/*event queue*/
	GF_List *events;
	GF_Mutex *ev_mx;
#endif

	/*simulation frame rate*/
	Double frame_rate;
	u32 frame_duration;
	u32 frame_time[GF_SR_FPS_COMPUTE_SIZE];
	u32 current_frame;

	u32 last_click_time;

	u32 scene_width, scene_height;
	Bool has_size_info;
	/*screen size*/
	u32 width, height;
	Bool fullscreen;
	/*!! paused will not stop display (this enables pausing a VRML world and still examining it)*/
	Bool paused, step_mode;
	Bool draw_next_frame;
	/*freeze_display prevents any screen updates - needed when output driver uses direct video memory access*/
	Bool is_hidden, freeze_display;

	u32 frame_number;
	/*count number of initialized sensors*/
	u32 interaction_sensors;

	/*set whenever 3D HW ctx changes (need to rebuild dlists/textures if any used)*/
	Bool reset_graphics;

	/*font engine*/
	GF_FontRaster *font_engine;

	/*options*/
	u32 aspect_ratio, antiAlias, texture_text_mode;
	Bool high_speed, stress_mode;

	/*key modif*/
	u32 key_states;
	u32 interaction_level;

	/*size override when no size info is present
		flags:	1: size override is requested (cfg)
				2: size override has been applied
	*/
	u32 override_size_flags;

	/*any of the above flags - reseted at each simulation tick*/
	u32 msg_type;
	/*for size*/
	u32 new_width, new_height;

	/*current background color*/
	u32 back_color;

	/*unit box (1.0 size) and unit sphere (1.0 radius)*/
	u32 draw_bvol;

	/*list of system colors*/
	u32 sys_colors[28];
};

/*macro setting up rendering stack with following member @owner: node pointer and @compositor: compositor*/
#define	gf_sr_traversable_setup(_object, _owner, _compositor)	\
		_object->owner = _owner;		\
		_object->compositor = _compositor; \


/*base stack for timed nodes (nodes that activate themselves at given times)
	@UpdateTimeNode: shall be setup by the node handler and is called once per simulation frame
	@is_registerd: all handlers indicate store their state if wanted (provided for conveniency but 
not inspected by the renderer)
	@needs_unregister: all handlers indicate they can be removed from the list of active time nodes
in order to save time. THIS IS INSPECTED by the renderer at each simulation tick after calling UpdateTimeNode
and if set, the node is removed right away from the list
*/
typedef struct _time_node
{
	void (*UpdateTimeNode)(struct _time_node *);
	Bool is_registered, needs_unregister;
	GF_Node *obj;
} GF_TimeNode;

void gf_sr_register_time_node(GF_Renderer *sr, GF_TimeNode *tn);
void gf_sr_unregister_time_node(GF_Renderer *sr, GF_TimeNode *tn);

/*texturing - Movietexture, ImageTexture and PixelTextures are handled by main renderer*/
typedef void *GF_HWTEXTURE;

enum
{
	/*texture repeat along s*/
	GF_SR_TEXTURE_REPEAT_S = (1<<0),
	/*texture repeat along t*/
	GF_SR_TEXTURE_REPEAT_T = (1<<1),
	/*texture is a matte texture*/
	GF_SR_TEXTURE_MATTE = (1<<2),
};

typedef struct _gf_sr_texture_handler
{
	GF_Node *owner;
	GF_Renderer *compositor;
	/*driver texture object*/
	GF_HWTEXTURE hwtx;
	/*media stream*/
	GF_MediaObject *stream;
	/*texture is open (for DEF/USE)*/
	Bool is_open;
	/*this is needed in case the Url is changed - note that media nodes cannot point to different
	URLs (when they could in VRML), the MF is only holding media segment descriptions*/
	MFURL current_url;
	/*to override by each texture node*/
	void (*update_texture_fcnt)(struct _gf_sr_texture_handler *txh);
	/*needs_release if a visual frame is grabbed (not used by modules)*/
	Bool needs_release;
	/*stream_finished: indicates stream is over (not used by modules)*/
	Bool stream_finished;
	/*needs_refresh: indicates texture content has been changed - needed by modules performing tile drawing*/
	Bool needs_refresh;
	/*needed to discard same frame fetch*/
	u32 last_frame_time;
	/*active display in the texture (0, 0 == top, left)*/
	//GF_Rect active_window;
	/*texture is transparent*/		
	Bool transparent;
	/*flags for user - the repeatS and repeatT are set upon creation, the rest is NEVER touched by renderer*/
	u32 flags;
	/*gradients are relative to the object bounds, therefore a gradient is not the same if used on 2 different
	objects - since we don't want to build an offscreen texture for the gradient, gradients have to be updated 
	at each draw - the matrix shall be updated to the gradient transformation in the local system
	MUST be set for gradient textures*/
	void (*compute_gradient_matrix)(struct _gf_sr_texture_handler *txh, GF_Rect *bounds, GF_Matrix2D *mat);

	/*image data for natural media*/
	char *data;
	u32 width, height, stride, pixelformat, pixel_ar;
	/*if set texture has been transformed by MatteTexture -> disable blit*/
	Bool has_cmat;

	/*matteTexture parent if any*/
	GF_Node *matteTexture;

#ifdef DANAE
	/* DANAE GF_MediaObject equivalent */
	void *dmo;
#endif

} GF_TextureHandler;

/*setup texturing object*/
void gf_sr_texture_setup(GF_TextureHandler *hdl, GF_Renderer *sr, GF_Node *owner);
/*destroy texturing object*/
void gf_sr_texture_destroy(GF_TextureHandler *txh);

/*return texture handle for built-in textures (movieTexture, ImageTexture and PixelTexture)*/
GF_TextureHandler *gf_sr_texture_get_handler(GF_Node *n);

/*these ones are needed by modules only for Background(2D) handling*/

/*returns 1 if url changed from current one*/
Bool gf_sr_texture_check_url_change(GF_TextureHandler *txh, MFURL *url);
/*starts associated object*/
GF_Err gf_sr_texture_play(GF_TextureHandler *txh, MFURL *url);
GF_Err gf_sr_texture_play_from_to(GF_TextureHandler *txh, MFURL *url, Double start_offset, Double end_offset, Bool can_loop, Bool lock_scene_timeline);
/*stops associated object*/
void gf_sr_texture_stop(GF_TextureHandler *txh);
/*restarts associated object - DO NOT CALL stop/start*/
void gf_sr_texture_restart(GF_TextureHandler *txh);
/*common routine for all video texture: fetches a frame and update the 2D texture object */
void gf_sr_texture_update_frame(GF_TextureHandler *txh, Bool disable_resync);
/*release video memory if needed*/
void gf_sr_texture_release_stream(GF_TextureHandler *txh);


/*
		Renderer Module: this is a very basic interface allowing both 2D and 3D renderers
		to be loaded at run-time
*/

/*interface name for video output*/
#define GF_RENDERER_INTERFACE	GF_4CC('G','R','E','N') 

typedef struct visual_render_module GF_VisualRenderer;

struct visual_render_module
{
	/* interface declaration*/
	GF_DECL_MODULE_INTERFACE

	/*load renderer - a pointer to the compositor is passed for later access to generic rendering stuff*/
	GF_Err (*LoadRenderer)(GF_VisualRenderer *vr, GF_Renderer *compositor);
	/*unloads renderer*/
	void (*UnloadRenderer)(GF_VisualRenderer *vr);

	/*the node private stack creation function. Note that some nodes are handled at a higher level thus will 
	never get to the renderer.*/
	void (*NodeInit)(GF_VisualRenderer *vr, GF_Node *node);
	/*signals the given node has been modified. If the module returns FALSE, the node will be marked as dirty
	and all its parents marked wityh DIRTY_CHILD flag*/
	Bool (*NodeChanged)(GF_VisualRenderer *vr, GF_Node *node);

	/*signals that output size has been changed so that the module may update output scene size (2D mainly)*/
	GF_Err (*RecomputeAR)(GF_VisualRenderer *vr);
	/*signals the scene graph has been deconnected: all vars related to the scene shall be reseted & memory cleanup done*/
	void (*SceneReset)(GF_VisualRenderer *vr);
	/*draw the scene
		1 - the first subtree to be rendered SHALL BE the main scene attached to compositor
		2 - there may be other graphs to draw (such as subtitles, etc,) not related to the main scene in extra_scenes list.
		3 - it is the module responsability to flush the video driver
		!!! The scene may be NULL, in which case the screen shall be cleared
	*/
	void (*DrawScene)(GF_VisualRenderer *vr);
	/*execute given event.
	for mouse events, x and y are in BIFS fashion (eg, from x in [-screen_width, screen_width] and y in [-screen_height, screen_height]) 
	return 1 if event matches a pick in the scene, 0 otherwise (this avoids performing shortcuts when user
	clicks on object...*/
	Bool (*ExecuteEvent)(GF_VisualRenderer *vr, GF_Event *event);
	/*signals the hw driver has been reseted to reload cfg*/	
	void (*GraphicsReset)(GF_VisualRenderer *vr);
	/*render inline scene*/
	void (*RenderInline)(GF_VisualRenderer *vr, GF_Node *inline_parent, GF_Node *inline_root, void *rs);
	/*get viewpoints/viewports for main scene - idx is 1-based, or 0 to retrieve by viewpoint name.
	if idx is greater than number of viewpoints return GF_EOS*/
	GF_Err (*GetViewpoint)(GF_VisualRenderer *vr, u32 viewpoint_idx, const char **outName, Bool *is_bound);
	/*set viewpoints/viewports for main scene given its name - idx is 1-based, or 0 to retrieve by viewpoint name
	if only one viewpoint is present in the scene, this will bind/unbind it*/
	GF_Err (*SetViewpoint)(GF_VisualRenderer *vr, u32 viewpoint_idx, const char *viewpoint_name);
	/*execut action as defined in gpac/scenegraph.h */
	Bool (*ScriptAction)(GF_VisualRenderer *vr, u32 type, GF_Node *n, GF_JSAPIParam *param);

	/*natural texture handling (image & video)*/

	/*allocates hw texture*/
	GF_Err (*AllocTexture)(GF_TextureHandler *hdl);
	/*releases hw texture*/
	void (*ReleaseTexture)(GF_TextureHandler *hdl);
	/*push texture data*/
	GF_Err (*SetTextureData)(GF_TextureHandler *hdl);
	/*signal HW reset in case the driver needs to reload texture*/
	void (*TextureHWReset)(GF_TextureHandler *hdl);

	/*set/get option*/
	GF_Err (*SetOption)(GF_VisualRenderer *vr, u32 option, u32 value);
	u32 (*GetOption)(GF_VisualRenderer *vr, u32 option);

	/*get/release video memory - READ ONLY*/
	GF_Err (*GetScreenBuffer)(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer);
	/*releases screen buffer and unlocks graph*/
	GF_Err (*ReleaseScreenBuffer)(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer);

	/*set to true if needs an OpenGL context - MUST be set when interface is created (before LoadRenderer)*/
	Bool bNeedsGL;

	/*user private*/
	void *user_priv;
};


/*
	Audio mixer - MAX 6 CHANNELS SUPPORTED
*/

/*the audio object as used by the mixer. All audio nodes need to implement this interface*/
typedef struct _audiointerface
{
	/*fetch audio data for a given audio delay (~soundcard drift) - if delay is 0 sync should not be performed 
	(eg intermediate mix) */
	char *(*FetchFrame) (void *callback, u32 *size, u32 audio_delay_ms);
	/*release a number of bytes in the indicated frame (ts)*/
	void (*ReleaseFrame) (void *callback, u32 nb_bytes);
	/*get media speed*/
	Fixed (*GetSpeed)(void *callback);
	/*gets volume for each channel - vol = Fixed[6]. returns 1 if volume shall be changed (!= 1.0)*/
	Bool (*GetChannelVolume)(void *callback, Fixed *vol);
	/*returns 1 if muted*/
	Bool (*IsMuted)(void *callback);
	/*user callback*/
	void *callback;
	/*returns 0 if config is not known yet or changed, 
	otherwise AND IF @for_reconf is set, updates member var below and return TRUE
	You may return 0 to force parent user invalidation*/
	Bool (*GetConfig)(struct _audiointerface *ai, Bool for_reconf);
	/*updated cfg, or 0 otherwise*/
	u32 chan, bps, sr, ch_cfg;
} GF_AudioInterface;

typedef struct __audiomix GF_AudioMixer;

/*create mixer - ar is NULL for any sub-mixers, or points to the main audio renderer (mixer outputs to sound driver)*/
GF_AudioMixer *gf_mixer_new(struct _audio_render *ar);
void gf_mixer_del(GF_AudioMixer *am);
void gf_mixer_remove_all(GF_AudioMixer *am);
void gf_mixer_add_input(GF_AudioMixer *am, GF_AudioInterface *src);
void gf_mixer_remove_input(GF_AudioMixer *am, GF_AudioInterface *src);
void gf_mixer_lock(GF_AudioMixer *am, Bool lockIt);
/*mix inputs in buffer, return number of bytes written to output*/
u32 gf_mixer_get_output(GF_AudioMixer *am, void *buffer, u32 buffer_size);
/*reconfig all sources if needed - returns TRUE if main audio config changed
NOTE: this is called at each gf_mixer_get_output by the mixer. To call externally for audio hardware
reconfiguration only*/
Bool gf_mixer_reconfig(GF_AudioMixer *am);
/*retrieves mixer cfg*/
void gf_mixer_get_config(GF_AudioMixer *am, u32 *outSR, u32 *outCH, u32 *outBPS, u32 *outChCfg);
/*called by audio renderer in case the hardware used a different setup than requested*/
void gf_mixer_set_config(GF_AudioMixer *am, u32 outSR, u32 outCH, u32 outBPS, u32 ch_cfg);
Bool gf_mixer_is_src_present(GF_AudioMixer *am, GF_AudioInterface *ifce);
u32 gf_mixer_get_src_count(GF_AudioMixer *am);
void gf_mixer_force_chanel_out(GF_AudioMixer *am, u32 num_channels);
u32 gf_mixer_get_block_align(GF_AudioMixer *am);
Bool gf_mixer_must_reconfig(GF_AudioMixer *am);


enum
{
	GF_SR_AUDIO_NO_RESYNC	= (1),
	GF_SR_AUDIO_NO_MULTI_CH = (1<<1),
};

/*the audio renderer*/
typedef struct _audio_render
{
	GF_AudioOutput *audio_out;
	u32 flags;

	/*startup time (the audio renderer is used when present as the system clock)*/
	u32 startTime;
	/*frozen time counter if set*/
	Bool Frozen;
	u32 FreezeTime;
	
	/*final output*/
	GF_AudioMixer *mixer;
	Bool need_reconfig;
	/*client*/
	GF_User *user;

	/*audio thread if output not self-threaded*/
	GF_Thread *th;
	/*thread state: 0: not intit, 1: running, 2: waiting for stop, 3: done*/
	u32 audio_th_state;

	u32 audio_delay, volume, pan;
} GF_AudioRenderer;

/*creates audio renderer*/
GF_AudioRenderer *gf_sr_ar_load(GF_User *user);
/*deletes audio renderer*/
void gf_sr_ar_del(GF_AudioRenderer *ar);
/*control audio renderer - CtrlType:
	0: pause
	1: resume
	2: clean HW buffer and play
*/
void gf_sr_ar_control(GF_AudioRenderer *ar, u32 CtrlType);
/*set volume and pan*/
void gf_sr_ar_set_volume(GF_AudioRenderer *ar, u32 Volume);
void gf_sr_ar_set_pan(GF_AudioRenderer *ar, u32 Balance);
/*set audio priority*/
void gf_sr_ar_set_priority(GF_AudioRenderer *ar, u32 priority);
/*gets time in msec - this is the only clock used by the whole ESM system - depends on the audio driver*/
u32 gf_sr_ar_get_clock(GF_AudioRenderer *ar);
/*reset all input nodes*/
void gf_sr_ar_reset(GF_AudioRenderer *ar);
/*add audio node*/
void gf_sr_ar_add_src(GF_AudioRenderer *ar, GF_AudioInterface *source);
/*remove audio node*/
void gf_sr_ar_remove_src(GF_AudioRenderer *ar, GF_AudioInterface *source);
/*reconfig audio hardware if needed*/
void gf_sr_ar_reconfig(GF_AudioRenderer *ar);
u32 gf_sr_ar_get_delay(GF_AudioRenderer *ar);

/*the sound node interface for intensity & spatialization*/
typedef struct _soundinterface
{
	/*gets volume for each channel - vol = Fixed[6]. returns 1 if volume shall be changed (!= 1.0)
	if NULL channels are always at full intensity*/
	Bool (*GetChannelVolume)(GF_Node *owner, Fixed *vol);
	/*get sound priority (0: min, 255: max) - used by mixer to determine*/
	u8 (*GetPriority) (GF_Node *owner);
	/*node owning the structure*/
	GF_Node *owner;
} GF_SoundInterface;

/*audio common to AudioClip and AudioSource*/
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GF_AudioInterface input_ifce;
	/*can be NULL if the audio node generates its output from other input*/
	GF_MediaObject *stream;
	/*object speed and intensity*/
	Fixed speed, intensity;
	Bool stream_finished;
	Bool need_release;
	MFURL url;
	Bool is_open, is_muted;
	Bool register_with_renderer, register_with_parent;

	GF_SoundInterface *snd;
} GF_AudioInput;
/*setup interface with audio renderer - overwrite any functions needed after setup EXCEPT callback object*/
void gf_sr_audio_setup(GF_AudioInput *ai, GF_Renderer *sr, GF_Node *node);
/*open audio object*/
GF_Err gf_sr_audio_open(GF_AudioInput *ai, MFURL *url, Double clipBegin, Double clipEnd);
/*closes audio object*/
void gf_sr_audio_stop(GF_AudioInput *ai);
/*restarts audio object (cf note in MediaObj)*/
void gf_sr_audio_restart(GF_AudioInput *ai);

Bool gf_sr_audio_check_url(GF_AudioInput *ai, MFURL *url);

/*base grouping audio node (nodes with several audio sources as children)*/
#define AUDIO_GROUP_NODE	\
	GF_AudioInput output;		\
	void (*add_source)(struct _audio_group *_this, GF_AudioInput *src);	\

typedef struct _audio_group
{
	AUDIO_GROUP_NODE
} GF_AudioGroup;


enum
{
	/*part of a switched-off subtree (needed for audio)*/
	GF_SR_TRAV_SWITCHED_OFF = (1<<1),
};

/*base class for the traversing context: this is needed so that audio renderer can work without knowledge of
the used graphics driver. All traversing contexts must derive from this one
rend_flag (needed for audio): one of the above*/

#ifdef GPAC_DISABLE_SVG

#define BASE_EFFECT_CLASS	\
	struct _audio_group *audio_parent;	\
	GF_SoundInterface *sound_holder;	\
	u32 trav_flags;	\

#else

#define BASE_EFFECT_CLASS	\
	struct _audio_group *audio_parent;	\
	GF_SoundInterface *sound_holder;	\
	u32 trav_flags;	\
	SVGPropertiesPointers *svg_props;	\
	u32 svg_flags; \

#endif


typedef struct 
{
	BASE_EFFECT_CLASS	
} GF_BaseEffect;


/*register audio node with parent audio renderer (mixer or main renderer)*/
void gf_sr_audio_register(GF_AudioInput *ai, GF_BaseEffect *eff);
void gf_sr_audio_unregister(GF_AudioInput *ai);


#ifndef GPAC_DISABLE_SVG
Bool gf_term_check_iri_change(GF_Terminal *term, MFURL *url, XMLRI *iri);
GF_Err gf_term_set_mfurl_from_uri(GF_Terminal *sr, MFURL *mfurl, XMLRI *iri);
#endif


#endif	/*_GF_RENDERER_DEV_H_*/

