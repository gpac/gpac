/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#ifndef _COMPOSITOR_DEV_H_
#define _COMPOSITOR_DEV_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/compositor.h>
/*include scene graph API*/
#include <gpac/thread.h>
/*bridge between the rendering engine and the systems media engine*/
#include <gpac/mediaobject.h>
#include <gpac/filters.h>

/*raster2D API*/
#include <gpac/evg.h>
/*font engine API*/
#include <gpac/modules/font.h>
/*AV hardware API*/
#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>

/*SVG properties*/
#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif

#include <gpac/mpeg4_odf.h>

#include <gpac/mediaobject.h>
#include <gpac/bifs.h>

typedef struct _gf_scene GF_Scene;
typedef struct _gf_addon_media GF_AddonMedia;
typedef struct _object_clock GF_Clock;

/*! Media object manager*/
typedef struct _od_manager GF_ObjectManager;

Bool gf_sc_send_event(GF_Compositor *compositor, GF_Event *evt);

/*if defined, events are queued before being processed, otherwise they are handled whenever triggered*/
//#define GF_SR_EVENT_QUEUE


/*use 2D caching for groups*/
//#define GF_SR_USE_VIDEO_CACHE

//#define GPAC_USE_TINYGL

/*depth-enabled version for autostereoscopic displays */
#define GF_SR_USE_DEPTH

/*FPS computed on this number of frame*/
#define GF_SR_FPS_COMPUTE_SIZE	60



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
	GF_SR_CFG_WINDOWSIZE_NOTIF = 1<<5,
	/*special flag indicating this is the initial resize, and video setup should be sent*/
	GF_SR_CFG_INITIAL_RESIZE = 1<<6,
};


enum
{
	GF_3D_STEREO_NONE = 0,
	GF_3D_STEREO_TOP,
	GF_3D_STEREO_SIDE,
	GF_3D_STEREO_HEADSET,

	GF_3D_STEREO_LAST_SINGLE_BUFFER = GF_3D_STEREO_HEADSET,

	/*all modes above GF_3D_STEREO_LAST_SINGLE_BUFFER require shaders and textures for view storage*/

	/*custom interleaving using GLSL shaders*/
	GF_3D_STEREO_CUSTOM,
	/*some built-in interleaving modes*/
	/*each pixel correspond to a different view*/
	GF_3D_STEREO_COLUMNS,
	GF_3D_STEREO_ROWS,
	/*special case of sub-pixel interleaving for 2 views*/
	GF_3D_STEREO_ANAGLYPH,
	/*SpatialView 19'' 5views interleaving*/
	GF_3D_STEREO_5VSP19,
	/*Alioscopy 8 views interleaving*/
	GF_3D_STEREO_8VALIO
};


/*forward definition of the visual manager*/
typedef struct _visual_manager GF_VisualManager;
typedef struct _draw_aspect_2d DrawAspect2D;
typedef struct _traversing_state GF_TraverseState;
typedef struct _gf_ft_mgr GF_FontManager;

#ifndef GPAC_DISABLE_3D
#include <gpac/internal/camera.h>
#include <gpac/internal/mesh.h>

typedef struct
{
	Bool multisample;
	Bool bgra_texture;
	Bool abgr_texture;
	Bool npot_texture;
	Bool rect_texture;
	Bool point_sprite;
	Bool vbo, pbo, fbo;
	Bool gles2_unpack;
	Bool has_shaders;
	Bool npot;
	s32 max_texture_size;
} GLCaps;

#endif

#define DOUBLECLICK_TIME_MS		250

enum
{
	TILE_DEBUG_NONE=0,
	TILE_DEBUG_PARTIAL,
	TILE_DEBUG_FULL
};

enum
{
	/*no text selection*/
	GF_SC_TSEL_NONE = 0,
	/*text selection in progress*/
	GF_SC_TSEL_ACTIVE,
	/*text selection frozen*/
	GF_SC_TSEL_FROZEN,
	/*text selection has just been released*/
	GF_SC_TSEL_RELEASED,
};

enum
{
	GF_SC_DRAW_NONE,
	GF_SC_DRAW_FRAME,
	GF_SC_DRAW_FLUSH,
};

enum
{
	GF_SC_DEPTH_GL_NONE=0,
	GF_SC_DEPTH_GL_POINTS,
	GF_SC_DEPTH_GL_STRIPS,
};


enum
{
	GF_SC_GLMODE_AUTO=0,
	GF_SC_GLMODE_OFF,
	GF_SC_GLMODE_HYBRID,
	GF_SC_GLMODE_ON
};


enum
{
	GF_SC_DRV_OFF=0,
	GF_SC_DRV_ON,
	GF_SC_DRV_AUTO,
};

struct __tag_compositor
{
	u32 init_flags;
	void *os_wnd;

	u32 drv;
	GF_Err last_error;

	//filter mode, we can be a source for our built-in URLs
	char *src;

	/*audio renderer*/
	struct _audio_render *audio_renderer;
	/*video out*/
	GF_VideoOutput *video_out;

	Bool softblt;

	Bool discard_input_events;
	u32 video_th_id;

	/*compositor exclusive access to the scene and display*/
	GF_Mutex *mx;

	//args have been updated
	Bool reload_config;

	//list of pids we need to monitor at each render pass. For now BIFS and OD only
	GF_List *systems_pids;

	/*list of modules containing hardcoded proto implementations*/
	GF_List *proto_modules;

	/*the main scene graph*/
	GF_SceneGraph *scene;
	/*extra scene graphs (OSD, etc), always registered in draw order. That's the module responsibility
	to draw them*/
	GF_List *extra_scenes;

	Bool inherit_type_3d;

	Bool force_late_frame_draw;
	/*all time nodes registered*/
	GF_List *time_nodes;
	/*all textures (texture handlers)*/
	GF_List *textures;
	Bool texture_inserted;

	/*all textures to be destroyed (needed for OpenGL context ...)*/
	GF_List *textures_gc;

	/*event queue*/
	GF_List *event_queue, *event_queue_back;
	GF_Mutex *evq_mx;

	Bool video_setup_failed;

	//dur config option
	Double dur;
	//simulation frame rate option
	GF_Fraction fps;
	//timescale option
	u32 timescale;
	//autofps option
	Bool autofps;
	//autofps option
	Bool vfr;
	Bool nojs;
	Bool forced_alpha;
	Bool noback;
	Bool clipframe;

	//frame duration in ms, used to match closest frame in input video streams
	u32 frame_duration;
	Bool frame_was_produced;
	Bool bench_mode;
	//0: no frame pending, 1: frame pending, needs clock increase, 2: frames are pending but one frame has been decoded, do not increase clock
	u32 force_bench_frame;
	//number of audio frames sent in call to send_frame
	u32 audio_frames_sent;

	u32 frame_time[GF_SR_FPS_COMPUTE_SIZE];
	u32 frame_dur[GF_SR_FPS_COMPUTE_SIZE];
	u32 current_frame;
	u32 last_frame_time, caret_next_draw_time;
	Bool show_caret;
	Bool text_edit_changed;
	//sampled value of audio clock used in bench mode only
	u32 scene_sampled_clock;
	u32 last_frame_ts;

	u32 last_click_time;
	s32 ms_until_next_frame;
	s32 frame_delay;
	Bool fullscreen_postponed;
	Bool sys_frames_pending;
	
	Bool amc, async;
	u32 asr, ach, alayout, afmt, asize, avol, apan, abuf;
	Double max_aspeed, max_vspeed;
	u32 buffer, rbuffer, mbuffer, ntpsync;
	
	u32 ogl, mode2d;

	s32 subtx, subty, subd, audd;
	u32 subfs;

	u64 hint_extra_scene_cts_plus_one;
	u32 hint_extra_scene_dur_plus_one;

	/*display size*/
	u32 display_width, display_height;
	GF_DisplayOrientationType disp_ori;

	/*visual output location on window (we may draw background color outside of it)
		vp_x & vp_y: horizontal & vertical offset of the drawing area in the video output
		vp_width & vp_height: width & height of the drawing area
			* in scalable mode, this is the display size
			* in not scalable mode, this is the final drawing area size (dst_w & dst_h of the blit)
	*/
	u32 vp_x, vp_y, vp_width, vp_height;
	/*backbuffer size - in scalable mode, matches display size, otherwise matches scene size*/
	u32 output_width, output_height;
	Bool out8b;
	u8 multiview_mode;
	/*scene size if any*/
	u32 scene_width, scene_height;
	Bool has_size_info;
	Bool fullscreen;
	/*!! paused will not stop display (this enables pausing a VRML world and still examining it)*/
	Bool paused, step_mode;
	u32 frame_draw_type;
	u32 force_next_frame_redraw;
	/*freeze_display prevents any screen updates - needed when output driver uses direct video memory access*/
	Bool is_hidden, freeze_display;
	Bool timed_nodes_valid;
	//player option, by default disabled. In player mode the video driver is always loaded
	//and no passthrough checks are done
	u32 player;
	//output pixel format option for passthrough mode, none by default
	u32 opfmt;
	//allocated framebuffer and size for passthrough mode
	u8 *framebuffer;
	u32 framebuffer_size, framebuffer_alloc;

	//passthrough texture object - only assigned by background2D
	struct _gf_sc_texture_handler *passthrough_txh;
	//passthrough packet - this is only created if the associated input packet doesn't use frame interface
	//the packet might be using the same buffer as the input data
	//otherwise, the resulting packet needs to be created from the framebuffer
	GF_FilterPacket *passthrough_pck;
	//data associated with the passthrough output - can be the same pointer as input packet or a clone
	u8 *passthrough_data;
	//timescale of the passthrough pid
	u32 passthrough_timescale;
	//pixel format of the passthrough pid at last emitted frame
	u32 passthrough_pfmt;
	//set if inplace processing is used:
	//- matching size and pixel format for input packet and output framebuffer
	//- inplace data processing (will skip background texture blit)
	Bool passthrough_inplace;
	//set to true if passthrough object is buffering, in whcih case scene clock has to be updated
	//once buffering is done
	Bool passthrough_check_buffer;

	//debug non-immediate mode ny erasing the parts that would have been drawn
	Bool debug_defer;

	Bool disable_composite_blit, disable_hardware_blit, rebuild_offscreen_textures;

	/*current frame number*/
	u32 frame_number;
	/*count number of initialized sensors*/
	u32 interaction_sensors;

	//in player mode, exit if set
	//in non player mode, check for eos
	u32 check_eos_state;
	u32 last_check_pass;
	Bool flush_audio;

	/*set whenever 3D HW ctx changes (need to rebuild dlists/textures if any used)*/
	u32 reset_graphics;

	/*font engine*/
	GF_FontManager *font_manager;
	/*set whenever a new font has been received*/
	Bool reset_fonts;
	s32 fonts_pending;

	/*options*/
	u32 aspect_ratio, aa, textxt;
	Bool fast, stress;
	Bool is_opengl;
	Bool autoconfig_opengl;
	u32 force_opengl_2d;

	//in this mode all 2D raster is done through and RGBA canvas except background IO and textures which are done by the GPU. The canvas is then flushed to GPU.
	//the mode supports defer and immediate rendering
	Bool hybrid_opengl;

	Bool fsize;
	Bool event_pending;

	/*key modif*/
	u32 key_states;
	u32 interaction_level;

	//set whenever a scene has Layer3D or CompositeTexture3D
	Bool needs_offscreen_gl;

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
	u32 back_color, bc, ckey;

	/*bounding box draw type: none, unit box/rect and sphere (3D only)*/
	u32 bvol;
	/*list of system colors*/
	u32 sys_colors[28];

	/*all visual managers created*/
	GF_List *visuals;
	/*all outlines cached*/
	GF_List *strike_bank;

	/*main visual manager - the one managing the primary video output*/
	GF_VisualManager *visual;
	/*set to false whenever a new scene is attached to compositor*/
	Bool root_visual_setup;

	/*indicates whether the aspect ratio shall be recomputed:
		1: AR changed
		2: AR changed and root visual type changed between 2D and 3D
	*/
	u32 recompute_ar;

	/*to copy!*/
	u32 nbviews, stereo, camlay;
	Bool rview, dbgpack;
	Fixed dispdist;
	char *mvshader;

	GF_PropVec2i osize, dpi;

	Bool zoom_changed;

	/*traversing context*/
	struct _traversing_state *traverse_state;

	/*current picked node if any*/
	GF_Node *grab_node;
	/*current picked node's parent use if any*/
	GF_Node *grab_use;
	/*current focus node if any*/
	GF_Node *focus_node;
	/*parent use node of the current focus node if any*/
	GF_Node *focus_used;
	/*current parent focus node if any - needed to navigate within PROTOs*/
	GF_List *focus_ancestors;
	GF_List *focus_use_stack;
	/*focus node uses dom events*/
	Bool focus_uses_dom_events;
	/*current sensor type*/
	u32 sensor_type;
	/*list of VRML sensors active before the picking phase (eg active at the previous pass)*/
	GF_List *previous_sensors;
	/*list of VRML sensors active after the picking phase*/
	GF_List *sensors;
	/*indicates a sensor is currently active*/
	u32 grabbed_sensor;

	/*current keynav node if any*/
	GF_Node *keynav_node;

	/*current keynav node if any*/
	GF_List *env_tests;

	/*hardware handle for 2D screen access - currently only used with win32 (HDC) */
	void *hw_context;
	/*indicates whether HW is locked*/
	Bool hw_locked;
	/*screen buffer for direct access*/
	GF_VideoSurface hw_surface;
	/*output buffer is configured in video memory*/
	u32  video_memory;
	Bool request_video_memory, was_system_memory;
	/*indicate if overlays were prezsent in the previous frame*/
	Bool last_had_overlays;

	/*options*/
	Bool sz;

	Bool yuvhw;
	/*disables partial hardware blit (eg during dirty rect) to avoid artefacts*/
	Bool blitp;

	/*user navigation mode*/
	u32 navigate_mode;
	/*set if content doesn't allow navigation*/
	Bool navigation_disabled;

	u32 rotate_mode;

	/*user mouse navigation state:
	 0: not active
	 1: pre-active phase: mouse has been clicked and waiting for mouse move to confirm. This allows
		for clicking on objects in the navigation mode
	 2: navigation is grabbed
	*/
	u32 navigation_state;
	/*navigation x & y grab point in scene coord system*/
	Fixed grab_x, grab_y;
	/*aspect ratio scale factor*/
	Fixed scale_x, scale_y;
	/*user zoom level*/
	Fixed zoom;
	/*user pan*/
	Fixed trans_x, trans_y;
	/*user rotation angle - ALWAYS CENTERED*/
	Fixed rotation;

	u32 auto_rotate;

	/*0: flush to be done - 1: flush can be skipped - 2: forces flush*/
	u32 skip_flush;
	Bool flush_pending;

#ifndef GPAC_DISABLE_SVG
	u32 num_clicks;
#endif

	/*a dedicated drawable for focus highlight */
	struct _drawable *focus_highlight;
	/*highlight fill and stroke colors (ARGB)*/
	u32 hlfill, hlline;
	Fixed hllinew;
	Bool disable_focus_highlight;

	/*picking info*/

	/*picked node*/
	GF_Node *hit_node;
	/*appearance at hit point - used for composite texture*/
	GF_Node *hit_appear, *prev_hit_appear;
	/*parent use stack - SVG only*/
	GF_List *hit_use_stack, *prev_hit_use_stack;
	/*picked node uses DOM event or VRML events ?*/
	Bool hit_use_dom_events;

	/*world->local and local->world transform at hit point*/
	GF_Matrix hit_world_to_local, hit_local_to_world;
	/*hit point in local coord & world coord*/
	SFVec3f hit_local_point, hit_world_point;
	/*tex coords at hit point*/
	SFVec2f hit_texcoords;
	/*picking ray in world coord system*/
	GF_Ray hit_world_ray;
	/*normal at hit point, local coord system*/
	SFVec3f hit_normal;
	/*distance from ray origin used to discards further hits - FIXME: may not properly work with transparent layer3D*/
	Fixed hit_square_dist;

	/*text selection and edition*/

	/*the active parent text node under selection*/
	GF_Node *text_selection;
	/*text selection start/end in world coord system*/
	SFVec2f start_sel, end_sel;
	/*text selection state*/
	u32 store_text_state;
	/*parent text node when a text is hit (to handle tspan selection)*/
	GF_Node *hit_text;
	u32 sel_buffer_len, sel_buffer_alloc;
	u16 *sel_buffer;
	u8 *selected_text;
	/*text selection color - reverse video not yet supported*/
	u32 text_sel_color;
	s32 picked_glyph_idx, picked_span_idx;

	/*set whenever the focus node is a text node*/
	u32 focus_text_type;
	Bool edit_is_tspan;
	/*pointer to edited text*/
	char **edited_text;
	u32 caret_pos, dom_text_pos;

#ifndef GPAC_DISABLE_3D
	/*options*/
	/*emulate power-of-2 for video texturing by using a po2 texture and texture scaling. If any textureTransform
	is applied to this texture, black stripes will appear on the borders.
	If not set video is written through glDrawPixels with bitmap (slow scaling), or converted to
	po2 texture*/
	Bool epow2;
	/*use OpenGL for outline rather than vectorial ones*/
	Bool linegl;
	/*disable RECT extensions (except for Bitmap...)*/
	Bool rext;
	/*disable RECT extensions (except for Bitmap...)*/
	u32 norms;
	/*backface cull type: 0 off, 1: on, 2: on with transparency*/
	u32 bcull;
	/*polygon atialiasing*/
	Bool paa;
	/*wireframe/solid mode*/
	u32 wire;
	/*collision detection mode*/
	u32 collide_mode;
	/*gravity enabled*/
	Bool gravity_on;
	/*AABB tree-based culling is disabled*/
	Bool cull;
	//use PBO to start pushing textures at the beginning of the render pass
	Bool pbo;

	u32 nav;

	/*unit box (1.0 size) and unit sphere (1.0 radius)*/
	GF_Mesh *unit_bbox;

	/*active layer3D for layer navigation - may be NULL*/
	GF_Node *active_layer;

	GLCaps gl_caps;

	u32 offscreen_width, offscreen_height;

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	u32 shader_mode_disabled;
#endif
	char *vertshader, *fragshader;

	//force video frame packing (0=no packing or GF_3D_STEREO_SIDE or GF_3D_STEREO_TOP)
	u32 fpack;

#ifdef GPAC_USE_TINYGL
	void *tgl_ctx;
#endif

	Fixed depth_gl_scale, depth_gl_strips_filter;
	u32 depth_gl_type;
	Fixed iod;
	/*increase/decrease the standard interoccular offset by the specified distance in cm*/
	Fixed interoccular_offset;
	/*specifies distance the camera focal point and the screen plane : <0 is behind the screen, >0 is in front*/
	Fixed focdist;

	struct _gf_sc_texture_handler *hybgl_txh;
	GF_Mesh *hybgl_mesh;
	GF_Mesh *hybgl_mesh_background;

	Bool force_type_3d;
	u8 *screen_buffer, *line_buffer;
	u32 screen_buffer_alloc_size;

	u32 tvtn, tvtt, tvtd;
	Bool tvtf;
	u32 vrhud_mode;
	Fixed fov;

	//offscreen rendering
	u32 fbo_tx_id, fbo_id, fbo_depth_id;
	Bool external_tx_id;

#endif

	Bool orientation_sensors_active;
	
	Bool texture_from_decoder_memory;

	u32 networks_time;
	u32 decoders_time;

	u32 visual_config_time;
	u32 traverse_setup_time;
	u32 traverse_and_direct_draw_time;
	u32 indirect_draw_time;


#ifdef GF_SR_USE_VIDEO_CACHE
	/*video cache size / max size in kbytes*/
	u32 video_cache_current_size, vcsize;
	u32 vcscale, vctol;
	/*sorted list (by cache priority) of cached groups - permanent for the lifetime of the scene/cache object*/
	GF_List *cached_groups;
	/*list of groups being cached in one frame */
	GF_List *cached_groups_queue;
#endif

#ifdef GF_SR_USE_DEPTH
	Bool autocal;
	/*display depth in pixels - if -1, it is the height of the display area*/
	s32 dispdepth;
#endif

	Bool gazer_enabled, sgaze;
	s32 gaze_x, gaze_y;
	Bool gaze_changed;

	Bool validator_mode;

	struct _gf_scene *root_scene;
	Bool drop;
	Bool sclock;
	u32 timeout;
	u32 play_state;
	Bool use_step_mode;
	Bool reload_scene_size;
	//associated filter, used to load input filters
	GF_Filter *filter;
//	GF_FilterSession *fsess;
	GF_FilterPid *vout;
	GF_FilterFrameInterface frame_ifce;
	GF_VideoSurface fb;

	Bool dbgpvr;
	Bool noaudio;

	/*all X3D key/mouse/string sensors*/
	GF_List *x3d_sensors;
	/*all input stream decoders*/
	GF_List *input_streams;


	/*special list used by nodes needing a call to RenderNode but not in the traverese scene graph
	 (VRML/MPEG-4 protos only). 
	 For such nodes the traverse state will be NULL
	 This is only used by InputSensor node at the moment
	 */
	GF_List *nodes_pending;
	GF_List *extensions, *unthreaded_extensions;

	u32 reload_state;
	char *reload_url;

};

typedef struct
{
	GF_Event evt;
	GF_DOM_Event dom_evt;
	GF_Node *node;
	GF_DOMEventTarget *target;
	GF_SceneGraph *sg;
} GF_QueuedEvent;

void gf_sc_queue_dom_event(GF_Compositor *compositor, GF_Node *node, GF_DOM_Event *evt);
void gf_sc_queue_dom_event_on_target(GF_Compositor *compositor, GF_DOM_Event *evt, GF_DOMEventTarget *target, GF_SceneGraph *sg);

/*base stack for timed nodes (nodes that activate themselves at given times)
	@UpdateTimeNode: shall be setup by the node handler and is called once per simulation frame
	@is_registerd: all handlers indicate store their state if wanted (provided for conveniency but not inspected by the compositor)
	@needs_unregister: all handlers indicate they can be removed from the list of active time nodes
in order to save time. THIS IS INSPECTED by the compositor at each simulation tick after calling UpdateTimeNode
and if set, the node is removed right away from the list
*/
typedef struct _time_node
{
	void (*UpdateTimeNode)(struct _time_node *);
	Bool is_registered, needs_unregister;
	/*user data*/
	void *udta;
} GF_TimeNode;

void gf_sc_register_time_node(GF_Compositor *sr, GF_TimeNode *tn);
void gf_sc_unregister_time_node(GF_Compositor *sr, GF_TimeNode *tn);

enum
{
	/*texture repeat along s*/
	GF_SR_TEXTURE_REPEAT_S = (1<<0),
	/*texture repeat along t*/
	GF_SR_TEXTURE_REPEAT_T = (1<<1),
	/*texture is a matte texture*/
	GF_SR_TEXTURE_MATTE = (1<<2),
	/*texture doesn't need vertical flip for OpenGL*/
	GF_SR_TEXTURE_NO_GL_FLIP = (1<<3),
	/*Set durin a composition cycle. If not set at the end of the cycle,
	the hardware binding is released*/
	GF_SR_TEXTURE_USED = (1<<4),

	/*texture is SVG (needs special treatment in OpenGL)*/
	GF_SR_TEXTURE_SVG = (1<<5),

	/*special flag indicating the underlying media directly handled by the hardware (decoding and composition)*/
	GF_SR_TEXTURE_PRIVATE_MEDIA = (1<<6),

	/*texture blit is disabled, must go through rasterizer*/
	GF_SR_TEXTURE_DISABLE_BLIT = (1<<7),
};

typedef struct _gf_sc_texture_handler
{
	GF_Node *owner;
	GF_Compositor *compositor;
	/*low-level texture object for internal rasterizer and OpenGL - this is not exposed out of libgpac*/
	struct __texture_wrapper *tx_io;
	/*media stream*/
	GF_MediaObject *stream;
	/*texture is open (for DEF/USE)*/
	Bool is_open;
	/*this is needed in case the Url is changed*/
//	MFURL current_url;
	/*to override by each texture node*/
	void (*update_texture_fcnt)(struct _gf_sc_texture_handler *txh);
	/*needs_release if a visual frame is grabbed (not used by modules)*/
	u32 needs_release;
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
	/*flags for user - the repeatS and repeatT are set upon creation, the rest is NEVER touched by compositor*/
	u32 flags;
	/*gradients are relative to the object bounds, therefore a gradient is not the same if used on 2 different
	objects - since we don't want to build an offscreen texture for the gradient, gradients have to be updated
	at each draw - the matrix shall be updated to the gradient transformation in the local system
	MUST be set for gradient textures*/
	void (*compute_gradient_matrix)(struct _gf_sc_texture_handler *txh, GF_Rect *bounds, GF_Matrix2D *mat, Bool for_3d);

	/*image data for natural media*/
	u8 *data;
	//we need a local copy of width/height/etc since some textures may be defined without a stream object
	u32 size, width, height, pixelformat, pixel_ar, stride, stride_chroma, nb_planes;
	Bool is_flipped;

	GF_FilterFrameInterface *frame_ifce;
	u32 nb_frames, upload_time;

#ifndef GPAC_DISABLE_VRML
	/*if set texture has been transformed by MatteTexture -> disable blit*/
	Bool has_cmat;

	/*matteTexture parent if any*/
	GF_Node *matteTexture;
#endif

	u32 probe_time_ms;
	/*user data for video output module, if needed*/
	void *vout_udta;
} GF_TextureHandler;

/*setup texturing object*/
void gf_sc_texture_setup(GF_TextureHandler *hdl, GF_Compositor *sr, GF_Node *owner);
/*destroy texturing object*/
void gf_sc_texture_destroy(GF_TextureHandler *txh);

/*return texture handle for built-in textures (movieTexture, ImageTexture and PixelTexture)*/
GF_TextureHandler *gf_sc_texture_get_handler(GF_Node *n);

/*these ones are needed by modules only for Background(2D) handling*/

/*returns 1 if url changed from current one*/
Bool gf_sc_texture_check_url_change(GF_TextureHandler *txh, MFURL *url);
/* opens associated object */
GF_Err gf_sc_texture_open(GF_TextureHandler *txh, MFURL *url, Bool lock_scene_timeline);
/*starts associated object*/
GF_Err gf_sc_texture_play(GF_TextureHandler *txh, MFURL *url);
GF_Err gf_sc_texture_play_from_to(GF_TextureHandler *txh, MFURL *url, Double start_offset, Double end_offset, Bool can_loop, Bool lock_scene_timeline);
/*stops associated object*/
void gf_sc_texture_stop_no_unregister(GF_TextureHandler *txh);
/*stops associated object and unregister it*/
void gf_sc_texture_stop(GF_TextureHandler *txh);

/*restarts associated object - DO NOT CALL stop/start*/
void gf_sc_texture_restart(GF_TextureHandler *txh);
/*common routine for all video texture: fetches a frame and update the 2D texture object */
void gf_sc_texture_update_frame(GF_TextureHandler *txh, Bool disable_resync);
/*release video memory if needed*/
void gf_sc_texture_release_stream(GF_TextureHandler *txh);

void gf_sc_texture_cleanup_hw(GF_Compositor *compositor);


/*sensor node handler - this is not defined as a stack because Anchor is both a grouping node and a
sensor node, and we DO need the groupingnode stack...*/
typedef struct _sensor_handler
{
	/*sensor enabled or not ?*/
	Bool (*IsEnabled)(GF_Node *node);
	/*user input on sensor:
	is_over: pointing device is over a shape the sensor is attached to
	is_cancel: the sensor state has been canceled due to another sensor. This typically happens following "click" events in SVG
				which do not consume the mousedown but consumes the mouseup
	evt_type: mouse event type
	compositor: pointer to compositor - hit info is stored at compositor level
	return: was the event consumed ?
	*/
	Bool (*OnUserEvent)(struct _sensor_handler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor);
	Bool grabbed;
	/*pointer to the sensor node*/
	GF_Node *sensor;
} GF_SensorHandler;

/*returns TRUE if the node is a pointing device sensor node that can be stacked during traversing (all sensor except anchor)*/
Bool compositor_mpeg4_is_sensor_node(GF_Node *node);
/*returns associated sensor handler from traversable stack (the node handler is always responsible for creation/deletion)
returns NULL if not a sensor or sensor is not activated*/
GF_SensorHandler *compositor_mpeg4_get_sensor_handler(GF_Node *n);
GF_SensorHandler *compositor_mpeg4_get_sensor_handler_ex(GF_Node *n, Bool skip_anchors);

/*rendering modes*/
enum
{
	/*regular traversing mode for z-sorting:
		- 2D mode: builds the display list (may draw directly if requested)
		- 3D mode: sort & queue transparent objects
	*/
	TRAVERSE_SORT = 0,
	/*explicit draw routine used when flushing 2D display list*/
	TRAVERSE_DRAW_2D,
	/*pick routine*/
	TRAVERSE_PICK,
	/*get bounds routine: returns bounds in local coord system (including node transform if any)*/
	TRAVERSE_GET_BOUNDS,
	/*set to signal bindable render - only called on bindable stack top if present.
	for background (drawing request), viewports/viewpoints fog and navigation (setup)
	all other nodes SHALL NOT RESPOND TO THIS CALL
	*/
	TRAVERSE_BINDABLE,

	/*writes the text selection into the compositor buffer - we need a traversing mode for this operation
	to handle properly text and tspans*/
	TRAVERSE_GET_TEXT,

#ifndef GPAC_DISABLE_3D
	/*explicit draw routine used when flushing 3D display list*/
	TRAVERSE_DRAW_3D,
	/*set global lights on. Since the model_matrix is not pushed to the target in this
	pass, global lights shall not forget to do it (cf lighting.c)*/
	TRAVERSE_LIGHTING,
	/*collision routine*/
	TRAVERSE_COLLIDE,
#endif
};


typedef struct _group_cache_candidate GF_CacheCandidate;



#define MAX_USER_CLIP_PLANES		4


/*the traversing context: set_up at top-level and passed through SFNode_Render. Each node is responsible for
restoring the context state before returning*/
struct _traversing_state
{
	struct _audio_group *audio_parent;
	struct _soundinterface *sound_holder;

#ifndef GPAC_DISABLE_SVG
	SVGPropertiesPointers *svg_props;
	u32 svg_flags;
#endif

	/*current traversing mode*/
	u32 traversing_mode;
	/*for 2D drawing, indicates objects are to be drawn as soon as traversed, at each frame*/
	Bool immediate_draw;
	//flag set when immediate_draw whn in defer mode, so that canvas is not erased in hybgl mode
	Bool immediate_for_defer;
	
	
	/*current subtree is part of a switched-off subtree (needed for audio)*/
	Bool switched_off;
	/*set by the traversed subtree to indicate no cull shall be performed*/
	Bool disable_cull;

	/*indicates if we are in a layer or not*/
	Bool is_layer;
	/*current graph traversed is in pixel metrics*/
	Bool pixel_metrics;
	/*minimal half-dimension (w/2, h/2)*/
	Fixed min_hsize;

	/*indicates if the current subtree is fliped compared to the target visual*/
	Bool fliped_coords;

	/*current size of viewport being traverse (root scene, layers)*/
	SFVec2f vp_size;

	/*the one and only visual manager currently being traversed*/
	GF_VisualManager *visual;

#ifndef GPAC_DISABLE_VRML
	/*current background and viewport stacks*/
	GF_List *backgrounds;
	GF_List *viewpoints;
#endif

	/*disable partial sphere rendrering in VR*/
	Bool disable_partial_sphere;

	Bool reverse_backface;

	/*current transformation from top-level*/
	GF_Matrix2D transform;
	/*current color transformation from top-level*/
	GF_ColorMatrix color_mat;
	/* Contains the viewbox transform, used for svg ref() transform */
	GF_Matrix2D vb_transform;

	/*only used for bitmap drawing*/
	GF_ColorKey *col_key;

	/*if set all nodes shall be redrawn - set only at specific places in the tree*/
	Bool invalidate_all;

	/*text splitting: 0: no splitting, 1: word by word, 2:letter by letter*/
	u32 text_split_mode;
	/*1-based idx of text element drawn*/
	u32 text_split_idx;

	/*all VRML sensors for the current level*/
	GF_List *vrml_sensors;

	/*current appearance when traversing geometry nodes*/
	GF_Node *appear;
	/*parent group for composition: can be Form, Layout or Layer2D*/
	struct _parent_node_2d *parent;

	/*override appearance of all nodes with this one*/
	GF_Node *override_appearance;

	/*group/object bounds in local coordinate system*/
	GF_Rect bounds;

	/*node for which bounds should be fetched - SVG only*/
	GF_Node *for_node;
	Bool abort_bounds_traverse;
	GF_Matrix2D mx_at_node;
	Bool ignore_strike;

	GF_List *use_stack;

	/* Styling Property and others for SVG context */
#ifndef GPAC_DISABLE_SVG
	SVG_Number *parent_use_opacity;
	SVGAllAttributes *parent_anim_atts;
	Bool parent_is_use;

	/*SVG text rendering state*/
	Bool in_svg_text;
	Bool in_svg_text_area;

	/* current chunk & position of last placed text chunk*/
	u32 chunk_index;
	Fixed text_end_x, text_end_y;

	/* text & tspan state*/
	GF_List *x_anchors;
	SVG_Coordinates *text_x, *text_y, *text_rotate;
	u32 count_x, count_y, count_rotate, idx_rotate;

	/* textArea state*/
	Fixed max_length, max_height;
	Fixed base_x, base_y;
	Fixed line_spacing;
	Fixed base_shift;
	/*quick and dirty hack to try to solve xml:space across text and tspans without
	flattening the DOMText nodes
	0: first block of text
	1: previous block of text ended with a space
	2: previous block of text did NOT end with a space
	*/
	u32 last_char_type;
	/*in textArea, indicates that the children bounds must be refreshed due to a baseline adjustment*/
	u32 refresh_children_bounds;
#endif
	GF_Node *text_parent;

	/*current context to be drawn - only set when drawing in 2D mode or 3D for SVG*/
	struct _drawable_context *ctx;

	/*world ray for picking - in 2D, orig is 2D mouse pos and direction is -z*/
	GF_Ray ray;
	s32 pick_x, pick_y;

	/*we have 2 clippers, one for regular clipping (layout, form if clipping) which is maintained in world coord system
	and one for layer2D which is maintained in parent coord system (cf layer rendering). The layer clipper
	is used only when cascading layers - layer3D doesn't use clippers*/
	Bool has_clip, has_layer_clip;
	/*active clipper in world coord system */
	GF_Rect clipper, layer_clipper;
	/*current object (model) transformation at the given layer*/
	GF_Matrix layer_matrix;


	/*set when traversing a cached group during offscreen bitmap construction.*/
	Bool in_group_cache;

	Bool in_svg_filter;

	u32 subscene_not_over;

#ifndef GPAC_DISABLE_3D
	/*the current camera*/
	GF_Camera *camera;

	/*current object (model) transformation from top-level, view is NOT included*/
	GF_Matrix model_matrix;

#ifndef GPAC_DISABLE_VRML
	/*fog bind stack*/
	GF_List *fogs;
	/*navigation bind stack*/
	GF_List *navigations;
#endif

	/*when drawing, signals the mesh is transparent (enables blending)*/
	Bool mesh_is_transparent;
	/*when drawing, signals the number of textures used by the mesh*/
	u32 mesh_num_textures;

	/*bounds for TRAVERSE_GET_BOUNDS and background rendering*/
	GF_BBox bbox;

	/*cull flag (used to skip culling of children when parent bbox is completely inside/outside frustum)*/
	u32 cull_flag;

	/*toggle local lights on/off - field is ONLY valid in TRAVERSE_RENDER mode, and local lights
	are always set off in reverse order as when set on*/
	Bool local_light_on;
	/*current directional ligths contexts - only valid in TRAVERSE_RENDER*/
	GF_List *local_lights;

	/*clip planes in world coords*/
	GF_Plane clip_planes[MAX_USER_CLIP_PLANES];
	u32 num_clip_planes;

	Bool camera_was_dirty;

	/*layer traversal state:
		set to the first traversed layer3D when picking
		set to the current layer3D traversed when rendering 3D to an offscreen bitmap. This allows other
			nodes (typically bindables) seting the layer dirty flags to force a redraw
	*/
	GF_Node *layer3d;
#endif


#ifdef GF_SR_USE_DEPTH
	Fixed depth_gain, depth_offset;
#endif


#ifdef GF_SR_USE_VIDEO_CACHE
	/*set to 1 if cache evaluation can be skipped - this is only set when there is not enough memory
	to cache a sub-group, in which case the group cannot be cached (we're caching in display coordinates)*/
	Bool cache_too_small;
#endif
};

/*
	Audio mixer - MAX GF_AUDIO_MIXER_MAX_CHANNELS CHANNELS SUPPORTED
*/

/*max number of channels we support in mixer*/
#define GF_AUDIO_MIXER_MAX_CHANNELS	24

/*the audio object as used by the mixer. All audio nodes need to implement this interface*/
typedef struct _audiointerface
{
	/*fetch audio data for a given audio delay (~soundcard drift) - if delay is 0 sync should not be performed
	(eg intermediate mix) */
	u8 *(*FetchFrame) (void *callback, u32 *size, u32 *planar_stride, u32 audio_delay_ms);
	/*release a number of bytes in the indicated frame (ts)*/
	void (*ReleaseFrame) (void *callback, u32 nb_bytes);
	/*get media speed*/
	Fixed (*GetSpeed)(void *callback);
	/*gets volume for each channel - vol = Fixed[GF_AUDIO_MIXER_MAX_CHANNELS]. returns 1 if volume shall be changed (!= 1.0)*/
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
	u32 chan, afmt, samplerate;
	u64 ch_layout;
	Bool forced_layout;
	//updated at each frame, used if frame fetch returns NULL
	Bool is_buffering;
	Bool is_eos;
} GF_AudioInterface;

typedef struct __audiomix GF_AudioMixer;

/*create mixer - ar is NULL for any sub-mixers, or points to the main audio renderer (mixer outputs to sound driver)*/
GF_AudioMixer *gf_mixer_new(struct _audio_render *ar);
void gf_mixer_del(GF_AudioMixer *am);
void gf_mixer_remove_all(GF_AudioMixer *am);
void gf_mixer_add_input(GF_AudioMixer *am, GF_AudioInterface *src);
void gf_mixer_remove_input(GF_AudioMixer *am, GF_AudioInterface *src);
void gf_mixer_lock(GF_AudioMixer *am, Bool lockIt);
void gf_mixer_set_max_speed(GF_AudioMixer *am, Double max_speed);

/*mix inputs in buffer, return number of bytes written to output*/
u32 gf_mixer_get_output(GF_AudioMixer *am, void *buffer, u32 buffer_size, u32 delay_ms);
/*reconfig all sources if needed - returns TRUE if main audio config changed
NOTE: this is called at each gf_mixer_get_output by the mixer. To call externally for audio hardware
reconfiguration only*/
Bool gf_mixer_reconfig(GF_AudioMixer *am);
/*retrieves mixer cfg*/
void gf_mixer_get_config(GF_AudioMixer *am, u32 *outSR, u32 *outCH, u32 *outFMT, u64 *outChCfg);
/*called by audio renderer in case the hardware used a different setup than requested*/
GF_Err gf_mixer_set_config(GF_AudioMixer *am, u32 outSR, u32 outCH, u32 outFMT, u64 ch_cfg);
Bool gf_mixer_is_src_present(GF_AudioMixer *am, GF_AudioInterface *ifce);
u32 gf_mixer_get_src_count(GF_AudioMixer *am);
GF_Err gf_mixer_force_channel_out(GF_AudioMixer *am, u32 num_channels);
u32 gf_mixer_get_block_align(GF_AudioMixer *am);
Bool gf_mixer_must_reconfig(GF_AudioMixer *am);
Bool gf_mixer_empty(GF_AudioMixer *am);
Bool gf_mixer_buffering(GF_AudioMixer *am);
Bool gf_mixer_is_eos(GF_AudioMixer *am);

//#define ENABLE_AOUT

/*the audio renderer*/
typedef struct _audio_render
{
	GF_Compositor *compositor;
	
	u32 max_bytes_out, samplerate, bytes_per_samp, nb_bytes_out, buffer_size, nb_buffers;
	u64 current_time_sr, time_at_last_config_sr;
	GF_FilterPid *aout;
	u32 video_ts;
	Bool scene_ready;
	u32 nb_audio_objects;

	/*startup time, used when no audio output is set*/
	u64 start_time;
	/*freeze time, used when no audio output is set*/
	u64 freeze_time;

/*	Bool disable_resync;
	Bool disable_multichannel;
*/
	/*frozen time counter if set*/
	Bool Frozen;

	/*system clock compute when audio output is present*/
	u32 current_time, bytes_per_second, time_at_last_config;
	//number of bytes requested by sound card since last reconfig
	u64 bytes_requested;

	/*final output*/
	GF_AudioMixer *mixer;
	Bool need_reconfig;

	u32 config_forced, wait_for_rcfg;

	u32 audio_delay, volume, pan, mute;

	//set when output is not realtime - set to 2 will indicate end of session
	u32 non_rt_output;

	Fixed yaw, pitch, roll;

} GF_AudioRenderer;

/*creates audio renderer*/
GF_AudioRenderer *gf_sc_ar_load(GF_Compositor *compositor, u32 init_flags);
/*deletes audio renderer*/
void gf_sc_ar_del(GF_AudioRenderer *ar);

enum
{
	GF_SC_AR_PAUSE=0,
	GF_SC_AR_RESUME,
	GF_SC_AR_RESET_HW_AND_PLAY,
};
/*control audio renderer*/
void gf_sc_ar_control(GF_AudioRenderer *ar, u32 CtrlType);
/*set volume and pan*/
void gf_sc_ar_set_volume(GF_AudioRenderer *ar, u32 Volume);
void gf_sc_ar_set_pan(GF_AudioRenderer *ar, u32 Balance);
/*mute/unmute audio*/
void gf_sc_ar_mute(GF_AudioRenderer *ar, Bool mute);

/*gets time in msec - this is the only clock used by the whole ESM system - depends on the audio driver*/
u32 gf_sc_ar_get_clock(GF_AudioRenderer *ar);
/*reset all input nodes*/
void gf_sc_ar_reset(GF_AudioRenderer *ar);
/*add audio node*/
void gf_sc_ar_add_src(GF_AudioRenderer *ar, GF_AudioInterface *source);
/*remove audio node*/
void gf_sc_ar_remove_src(GF_AudioRenderer *ar, GF_AudioInterface *source);
/*reconfig audio hardware if needed*/
void gf_sc_ar_send_or_reconfig(GF_AudioRenderer *ar);

void gf_sc_ar_update_video_clock(GF_AudioRenderer *ar, u32 video_ts);


/*the sound node interface for intensity & spatialization*/
typedef struct _soundinterface
{
	/*gets volume for each channel - vol = Fixed[GF_AUDIO_MIXER_MAX_CHANNELS]. returns 1 if volume shall be changed (!= 1.0)
	if NULL channels are always at full intensity*/
	Bool (*GetChannelVolume)(GF_Node *owner, Fixed *vol);
	/*node owning the structure*/
	GF_Node *owner;
} GF_SoundInterface;

/*audio common to AudioClip and AudioSource*/
typedef struct
{
	GF_Node *owner;
	GF_Compositor *compositor;
	GF_AudioInterface input_ifce;
	/*can be NULL if the audio node generates its output from other input*/
	GF_MediaObject *stream;
	/*object speed and intensity*/
	Fixed speed, intensity;
	Bool stream_finished;
	Bool need_release;
	u32 is_open;
	Bool is_muted;
	Bool is_playing;
	Bool register_with_renderer, register_with_parent;

	GF_SoundInterface *snd;
} GF_AudioInput;
/*setup interface with audio renderer - overwrite any functions needed after setup EXCEPT callback object*/
void gf_sc_audio_setup(GF_AudioInput *ai, GF_Compositor *sr, GF_Node *node);
/*unregister interface from renderer/mixer and stops source - deleteing the interface is the caller responsibility*/
void gf_sc_audio_predestroy(GF_AudioInput *ai);
/*open audio object*/
GF_Err gf_sc_audio_open(GF_AudioInput *ai, MFURL *url, Double clipBegin, Double clipEnd, Bool lock_timeline);
/*closes audio object*/
void gf_sc_audio_stop(GF_AudioInput *ai);
/*restarts audio object (cf note in MediaObj)*/
void gf_sc_audio_restart(GF_AudioInput *ai);

Bool gf_sc_audio_check_url(GF_AudioInput *ai, MFURL *url);

/*base grouping audio node (nodes with several audio sources as children)*/
#define AUDIO_GROUP_NODE	\
	GF_AudioInput output;		\
	void (*add_source)(struct _audio_group *_this, GF_AudioInput *src);	\
 
typedef struct _audio_group
{
	AUDIO_GROUP_NODE
} GF_AudioGroup;


/*register audio node with parent audio renderer (mixer or main renderer)*/
void gf_sc_audio_register(GF_AudioInput *ai, GF_TraverseState *tr_state);
void gf_sc_audio_unregister(GF_AudioInput *ai);


#ifndef GPAC_DISABLE_SVG
GF_Err gf_sc_get_mfurl_from_xlink(GF_Node *node, MFURL *mfurl);
Fixed gf_sc_svg_convert_length_to_display(GF_Compositor *sr, SVG_Length *length);
char *gf_scene_resolve_xlink(GF_Node *node, char *the_url);
#endif

GF_Err compositor_2d_set_aspect_ratio(GF_Compositor *sr);
void compositor_2d_set_user_transform(GF_Compositor *sr, Fixed zoom, Fixed tx, Fixed ty, Bool is_resize) ;
GF_Err compositor_2d_get_video_access(GF_VisualManager *surf);
void compositor_2d_release_video_access(GF_VisualManager *surf);
GF_Rect compositor_2d_update_clipper(GF_TraverseState *tr_state, GF_Rect this_clip, Bool *need_restore, GF_Rect *original, Bool for_layer);
Bool compositor_2d_check_attached(GF_VisualManager *visual);
void compositor_2d_clear_surface(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor, u32 is_offscreen);
void compositor_2d_init_callbacks(GF_Compositor *compositor);

#ifndef GPAC_DISABLE_3D
void compositor_2d_reset_gl_auto(GF_Compositor *compositor);
void compositor_2d_hybgl_flush_video(GF_Compositor *compositor, GF_IRect *area);
void compositor_2d_hybgl_clear_surface(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor, u32 is_offscreen_clear);
#endif

Bool compositor_texture_rectangles(GF_VisualManager *visual, GF_TextureHandler *txh, GF_IRect *clip, GF_Rect *unclip, GF_Window *src, GF_Window *dst, Bool *disable_blit, Bool *has_scale);

Bool compositor_get_2d_plane_intersection(GF_Ray *ray, SFVec3f *res);

void compositor_send_resize_event(GF_Compositor *compositor, GF_SceneGraph *subscene, Fixed old_z, Fixed old_tx, Fixed old_ty, Bool is_resize);

void compositor_set_cache_memory(GF_Compositor *compositor, u32 memory);

#ifndef GPAC_DISABLE_3D

GF_Err compositor_3d_set_aspect_ratio(GF_Compositor *sr);
GF_Camera *compositor_3d_get_camera(GF_Compositor *sr);
void compositor_3d_reset_camera(GF_Compositor *sr);
GF_Camera *compositor_layer3d_get_camera(GF_Node *node);
void compositor_layer3d_bind_camera(GF_Node *node, Bool do_bind, u32 nav_value);
void compositor_3d_draw_bitmap(struct _drawable *stack, DrawAspect2D *asp, GF_TraverseState *tr_state, Fixed width, Fixed height, Fixed bmp_scale_x, Fixed bmp_scale_y);

GF_Err compositor_3d_get_screen_buffer(GF_Compositor *sr, GF_VideoSurface *fb, u32 depth_buffer_mode);
GF_Err compositor_3d_get_offscreen_buffer(GF_Compositor *sr, GF_VideoSurface *fb, u32 view_idx, u32 depth_buffer_mode);
GF_Err compositor_3d_release_screen_buffer(GF_Compositor *sr, GF_VideoSurface *framebuffer);

void gf_sc_load_opengl_extensions(GF_Compositor *sr, Bool has_gl_context);

Bool gf_sc_fit_world_to_screen(GF_Compositor *compositor);

GF_Err compositor_3d_setup_fbo(u32 width, u32 height, u32 *fbo_id, u32 *tx_id, u32 *depth_id);
void compositor_3d_delete_fbo(u32 *fbo_id, u32 *fbo_tx_id, u32 *fbo_depth_id, Bool keep_tx_id);
u32 compositor_3d_get_fbo_pixfmt();
void compositor_3d_enable_fbo(GF_Compositor *compositor, Bool enable);

#endif

Bool gf_sc_exec_event(GF_Compositor *sr, GF_Event *evt);
void gf_sc_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, GF_TraverseState *tr_state, s32 *child_idx);

Bool gf_sc_exec_event_vrml(GF_Compositor *compositor, GF_Event *ev);

void gf_sc_visual_register(GF_Compositor *sr, GF_VisualManager *surf);
void gf_sc_visual_unregister(GF_Compositor *sr, GF_VisualManager *surf);
Bool gf_sc_visual_is_registered(GF_Compositor *sr, GF_VisualManager *surf);

Bool gf_sc_pick_in_clipper(GF_TraverseState *tr_state, GF_Rect *clip);

void compositor_gradient_update(GF_TextureHandler *txh);
void compositor_set_ar_scale(GF_Compositor *sr, Fixed scaleX, Fixed scaleY);

/*reset focus if node being deleted has the focus - must be called for each focusable node (internally called for 2D & 3D drawable nodes)*/
void gf_sc_check_focus_upon_destroy(GF_Node *n);

void gf_sc_key_navigator_del(GF_Compositor *sr, GF_Node *n);
void gf_sc_change_key_navigator(GF_Compositor *sr, GF_Node *n);
GF_Node *gf_scene_get_keynav(GF_SceneGraph *sg, GF_Node *sensor);
const char *gf_scene_get_service_url(GF_SceneGraph *sg);


Bool gf_scene_is_over(GF_SceneGraph *sg);
GF_SceneGraph *gf_scene_enum_extra_scene(GF_SceneGraph *sg, u32 *i);

Bool gf_scene_is_dynamic_scene(GF_SceneGraph *sg);

#ifndef GPAC_DISABLE_SVG

void compositor_svg_build_gradient_texture(GF_TextureHandler *txh);

/*base routine fo all svg elements:
	- check for conditional processing (requiredXXX, ...)
	- apply animation and inheritance

	returns 0 if the node shall not be traversed due to conditional processing
*/
Bool compositor_svg_traverse_base(GF_Node *node, SVGAllAttributes *all_atts, GF_TraverseState *tr_state, SVGPropertiesPointers *backup_props, u32 *backup_flags);
Bool compositor_svg_is_display_off(SVGPropertiesPointers *props);
void compositor_svg_apply_local_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix);
void compositor_svg_restore_parent_transformation(GF_TraverseState *tr_state, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix);

void compositor_svg_traverse_children(GF_ChildNodeItem *children, GF_TraverseState *tr_state);

Bool compositor_svg_evaluate_conditional(GF_Compositor *compositor, SVGAllAttributes *all_atts);

/*returns the node associated with the given xlink - this is not always the target node of the xlink structure due
to async restart of animation nodes*/
GF_Node *compositor_svg_get_xlink_resource_node(GF_Node *node, XMLRI *xlink);

#endif

/*Text handling*/

/*we identify the edit caret in a text string as this value*/
#define GF_CARET_CHAR 0x1

typedef struct _gf_font GF_Font;

struct _gf_font
{
	/*fonts are linked within the font manager*/
	GF_Font *next;
	/*list of glyphs in the font*/
	GF_Glyph *glyph;

	char *name;
	u32 em_size;
	u32 styles;
	/*font uits in em size*/
	s32 ascent, descent, underline, line_spacing, max_advance_h, max_advance_v;
	s32 baseline;

	/*only set for embedded font engines (SVG fonts)*/
	GF_Font *(*get_alias)(void *udta);
	GF_Err (*get_glyphs)(void *udta, const char *utf_string, u32 *glyph_ids_buffer, u32 *io_glyph_ids_buffer_size, const char *xml_lang, Bool *is_rtl);
	GF_Glyph *(*load_glyph)(void *udta, u32 glyph_name);
	void *udta;

	Bool not_loaded;

	struct _gf_ft_mgr *ft_mgr;

	GF_Compositor *compositor;
	/*list of spans currently using the font - this is needed to allow for dynamic discard of the font*/
	GF_List *spans;
};

enum
{
	/*span direction is horizontal*/
	GF_TEXT_SPAN_HORIZONTAL = 1,
	/*span is underlined*/
	GF_TEXT_SPAN_UNDERLINE = 1<<1,
	/*span is fliped (coord systems with Y-axis pointing downwards like SVG)*/
	GF_TEXT_SPAN_FLIP = 1<<2,
	/*span is in the current text selection*/
	GF_TEXT_SPAN_RIGHT_TO_LEFT = 1<<3,
	/*span is in the current text selection*/
	GF_TEXT_SPAN_SELECTED = 1<<4,
	/*span is strikeout*/
	GF_TEXT_SPAN_STRIKEOUT = 1<<5
};

typedef struct __text_span
{
	GF_Font *font;

	GF_Glyph **glyphs;
	u32 nb_glyphs;

	u32 flags;

	Fixed font_size;

	/*scale to apply to get to requested font size*/
	Fixed font_scale;
	GF_Rect bounds;

	/*MPEG-4 span scaling (length & maxExtend)*/
	Fixed x_scale, y_scale;
	/*x (resp. y) offset in local coord system. Ignored if per-glyph dx (resp dy) are specified*/
	Fixed off_x, off_y;

	/*per-glyph positioning - when allocated, this is the same number as the glyphs*/
	Fixed *dx, *dy, *rot;

	/*span language*/
//	const char *lang;

	/*span texturing and 3D tools*/
	struct _span_internal *ext;

	/*SVG stuff :(*/
	GF_Node *anchor;
	GF_Node *user;
} GF_TextSpan;

GF_FontManager *gf_font_manager_new();
void gf_font_manager_del(GF_FontManager *fm);

GF_Font *gf_font_manager_set_font(GF_FontManager *fm, char **alt_fonts, u32 nb_fonts, u32 styles);
GF_Font *gf_font_manager_set_font_ex(GF_FontManager *fm, char **alt_fonts, u32 nb_fonts, u32 styles, Bool check_only);

GF_TextSpan *gf_font_manager_create_span(GF_FontManager *fm, GF_Font *font, char *span, Fixed font_size, Bool needs_x_offset, Bool needs_y_offset, Bool needs_rotate, const char *lang, Bool fliped_text, u32 styles, GF_Node *user);
void gf_font_manager_delete_span(GF_FontManager *fm, GF_TextSpan *tspan);

GF_Err gf_font_manager_register_font(GF_FontManager *fm, GF_Font *font);
GF_Err gf_font_manager_unregister_font(GF_FontManager *fm, GF_Font *font);

void gf_font_manager_refresh_span_bounds(GF_TextSpan *span);
GF_Path *gf_font_span_create_path(GF_TextSpan *span);


void gf_font_spans_draw_2d(GF_List *spans, GF_TraverseState *tr_state, u32 hl_color, Bool force_texture_text, GF_Rect *bounds);
void gf_font_spans_draw_3d(GF_List *spans, GF_TraverseState *tr_state, DrawAspect2D *asp, u32 text_hl, Bool force_texturing);
void gf_font_spans_pick(GF_Node *node, GF_List *spans, GF_TraverseState *tr_state, GF_Rect *node_bounds, Bool use_dom_events, struct _drawable *drawable);
void gf_font_spans_get_selection(GF_Node *node, GF_List *spans, GF_TraverseState *tr_state);

GF_Font *gf_compositor_svg_set_font(GF_FontManager *fm, char *a_font, u32 styles, Bool check_only);

/*switches focus node:
@compositor: compositor
@move_prev: finds previous focus rather than next
@focus: new focus if force_focus_type is set
@force_focus_type: 0: focus not forced, 1: focus forced to focus, 2: focus forced to prev/next focusable child of focus node*/
u32 gf_sc_focus_switch_ring(GF_Compositor *compositor, Bool move_prev, GF_Node *focus, u32 force_focus_type);

Bool compositor_handle_navigation(GF_Compositor *compositor, GF_Event *ev);

void compositor_handle_auto_navigation(GF_Compositor *compositor);

void gf_sc_next_frame_state(GF_Compositor *compositor, u32 state);


#ifdef GPAC_USE_TINYGL
void gf_get_tinygl_depth(GF_TextureHandler *txh);
#endif


/*TODO - remove this !!*/
typedef struct
{
	void *udta;
	/*called when new video frame is ready to be flushed on screen. time is the compositor global clock in ms*/
	void (*on_video_frame)(void *udta, u32 time);
	/*called when video output has been resized*/
	void (*on_video_reconfig)(void *udta, u32 width, u32 height, u8 bpp);
} GF_VideoListener;

GF_Err gf_sc_set_scene_size(GF_Compositor *compositor, u32 Width, u32 Height, Bool force_size);

void gf_sc_sys_frame_pending(GF_Compositor *compositor, u32 cts, u32 obj_time, GF_Filter *from_filter);
Bool gf_sc_check_sys_frame(GF_Scene *scene, GF_ObjectManager *odm, GF_FilterPid *for_pid, GF_Filter *from_filter, u64 cts_in_ms, u32 dur_in_ms);

/*
get diff in ms (independent of clock absolute speed value, only speed sign) between a clock value and a timestamp in ms
return value is positive if timestamp is ahead (in the future of the clock timeline) of clock, negative if in the past (late)
*/
s32 gf_clock_diff(GF_Clock *ck, u32 ck_time, u32 cts);

Bool gf_sc_is_over(GF_Compositor *compositor, GF_SceneGraph *scene_graph);

/*returns true if scene or current layer accepts tghe requested navigation type, false otherwise*/
Bool gf_sc_navigation_supported(GF_Compositor *compositor, u32 type);

u32 gf_sc_check_end_of_scene(GF_Compositor *compositor, Bool skip_interactions);


/*add/rem node requiring a call to render without being present in traversed graph (VRML/MPEG-4 protos).
For these nodes, the traverse effect passed will be NULL.*/
void gf_sc_queue_node_traverse(GF_Compositor *compositor, GF_Node *node);
void gf_sc_unqueue_node_traverse(GF_Compositor *compositor, GF_Node *node);

typedef struct
{
	/*od_manager owning service, NULL for services created for remote channels*/
	struct _od_manager *owner;

	/*service url*/
	char *url;
	char *url_frag;
	GF_Filter *source_filter;

	/*number of attached remote channels ODM (ESD URLs)*/
//	u32 nb_ch_users;
	/*number of attached remote ODM (OD URLs)*/
	u32 nb_odm_users;

	/*clock objects. Kept at service level since ESID namespace is the service one*/
	GF_List *clocks;

	Fixed set_speed;
	Bool connect_ack;
} GF_SceneNamespace;



/*opens service - performs URL concatenation if parent service specified*/
GF_SceneNamespace *gf_scene_ns_new(GF_Scene *scene, GF_ObjectManager *owner, const char *url, const char *parent_url);
/*destroy service*/
void gf_scene_ns_del(GF_SceneNamespace *ns, GF_Scene *scene);

void gf_scene_ns_connect_object(GF_Scene *scene, GF_ObjectManager *odm, char *serviceURL, char *parent_url, GF_SceneNamespace *parent_ns);



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

	/*list of GF_MediaObject - these are the links between scene nodes (URL, xlink:href) and media resources.
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
	with static libgpac (avoids depending on QuickJS and OpenGL32 if not needed)*/
	void (*on_media_event)(GF_Scene *scene, u32 type);

	/*duration of inline scene*/
	u64 duration;

	u32 nb_buffering;
	u32 nb_rebuffer;

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
	SFURL visual_url, audio_url, text_url, dims_url, subs_url;

	Bool is_tiled_srd;
	u32 srd_type;
	s32 srd_min_x, srd_max_x, srd_min_y, srd_max_y;

	u32 ambisonic_type;

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

	/*list of attached Inline nodes*/
	GF_List *attached_inlines;
#endif


	Bool disable_hitcoord_notif;

	u32 addon_position, addon_size_factor;

	GF_List *declared_addons;
	//set when content is replaced by an addon (DASH PVR mode)
	Bool main_addon_selected;
	u32 sys_clock_at_main_activation;
	u64 obj_clock_at_main_activation;

	//0: no pause - 1: paused and trigger pause command to net, 2: only clocks are paused but commands not sent
	u32 first_frame_pause_type;
	u32 vr_type;

	GF_Compositor *compositor;

	//only for root scene
	GF_List *namespaces;

	Bool has_splicing_addons;
};

GF_Scene *gf_scene_new(GF_Compositor *compositor, GF_Scene *parentScene);
void gf_scene_del(GF_Scene *scene);

struct _od_manager *gf_scene_find_odm(GF_Scene *scene, u16 OD_ID);
void gf_scene_disconnect(GF_Scene *scene, Bool for_shutdown);

GF_Scene *gf_scene_get_root_scene(GF_Scene *scene);
Bool gf_scene_is_root(GF_Scene *scene);

void gf_scene_remove_object(GF_Scene *scene, GF_ObjectManager *odm, u32 for_shutdown);
/*browse all (media) channels and send buffering info to the app*/
void gf_scene_buffering_info(GF_Scene *scene, Bool rebuffer_done);
void gf_scene_attach_to_compositor(GF_Scene *scene);
struct _mediaobj *gf_scene_get_media_object(GF_Scene *scene, MFURL *url, u32 obj_type_hint, Bool lock_timelines);
struct _mediaobj *gf_scene_get_media_object_ex(GF_Scene *scene, MFURL *url, u32 obj_type_hint, Bool lock_timelines, struct _mediaobj *sync_ref, Bool force_new_if_not_attached, GF_Node *node_ptr, GF_Scene *original_parent_scene);
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
instead of closed and reopened. If a media control is present on inline, from_time is overridden by MC range*/
void gf_scene_restart_dynamic(GF_Scene *scene, s64 from_time, Bool restart_only, Bool disable_addon_check);

void gf_scene_insert_pid(GF_Scene *scene, GF_SceneNamespace *sns, GF_FilterPid *pid, Bool is_in_iod);

/*exported for compositor: handles filtering of "self" parameter indicating anchor only acts on container inline scene
not root one. Returns 1 if handled - cf \ref GF_EventNavigate*/
Bool gf_scene_process_anchor(GF_Node *caller, GF_Event *evt);
void gf_scene_force_size_to_video(GF_Scene *scene, GF_MediaObject *mo);

//check clock status.
//If @check_buffering is 0, returns 1 if all clocks have seen eos, 0 otherwise
//If @check_buffering is 1, returns 1 if no clock is buffering, 0 otheriwse
Bool gf_scene_check_clocks(GF_SceneNamespace *ns, GF_Scene *scene, Bool check_buffering);

void gf_scene_notify_event(GF_Scene *scene, u32 event_type, GF_Node *n, void *dom_evt, GF_Err code, Bool no_queuing);

void gf_scene_mpeg4_inline_restart(GF_Scene *scene);
void gf_scene_mpeg4_inline_check_restart(GF_Scene *scene);


GF_Node *gf_scene_get_subscene_root(GF_Node *inline_node);

void gf_scene_select_main_addon(GF_Scene *scene, GF_ObjectManager *odm, Bool set_on, u64 absolute_clock_time);
void gf_scene_reset_addons(GF_Scene *scene);
void gf_scene_reset_addon(GF_AddonMedia *addon, Bool disconnect);

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

/*restarts inline scene - care has to be taken not to remove the scene while it is traversed*/
void gf_inline_restart(GF_Scene *scene);

#endif

/*compares object URL with another URL - ONLY USE THIS WITH DYNAMIC ODs*/
Bool gf_mo_is_same_url(GF_MediaObject *obj, MFURL *an_url, Bool *keep_fragment, u32 obj_hint_type);

void gf_mo_update_caps(GF_MediaObject *mo);
void gf_mo_update_caps_ex(GF_MediaObject *mo, Bool check_unchanged);


const char *gf_scene_get_fragment_uri(GF_Node *node);
void gf_scene_set_fragment_uri(GF_Node *node, const char *uri);


/*GF_NET_ASSOCIATED_CONTENT_TIMING*/
typedef struct
{
	u32 timeline_id;
	u32 media_timescale;
	u64 media_timestamp;
	//for now only used in MPEG-2, so media_pts is in 90khz scale
	u64 media_pts;
	Bool force_reload;
	Bool is_paused;
	Bool is_discontinuity;
	u64 ntp;
} GF_AssociatedContentTiming;

typedef struct
{
	//negative values mean "timeline is ready no need for timing message"
	s32 timeline_id;
	const char *external_URL;
	Bool is_announce, is_splicing;
	Bool reload_external;
	Bool enable_if_defined;
	Bool disable_if_defined;
	GF_Fraction activation_countdown;
	//start and end times of splicing if any
	Double splice_start_time, splice_end_time;
	Bool splice_time_pts;
} GF_AssociatedContentLocation;

void gf_scene_register_associated_media(GF_Scene *scene, GF_AssociatedContentLocation *addon_info);
void gf_scene_notify_associated_media_timeline(GF_Scene *scene, GF_AssociatedContentTiming *addon_time);


/*clock*/
struct _object_clock
{
	u16 clock_id;
	GF_Compositor *compositor;
	GF_Mutex *mx;
	/*no_time_ctrl : set if ANY stream running on this clock has no time control capabilities - this avoids applying
	mediaControl and others that would break stream dependencies*/
	Bool clock_init, has_seen_eos, no_time_ctrl;
	u32 init_timestamp, start_time, pause_time, nb_paused;
	//number of loops (x 0xFFFFFFFFUL) at init time, used to recompute absolute 64bit clock value if needed
	u32 init_ts_loops;

	/*the number of streams buffering on this clock*/
	u32 nb_buffering;
	/*associated media control if any*/
	struct _media_control *mc;
	/*for MC only (no FlexTime)*/
	Fixed speed;
	//whenever speed is changed we store the time at this instant
	u32 speed_set_time;
	s32 audio_delay;

	u32 last_ts_rendered;
	u32 service_id;

	//media time in ms
	u64 media_time_orig;
	//absolute media timestamp in ms corresponding to the media time
	u64 media_ts_orig;
	Bool has_media_time_shift;

	//in ms without 32bit modulo
	u64 ocr_discontinuity_time;
	//we increment this one at each reset, and ask the filter chain to mark packets with this flag
	u32 timeline_id;
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
u64 gf_clock_media_time(GF_Clock *ck);

/*translates from clock time in ms to absolute media time in ms*/
u64 gf_clock_to_media_time(GF_Clock *ck, u32 clock_val);

/*return time in ms since clock started - may be different from clock time when seeking or live*/
u32 gf_clock_elapsed_time(GF_Clock *ck);

/*sets clock time - FIXME: drift updates for OCRs*/
void gf_clock_set_time(GF_Clock *ck, u64 ref_TS, u32 timescale);
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

/*set audio delay, i.e. amount of ms to delay non-audio streams
audio is usually send to the sound card quite ahead of time, depending on the output compositor settings*/
void gf_clock_set_audio_delay(GF_Clock *ck, s32 ms_delay);

//convert a 64-bit timestamp to clock time in ms. Clock times are always on 32 bits
u32 gf_timestamp_to_clocktime(u64 ts, u32 timescale);

//get absolute clock time in ms, including wrapping due to 32bit counting
u64 gf_clock_time_absolute(GF_Clock *ck);

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

	/*flag indicates the odm is a target passthrough*/
	GF_ODM_PASSTHROUGH = (1<<15),
	/*flag indicates the clock is shared between tiles and a play should not trigger a rebuffer*/
	GF_ODM_TILED_SHARED_CLOCK = (1<<16),
	/*flag indicates TEMI info is associated with PID*/
	GF_ODM_HAS_TEMI = (1<<17),

	/*flag indicates this visual pid is a text subtitle*/
	GF_ODM_IS_SPARSE = (1<<18),

	/*flag set when ODM is a forced subtitle in normal play mode*/
	GF_ODM_SUB_FORCED = (1<<19),
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

	u32 prev_clock_at_discontinuity_plus_one;

	u32 nb_dropped;

	GF_FilterPid *pid;
	//object ID for linking with mediaobjects
	u32 ID;
	u32 pid_id; //esid for clock solving

	//MPEG-4 OD descriptor
	GF_ObjectDescriptor *OD;

	//parent service ID as defined from input
	u32 ServiceID;
	Bool hybrid_layered_coded;

	Bool has_seen_eos;
	GF_List *extra_pids;

	Bool clock_inherited;
	//0 or 1, except for IOD where we may have several BIFS/OD streams
	u32 nb_buffering, nb_rebuffer;
	u32 buffer_max_ms, buffer_min_ms, buffer_playout_ms;
	Bool blocking_media;

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
	/* during playback: timing as evaluated by the composition memory or the scene codec, in absolute media time
	or in absolute clock time if no media time mapping*/
	u64 media_current_time;
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
	//delay in PID timescale
	s64 timestamp_offset;
	
	Fixed set_speed;
	Bool disable_buffer_at_next_play;

//	u32 raw_media_frame_pending;
	GF_Semaphore *raw_frame_sema;

#ifndef GPAC_DISABLE_VRML
	/*the one and only media control currently attached to this object*/
	struct _media_control *media_ctrl;
	/*the list of media control controlling the object*/
	GF_List *mc_stack;
	/*the media sensor(s) attached to this object*/
	GF_List *ms_stack;
#endif

	//only set on root OD of addon subscene, which gather all the hybrid resources
	GF_AddonMedia *addon;
	//set for objects splicing the main content, indicates the media type (usually in @codec but no codec created for splicing)
	u32 splice_addon_mtype;

	//for a regular ODM, this indicates that the current scalable_odm associated
	struct _od_manager *upper_layer_odm;
	//for a scalable ODM, this indicates the lower layer odm associated
	struct _od_manager *lower_layer_odm;

	s32 last_drawn_frame_ntp_diff;
	u64 last_drawn_frame_ntp_sender, last_drawn_frame_ntp_receive;
	u32 ambi_ch_id;

	const char *redirect_url;
	/*0: not set, 1: set , 2: set and disconnect was called to remove the object*/
	u32 skip_disconnect_state;

	Bool ignore_sys;
	u64 last_filesize_signaled;
	Bool too_slow;
};

GF_ObjectManager *gf_odm_new();
void gf_odm_del(GF_ObjectManager *ODMan);

/*setup OD*/
void gf_odm_setup_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, GF_FilterPid *for_pid);

void gf_odm_setup_remote_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, char *remote_url, Bool for_addon);

/*disconnect OD and removes it if desired (otherwise only STOP is propagated)*/
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

void gf_odm_update_duration(GF_ObjectManager *odm, GF_FilterPid *pid);

GF_Segment *gf_odm_find_segment(GF_ObjectManager *odm, char *descName);

Bool gf_odm_stop_or_destroy(GF_ObjectManager *odm);

void gf_odm_signal_eos_reached(GF_ObjectManager *odm);

void gf_odm_reset_media_control(GF_ObjectManager *odm, Bool signal_reset);

void gf_odm_check_clock_mediatime(GF_ObjectManager *odm);

void gf_odm_addon_setup(GF_ObjectManager *odm);


/*!
 *	Media Object types
 *
 *	This type provides a hint to network modules which may have to generate an service descriptor on the fly.
 *	They occur only if objects/services used in the scene are not referenced through ObjectDescriptors (MPEG-4)
 *	but direct through URL
*/
enum
{
	/*!service descriptor expected is of undefined type. This should be treated like GF_MEDIA_OBJECT_SCENE*/
	GF_MEDIA_OBJECT_UNDEF = 0,
	/*!service descriptor expected is of SCENE type and shall contain a scene stream and OD one if needed*/
	GF_MEDIA_OBJECT_SCENE,
	/*!service descriptor expected is of SCENE UPDATES type (animation streams)*/
	GF_MEDIA_OBJECT_UPDATES,
	/*!service descriptor expected is of VISUAL type*/
	GF_MEDIA_OBJECT_VIDEO,
	/*!service descriptor expected is of AUDIO type*/
	GF_MEDIA_OBJECT_AUDIO,
	/*!service descriptor expected is of TEXT type (3GPP/MPEG4)*/
	GF_MEDIA_OBJECT_TEXT,
	/*!service descriptor expected is of UserInteraction type (MPEG-4 InputSensor)*/
	GF_MEDIA_OBJECT_INTERACT
};

/*! All Media Objects inserted through URLs and not MPEG-4 OD Framework use this ODID*/
#define GF_MEDIA_EXTERNAL_ID		1050

enum
{
	//no connection error, no frames seen in input pipeline
	MO_CONNECT_OK=0,
	//no connection error, frames seen in input pipeline but no frame yet available for object
	MO_CONNECT_BUFFERING,
	//explicit source setup failure
	MO_CONNECT_FAILED,
	//timeout of input pipeline (no frames seen after compositor->timeout ms)
	MO_CONNECT_TIMEOUT
};

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
	u32 last_fetch_time;
	Bool first_frame_fetched;
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
	u8 *frame;
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
	Bool srd_map_changed;

	/*currently valid properties of the object*/
	u32 width, height, stride, pixel_ar, pixelformat, bitrate;
	Bool is_flipped;
	u32 sample_rate, num_channels, afmt, bytes_per_sec;
	u64 channel_config;
	Bool planar_audio;
	u32 srd_x, srd_y, srd_w, srd_h;
	u32 srd_full_w, srd_full_h, srd_map_ox, srd_map_oy;
	u32 flip, rotate;

	u32 quality_degradation_hint;
	u32 nb_views;
	u32 nb_layers;
	u32 view_min_x, view_max_x, view_min_y, view_max_y;
	GF_FilterFrameInterface *frame_ifce;

	Float c_x, c_y, c_w, c_h;
	u32 connect_state;
};

GF_MediaObject *gf_mo_new();


/*media access events */
void gf_odm_service_media_event(GF_ObjectManager *odm, GF_EventType event_type);
void gf_odm_service_media_event_with_download(GF_ObjectManager *odm, GF_EventType event_type, u64 loaded_size, u64 total_size, u32 bytes_per_sec, u32 buffer_level_plus_one, u32 min_buffer_time);

/*checks the URL and returns the ODID (MPEG-4 od://) or GF_MEDIA_EXTERNAL_ID for all regular URLs*/
u32 gf_mo_get_od_id(MFURL *url);

void gf_scene_generate_views(GF_Scene *scene, char *url, char *parent_url);
void gf_scene_generate_mosaic(GF_Scene *scene, char *url, char *parent_path);
//sets pos and size of addon
//	size is 1/2 height (0), 1/3 (1) or 1/4 (2)
//	pos is bottom-left(0), top-left (1) bottom-right (2) or top-right (3)
void gf_scene_set_addon_layout_info(GF_Scene *scene, u32 position, u32 size_factor);


void gf_scene_message(GF_Scene *scene, const char *service, const char *message, GF_Err error);
void gf_scene_message_ex(GF_Scene *scene, const char *service, const char *message, GF_Err error, Bool no_filtering);

//returns media time in sec for the addon - timestamp_based is set to 1 if no timeline has been found (eg sync is based on direct timestamp comp)
Double gf_scene_adjust_time_for_addon(GF_AddonMedia *addon, Double clock_time, u8 *timestamp_based);
s64 gf_scene_adjust_timestamp_for_addon(GF_AddonMedia *addon, u64 orig_ts_ms);

/*check if the associated addon has to be restarted, based on the timestamp of the main media (used for scalable addons only). Returns 1 if the addon has been restarted*/
Bool gf_scene_check_addon_restart(GF_AddonMedia *addon, u64 cts, u64 dts);

/*callbacks for scene graph library so that all related ESM nodes are properly instanciated*/
void gf_scene_node_callback(void *_is, GF_SGNodeCbkType type, GF_Node *node, void *param);


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
	//addon used for temporary splicing
	GF_ADDON_TYPE_SPLICED,
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

	Double splice_start, splice_end;
	Bool is_over;
	Bool splice_in_pts;
	u32 nb_splicing;
	Bool min_dts_set;
	u64 splice_min_dts;
};

void gf_scene_toggle_addons(GF_Scene *scene, Bool show_addons);


void gf_scene_set_service_id(GF_Scene *scene, u32 service_id);


/*post extended user mouse interaction to compositor
	X and Y are point coordinates in the display expressed in 2D coord system top-left (0,0), Y increasing towards bottom
	@xxx_but_down: specify whether the mouse button is down(2) or up (1), 0 if unchanged
	@wheel: specify current wheel inc (0: unchanged , +1 for one wheel delta forward, -1 for one wheel delta backward)
*/
void gf_sc_input_sensor_mouse_input(GF_Compositor *compositor, GF_EventMouse *event);

/*post extended user key interaction to compositor
	@key_code: GPAC DOM code of input key
	@hw_code: hardware code of input key
	@isKeyUp: set if key is released
*/
Bool gf_sc_input_sensor_keyboard_input(GF_Compositor *compositor, u32 key_code, u32 hw_code, Bool isKeyUp);

/*post extended user character interaction to compositor
	@character: unicode character input
*/
void gf_sc_input_sensor_string_input(GF_Compositor *compositor, u32 character);

GF_Err gf_input_sensor_setup_object(GF_ObjectManager *odm, GF_ESD *esd);
void gf_input_sensor_delete(GF_ObjectManager *odm);

void InitInputSensor(GF_Scene *scene, GF_Node *node);
void InputSensorModified(GF_Node *n);
void InitKeySensor(GF_Scene *scene, GF_Node *node);
void InitStringSensor(GF_Scene *scene, GF_Node *node);


/*! Media Object information*/
typedef struct
{
	u32 ODID;
	u32 ServiceID;
	u32 pid_id, ocr_id;
	Double duration;
	Double current_time;
	/*0: stopped, 1: playing, 2: paused, 3: not setup, 4; setup failed.*/
	u32 status;

	Bool raw_media;
	Bool generated_scene;

	/*name of module handling the service service */
	const char *service_handler;
	/*name of service*/
	const char *service_url;
	/*service redirect object*/
	const char *remote_url;
	/*set if the service is owned by this object*/
	Bool owns_service;

	/*stream buffer:
		-2: stream is not playing
		-1: stream has no buffering
		>=0: amount of media data present in buffer, in ms
	*/
	s32 buffer;
	u32 min_buffer, max_buffer;
	/*number of AUs in DB (cumulated on all input channels)*/
	u32 db_unit_count;
	/*number of CUs in composition memory (if any) and CM capacity*/
	u32 cb_unit_count, cb_max_count;
	/*inidciate that thye composition memory is bypassed for this decoder (video only) */
	Bool direct_video_memory;
	/*clock delay in ms of object clock: this is the delay set by the audio renderer to keep AV in sync
	and corresponds to the amount of ms to delay non-audio streams to keep sync*/
	s32 clock_drift;
	/*codec name*/
	const char *codec_name;
	/*object type - match streamType (cf constants.h)*/
	u32 od_type;
	/*audio properties*/
	u32 sample_rate, afmt, num_channels;
	/*video properties (w & h also used for scene codecs)*/
	u32 width, height, pixelFormat, par;

	/*average birate over last second and max bitrate over one second at decoder input according to media timeline - expressed in bits per sec*/
	u32 avg_bitrate, max_bitrate;
	/*average birate over last second and max bitrate over one second at decoder input according to processing time - expressed in bits per sec*/
	u32 avg_process_bitrate, max_process_bitrate;
	u32 nb_dec_frames, nb_dropped;
	u32 first_frame_time, last_frame_time;
	u64 total_dec_time, irap_total_dec_time;
	u32 max_dec_time, irap_max_dec_time;
	u32 au_duration;
	u32 nb_iraps;
	s32 ntp_diff;
	//0 if mono, 2 or more otherwise
	u32 nb_views;

	/*4CC of original protection scheme used*/
	u32 protection;

	u32 lang;
	const char *lang_code;

	/*name of media if not defined in OD framework*/
	const char *media_url;
} GF_MediaInfo;
//todo deprecate ?
GF_Err gf_odm_get_object_info(GF_ObjectManager *odm, GF_MediaInfo *info);


/*restart object and takes care of media control/clock dependencies*/
void mediacontrol_restart(GF_ObjectManager *odm);
void mediacontrol_pause(GF_ObjectManager *odm);
//resumes object.
//If @resume_to_live is set, will deactivate main addon acting as PVR
void mediacontrol_resume(GF_ObjectManager *odm, Bool resume_to_live);

Bool MC_URLChanged(MFURL *old_url, MFURL *new_url);

void mediasensor_update_timing(GF_ObjectManager *odm, Bool is_eos);

#ifndef GPAC_DISABLE_VRML

typedef struct
{
	GF_AudioInput input;
	GF_TimeNode time_handle;
	Double start_time;
	Bool set_duration, failure;
} AudioClipStack;


typedef struct
{
	GF_AudioInput input;
	GF_TimeNode time_handle;
	Bool is_active;
	Double start_time;
} AudioSourceStack;


/*to do: add preroll support*/
typedef struct _media_control
{
	M_MediaControl *control;

	/*store current values to detect changes*/
	Double media_start, media_stop;
	Fixed media_speed;
	Bool enabled;
	MFURL url;

	GF_Scene *parent;
	/*stream owner*/
	GF_MediaObject *stream;
	/*stream owner's clock*/
	GF_Clock *ck;

	u32 changed;
	Bool is_init;
	Bool paused;
	u32 prev_active;

	/*stream object list (segments)*/
	GF_List *seg;
	/*current active segment index (ie, controlling the PLAY range of the media)*/
	u32 current_seg;
} MediaControlStack;
void InitMediaControl(GF_Scene *scene, GF_Node *node);
void MC_Modified(GF_Node *node);

void MC_GetRange(MediaControlStack *ctrl, Double *start_range, Double *end_range);

/*assign mediaControl for this object*/
void gf_odm_set_mediacontrol(GF_ObjectManager *odm, struct _media_control *ctrl);
/*get media control ruling the clock the media is running on*/
struct _media_control *gf_odm_get_mediacontrol(GF_ObjectManager *odm);
/*removes control from OD context*/
void gf_odm_remove_mediacontrol(GF_ObjectManager *odm, struct _media_control *ctrl);
/*switches control (propagates enable=FALSE), returns 1 if control associated with OD has changed to new one*/
Bool gf_odm_switch_mediacontrol(GF_ObjectManager *odm, struct _media_control *ctrl);

/*returns 1 if this is a segment switch, 0 otherwise - takes care of object restart if segment switch*/
Bool gf_odm_check_segment_switch(GF_ObjectManager *odm);

typedef struct _media_sensor
{
	M_MediaSensor *sensor;

	GF_Scene *parent;

	GF_List *seg;
	Bool is_init;
	/*stream owner*/
	GF_MediaObject *stream;

	/*private cache (avoids browsing all sensor*/
	u32 active_seg;
} MediaSensorStack;

void InitMediaSensor(GF_Scene *scene, GF_Node *node);
void MS_Modified(GF_Node *node);

void MS_Stop(MediaSensorStack *st);


void InitMediaControl(GF_Scene *scene, GF_Node *node);
void MC_Modified(GF_Node *node);

void InitMediaSensor(GF_Scene *scene, GF_Node *node);
void MS_Modified(GF_Node *node);

void gf_init_inline(GF_Scene *scene, GF_Node *node);
void gf_inline_on_modified(GF_Node *node);

void gf_scene_init_storage(GF_Scene *scene, GF_Node *node);

#endif	/*GPAC_DISABLE_VRML*/

Bool gf_sc_check_gl_support(GF_Compositor *compositor);
void gf_sc_mo_destroyed(GF_Node *n);
Bool gf_sc_on_event(void *cbck, GF_Event *event);

#ifdef __cplusplus
}
#endif
#endif	/*_COMPOSITOR_DEV_H_*/

