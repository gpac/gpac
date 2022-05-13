/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

#ifndef _GF_COMPOSITOR_H_
#define _GF_COMPOSITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/compositor.h>
\brief GPAC A/V/2D/3D compositor/rendering.
*/
	
/*!
\addtogroup compose_grp Compositor
\ingroup playback_grp
\brief GPAC A/V/2D/3D compositor/rendering.

This section documents the compositor of GPAC in charge of assembling audio, images, video, text, 2D and 3D graphics
  in a timed fashion. The compositor can only be run as a filter starting from GPAC 0.9.0, as it requires the filters API
  to fetch media data.
  The compositor can work in real-time mode (player) or as a regular filter. See `gpac -h compositor`for more information.

  The compositor object is the private data of the compositor filter and can be retrieved using \ref gf_filter_get_udta

  The compositor object will look for the following variables in the temp section of the GPAC config:
	- window-flags: output window initialization flags - see GF_VideoOutputWindowFlags
	- window-handle: OS-specific window handle (HWND on windows,
	- window-display

@{
 */
	

/*include scene graph API*/
#include <gpac/scenegraph.h>
/*GF_Event*/
#include <gpac/events.h>
/*frame buffer definition*/
#include <gpac/color.h>

/*! Compositor object*/
typedef struct __tag_compositor GF_Compositor;

/*! loads a compositor object
\param compositor a preallocated structure for the compositor to initialize
\return error if any
*/
GF_Err gf_sc_load(GF_Compositor *compositor);
/*! unloads compositor
\param compositor the compositor object to unload. The structure memory is not freed
*/
void gf_sc_unload(GF_Compositor *compositor);

/*! sets simulation frame rate. The compositor framerate impacts the frequency at which time nodes and animations are updated,
but does not impact the video objects frame rates.
\param compositor the target compositor
\param fps the desired frame rate
*/
void gf_sc_set_fps(GF_Compositor *compositor, GF_Fraction fps);

/*! sets the root scene graph of the compositor.
\param compositor the target compositor
\param scene_graph the scene graph to attach. If NULL, removes current scene and resets simulation time
\return error if any
*/
GF_Err gf_sc_set_scene(GF_Compositor *compositor, GF_SceneGraph *scene_graph);

/*! draws a single frame. If the frame is drawn, a packet is sent on the compositor vout output pi
\param compositor the target compositor
\param no_video_flush disables video frame flushing to graphics card. Ignored in non-player mode
\param ms_till_next set to the number of milliseconds until next expected frame
\return GF_TRUE if there are pending tasks (frame late, fonts pending, etc) or GF_FALSE if everything was ready while drawing the frame
*/
Bool gf_sc_draw_frame(GF_Compositor *compositor, Bool no_video_flush, s32 *ms_till_next);

/*! notify the given node has been modified. The compositor filters object to decide whether the scene graph has to be
traversed or not.
\param compositor the target compositor
\param node the node to invalidate. If NULL, this means complete traversing of the graph is requested
*/
void gf_sc_invalidate(GF_Compositor *compositor, GF_Node *node);


/*! mark next frame as required to be refreshed
\param compositor the target compositor
*/
void gf_sc_invalidate_next_frame(GF_Compositor *compositor);

/*! checks if a frame has been produced since last call to \ref gf_sc_invalidate_next_frame. This is typically used when the caller app has no control over flush
\param compositor the target compositor
\return GF_TRUE if frame was produced , GF_FALSE otherwise
*/
Bool gf_sc_frame_was_produced(GF_Compositor *compositor);

/*! returns the compositor time. The compositor time is the time every time line is synchronized to
\param compositor the target compositor
\return compositor time in milliseconds
*/
u32 gf_sc_get_clock(GF_Compositor *compositor);

/*! signals the node or scenegraph is about to be destroyed. This should be called after the node destructor if any.
This function cleans up any pending events on the target node or graph.
\param compositor the target compositor
\param node the target node. If NULL, sg shall be set to the scenegraph about to be destroyed
\param sg the target scenegraph
*/
void gf_sc_node_destroy(GF_Compositor *compositor, GF_Node *node, GF_SceneGraph *sg);

/*! locks/unlocks the visual scene rendering. Modifications of the scene tree shall only happen when scene compositor is locked
\param compositor the target compositor
\param do_lock indicates if the compositor should be locked (GF_TRUE) or released (GF_FALSE)
*/
void gf_sc_lock(GF_Compositor *compositor, Bool do_lock);

/*! notify user input
\param compositor the target compositor
\param event the target event to notify
\return GF_FALSE if event hasn't been handled by the compositor, GF_TRUE otherwise*/
Bool gf_sc_user_event(GF_Compositor *compositor, GF_Event *event);

/*! disconnects the current url
\param compositor the target compositor
*/
void gf_sc_disconnect(GF_Compositor *compositor);

/*! connects a given url
\param compositor the target compositor
\param URL the target URL
\param startTime start time in milliseconds
\param pause_at_first_frame indicate if compositor should pause once first frame is reached
	- If 1, pauses at the first frame
	- If 2, pauses at the first frame only if the compositor is in paused state
	- otherwise do not pause
\param secondary_scene indicate this is a secondary scene (subs, etc)
\param parent_path parent path of URL, may be NULL
*/
void gf_sc_connect_from_time(GF_Compositor *compositor, const char *URL, u64 startTime, u32 pause_at_first_frame, Bool secondary_scene, const char *parent_path);


/*! maps screen coordinates to bifs 2D coordinates for the current zoom/pan settings. The input coordinate X and Y are
 point coordinates in the display expressed in BIFS-like fashion (0,0) at center of display and Y increasing from bottom to top

\param compositor the target compositor
\param X horizontal point coordinate
\param Y vertical point coordinate
\param bifsX set to the scene horizontal coordinate of the point
\param bifsY set to the scene vertical coordinate of the point
*/
void gf_sc_map_point(GF_Compositor *compositor, s32 X, s32 Y, Fixed *bifsX, Fixed *bifsY);


/*! returns current FPS
\param compositor the target compositor
\param absoluteFPS if set to GF_TRUE, the return value is the absolute framerate, eg NbFrameCount/NbTimeSpent regardless of
whether a frame has been drawn or not, which means the FPS returned can be much greater than the compositor FPS. If set to GF_FALSE, the return value is the FPS taking into account not drawn frames (eg, less than or equal to compositor FPS)
\return the current frame rate
*/
Double gf_sc_get_fps(GF_Compositor *compositor, Bool absoluteFPS);

/*! checks if a text selection is in progress
\param compositor the target compositor
\return GF_TRUE is some text is selected, GF_FALSE otherwise
*/
Bool gf_sc_has_text_selection(GF_Compositor *compositor);

/*! gets text selection
\param compositor the target compositor
\return the selected text as UTF8 string
*/
const char *gf_sc_get_selected_text(GF_Compositor *compositor);

/*! replace text selection content
\param compositor the target compositor
\param text the text to paste as UTF8 string
\return error if any
*/
GF_Err gf_sc_paste_text(GF_Compositor *compositor, const char *text);

/*! adds an object to the scene - only possible if scene has selectable streams (cf GF_OPT_CAN_SELECT_STREAMS option)
\param compositor the target compositor
\param url the URL of the object to inject
\param auto_play selcts the object for playback when inserting it
\return error if any
*/
GF_Err gf_sc_add_object(GF_Compositor *compositor, const char *url, Bool auto_play);

/*! compositor screen buffer grab mode*/
typedef enum
{
	/*! grab color only */
	GF_SC_GRAB_DEPTH_NONE = 0,
	/*! grab depth only */
	GF_SC_GRAB_DEPTH_ONLY = 1,
	/*! grab RGB+depth  */
	GF_SC_GRAB_DEPTH_RGBD = 2,
	/*! grab RGB+depth (7 bits) + 1 bit shape from alpha */
	GF_SC_GRAB_DEPTH_RGBDS = 3
} GF_CompositorGrabMode;

/*! gets screen buffer. This locks the scene graph too until \ref gf_sc_get_offscreen_buffer is called
\param compositor the target compositor
\param framebuffer will be set to the grabbed framebuffer. The pixel data is owned by the compositor and shall not be freed
\param depth_grab_mode mode for depth grabbing in 3D
\return error if any
*/
GF_Err gf_sc_get_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer, GF_CompositorGrabMode depth_grab_mode);

/*! gets offscreen buffer in autostereo rendering modes. This locks the scene graph too until \ref gf_sc_get_offscreen_buffer is called
\param compositor the target compositor
\param framebuffer will be set to the grabbed framebuffer. The pixel data is owned by the compositor and shall not be freed
\param view_idx indicates the 0-based index of the view to grab
\param depth_grab_mode mode for depth grabbing in 3D
\return error if any
*/
GF_Err gf_sc_get_offscreen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer, u32 view_idx, GF_CompositorGrabMode depth_grab_mode);

/*! releases screen buffer and unlocks graph
\param compositor the target compositor
\param framebuffer used during grab call
\return error if any
*/
GF_Err gf_sc_release_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer);

/*! forces full graphics reset (deletes GL textures, FBO, 2D offscreen caches, etc ...).
\param compositor the target compositor
*/
void gf_sc_reset_graphics(GF_Compositor *compositor);

/*! gets viewpoints/viewports for main scene - idx is 1-based, and if greater than number of viewpoints return GF_EOS
\param compositor the target compositor
\param viewpoint_idx the index of the requested viewport. This is a 1-based index, and if greater than number of viewpoints the function will return GF_EOS
\param out_name set to the viewport name. May be NULL
\param is_bound set to GF_TRUE if the viewport is bound. May be NULL
\return GF_EOS if no more viewpoint, or error if any
*/
GF_Err gf_sc_get_viewpoint(GF_Compositor *compositor, u32 viewpoint_idx, const char **out_name, Bool *is_bound);

/*! sets viewpoints/viewports for main scene given its name - idx is 1-based, or 0 to retrieve by viewpoint name
if only one viewpoint is present in the scene, this will bind/unbind it
\param compositor the target compositor
\param viewpoint_idx the index of the viewport to bind. This is a 1-based index, and if greater than number of viewpoints the function will return GF_EOS. Use 0 to bind the viewpoint with the given name.
\param viewpoint_name name of the viewpoint to bind if viewpoint_idx is 0
\return error if any
*/
GF_Err gf_sc_set_viewpoint(GF_Compositor *compositor, u32 viewpoint_idx, const char *viewpoint_name);

/*! renders subscene root node. rs is the current traverse stack
\param compositor the target compositor
\param inline_parent the parent node of the subscene (VRML Inline, SVG animation, ...)
\param subscene the subscene tree to render
\param rs the rendering state at the parent level. This is needed to handle graph metrics changes between scenes
*/
void gf_sc_traverse_subscene(GF_Compositor *compositor, GF_Node *inline_parent, GF_SceneGraph *subscene, void *rs);

/*! sets output (display) size
\param compositor the target compositor
\param new_width the desired new output width
\param new_height the desired new output height
\return error if any
*/
GF_Err gf_sc_set_size(GF_Compositor *compositor, u32 new_width, u32 new_height);

/*! adds or removes extra scene from compositor. Extra scenes are on-screen displays, text tracks or any other scene graphs
not directly loaded by the main scene
\param compositor the target compositor
\param extra_scene the scene graph to add/remove
\param do_remove if set to GF_TRUE, the given scene graph will be unregistered; otherwise, the given scene graph will be registered
*/
void gf_sc_register_extra_graph(GF_Compositor *compositor, GF_SceneGraph *extra_scene, Bool do_remove);

/*! retrieves the compositor object associated with a node. Currently GPAC only handles one possible compositor per node.
\param node the target node
\return the compositor used by the node
*/
GF_Compositor *gf_sc_get_compositor(GF_Node *node);

/*! executes a script action
\param compositor the target compositor
\param type the script action type
\param node the optional node on which the action takes place
\param param the parameter for this action
\return GF_TRUE if success
*/
Bool gf_sc_script_action(GF_Compositor *compositor, GF_JSAPIActionType type, GF_Node *node, GF_JSAPIParam *param);

/*! checks if the given URI matches a built-in GPAC VRML Prototype node
\param compositor the target compositor
\param uri the URI to test
\return GF_TRUE if the URI indicates a built-in prototype, GF_FALSE otherwise
*/
Bool gf_sc_uri_is_hardcoded_proto(GF_Compositor *compositor, const char *uri);

/*! reloads the compositor configuration from the GPAC config file
\param compositor the target compositor
*/
void gf_sc_reload_config(GF_Compositor *compositor);


/*! gets main scene current time
\param compositor the target compositor
\return time in milliseconds
*/
u32 gf_sc_get_time_in_ms(GF_Compositor *compositor);

/*! switches quality up or down - can be called several time in the same direction
this will call all decoders to adjust their quality levels

\param compositor the target compositor
\param up if GF_TRUE, switches quality up,otherwise down
*/
void gf_sc_switch_quality(GF_Compositor *compositor, Bool up);

/*! sets addon on or off (only one addon possible for now). When OFF, the associated service is shut down
\param compositor the target compositor
\param show_addons if GF_TRUE, displays addons otheriwse hides the addons
*/
void gf_sc_toggle_addons(GF_Compositor *compositor, Bool show_addons);

/*! sets playback speed
\param compositor the target compositor
\param speed the requested speed
\return error if any
*/
GF_Err gf_sc_set_speed(GF_Compositor *compositor, Fixed speed);

/*! checks if given URL is understood by client.
\param compositor the target compositor
\param URL the URL to check
\param use_parent_url if GF_TRUE, relative URLs are solved against the current presentation URL
\return GF_TRUE if client should be able to handle the URL*/
Bool gf_sc_is_supported_url(GF_Compositor *compositor, const char *URL, Bool use_parent_url);

/*! navigates to a given destination or shutdown/restart the current one if any.
This is the only safe way of restarting/jumping a presentation from inside the EventProc
where doing a disconnect/connect could deadlock if toURL is NULL, uses the current URL
\param compositor the target compositor
\param toURL the new target URL
*/
void gf_sc_navigate_to(GF_Compositor *compositor, const char *toURL);

/*! restarts url from given time (in ms).
\param compositor the target compositor
\param from_time restart time in milliseconds
\param pause_at_first_frame if 1, pauses at the first frame. If 2, pauses at the first frame only if the compositor is in paused state.
\return	0: service is not connected yet, 1: service has no seeking capabilities, 2: service has been seeked
*/
u32 gf_sc_play_from_time(GF_Compositor *compositor, u64 from_time, u32 pause_at_first_frame);

/*! disconnects the current url
\param compositor the target compositor
*/
void gf_sc_disconnect(GF_Compositor *compositor);


/*! dumps scene graph in specified file, in BT or XMT format
\param compositor the target compositor
\param rad_name file radical (NULL for stdout) - if not NULL MUST BE GF_MAX_PATH length
\param filename sets to the complete filename (rad + ext) and shall be destroyed by caller (optional can be NULL)
\param xml_dump if GF_TRUE, duimps using XML format (XMT-A, X3D) for scene graphs having both XML and simple text representations
\param skip_proto is GF_TRUE, proto declarations are not dumped
\return error if any
*/
GF_Err gf_sc_dump_scene(GF_Compositor *compositor, char *rad_name, char **filename, Bool xml_dump, Bool skip_proto);


/*! sends a set of scene commands (BT, XMT, X3D, LASeR+XML) to the scene
\param compositor the target compositor
\param type indicates the language used - accepted values are
	"model/x3d+xml" or "x3d": commands is an X3D+XML scene
	"model/x3d+vrml" or  "xrdv": commands is an X3D+VRML scene
	"model/vrml" or "vrml": commands is an VRML scene
	"application/x-xmt" or "xmt": commands is an XMT-A scene or a list of XMT-A updates
	"application/x-bt" or "bt": commands is a BT scene or a list of BT updates
	"image/svg+xml" or "svg": commands is an SVG scene
	"application/x-laser+xml" or "laser": commands is an SVG/LASeR+XML  scene or a list of LASeR+XML updates
	if not specified, the type will be guessed from the current root node if any
\param com the textual update
\return error if any
*/
GF_Err gf_sc_scene_update(GF_Compositor *compositor, char *type, char *com);

/*! selects service by given ID for multiplexed services (MPEG-2 TS)
\param compositor the target compositor
\param service_id the service ID to select
*/
void gf_sc_select_service(GF_Compositor *compositor, u32 service_id);

/*! gets URL of main scene

\param compositor the target compositor
\return URL
*/
const char *gf_sc_get_url(GF_Compositor *compositor);

/*! gets main pid in scene

\param compositor the target compositor
\return PID
*/
const void *gf_sc_get_main_pid(GF_Compositor *compositor);

/*! gets world info of root scene

\param compositor the target compositor
\param descriptions filled with descriptions strings (do not free)
\return world title or NULL if no world info
*/
const char *gf_sc_get_world_info(GF_Compositor *compositor, GF_List *descriptions);

/*! gets service ID of root scene

\param compositor the target compositor
\return service ID or 0 if none speicifed (single service in URL)
*/
u32 gf_sc_get_current_service_id(GF_Compositor *compositor);

/*! gets simulation frame rate
\param compositor the target compositor
\param nb_frames_drawn set to the number of frames drawn
\return simulation frames per seconds
*/
Double gf_sc_get_simulation_frame_rate(GF_Compositor *compositor, u32 *nb_frames_drawn);

/*! AspectRatio Type */
enum
{
	 /*! keep AR*/
	GF_ASPECT_RATIO_KEEP = 0,
	/*! keep 16/9*/
	GF_ASPECT_RATIO_16_9,
	/*! keep 4/3*/
	GF_ASPECT_RATIO_4_3,
	/*! none (all rendering area used)*/
	GF_ASPECT_RATIO_FILL_SCREEN
};

/*! AntiAlias settings*/
enum
{
	/*! no antialiasing*/
	GF_ANTIALIAS_NONE = 0,
	/*! only text has antialiasing*/
	GF_ANTIALIAS_TEXT,
	/*! full antialiasing*/
	GF_ANTIALIAS_FULL
};

/*! PlayState settings*/
enum
{
	/*! compositor is playing (get only)*/
	GF_STATE_PLAYING = 0,
	/*! compositor is paused (get only)*/
	GF_STATE_PAUSED,
	/*! On set, compositor will pause after next frame (simulation tick). On get, indicates that rendering step hasn't performed yet*/
	GF_STATE_STEP_PAUSE,
	/*! indicates resume shall restart from live point if any rather than pause point (set only)*/
	GF_STATE_PLAY_LIVE = 10
};

/*! interaction level settings
\note GF_INTERACT_NORMAL and GF_INTERACT_NAVIGATION filter events. If set, any event processed by these 2 modules won't be forwarded to the user
*/
enum
{
	/*! regular interactions enabled (touch sensors)*/
	GF_INTERACT_NORMAL = 1,
	/*! InputSensor interactions enabled (mouse and keyboard)*/
	GF_INTERACT_INPUT_SENSOR = 2,
	/*! all navigation interactions enabled (mouse and keyboard)*/
	GF_INTERACT_NAVIGATION = 4,
};

/*! BoundingVolume settings*/
enum
{
	/*! doesn't draw bounding volume*/
	GF_BOUNDS_NONE = 0,
	/*! draw object bounding box / rect*/
	GF_BOUNDS_BOX,
	/*! draw object AABB tree (3D only) */
	GF_BOUNDS_AABB
};

/*! Wireframe settings*/
enum
{
	/*! draw solid volumes*/
	GF_WIREFRAME_NONE = 0,
	/*! draw only wireframe*/
	GF_WIREFRAME_ONLY,
	/*! draw wireframe on solid object*/
	GF_WIREFRAME_SOLID
};


/*! navigation type*/
enum
{
	/*! navigation is disabled by content and cannot be forced by user*/
	GF_NAVIGATE_TYPE_NONE,
	/*! 2D navigation modes only can be used*/
	GF_NAVIGATE_TYPE_2D,
	/*! 3D navigation modes only can be used*/
	GF_NAVIGATE_TYPE_3D
};

/*! navigation modes - non-VRML ones are simply blaxxun contact ones*/
enum
{
	/*! no navigation*/
	GF_NAVIGATE_NONE = 0,
	/*! 3D navigation modes*/
	/*! walk navigation*/
	GF_NAVIGATE_WALK,
	/*! fly navigation*/
	GF_NAVIGATE_FLY,
	/*! pan navigation*/
	GF_NAVIGATE_PAN,
	/*! game navigation*/
	GF_NAVIGATE_GAME,
	/*! slide navigation, for 2D and 3D*/
	GF_NAVIGATE_SLIDE,
	/*! all modes below disable collision detection & gravity in 3D*/
	/*examine navigation, for 2D and 3D */
	GF_NAVIGATE_EXAMINE,
	/*! orbit navigation - 3D only*/
	GF_NAVIGATE_ORBIT,
	/*! QT-VR like navigation - 3D only*/
	GF_NAVIGATE_VR,
};

/*! collision flags*/
enum
{
	/*! no collision*/
	GF_COLLISION_NONE,
	/*! regular collision*/
	GF_COLLISION_NORMAL,
	/*! collision with camera displacement*/
	GF_COLLISION_DISPLACEMENT,
};

/*! TextTexturing settings*/
enum
{
	/*! text drawn as texture in 3D mode, regular in 2D mode*/
	GF_TEXTURE_TEXT_DEFAULT = 0,
	/*! text never drawn as texture*/
	GF_TEXTURE_TEXT_NEVER,
	/*! text always drawn*/
	GF_TEXTURE_TEXT_ALWAYS
};

/*! Normal drawing settings*/
enum
{
	/*! normals never drawn*/
	GF_NORMALS_NONE = 0,
	/*! normals drawn per face (at barycenter)*/
	GF_NORMALS_FACE,
	/*! normals drawn per vertex*/
	GF_NORMALS_VERTEX
};


/*! Back-face culling mode*/
enum
{
	/*! backface culling disabled*/
	GF_BACK_CULL_OFF = 0,
	/*! backface culliong enabled*/
	GF_BACK_CULL_ON,
	/*! backface culling enabled also for transparent meshes*/
	GF_BACK_CULL_ALPHA,
};

/*! 2D drawing mode*/
enum
{
	/*! defer draw mode (only modified parts of the canvas are drawn)*/
	GF_DRAW_MODE_DEFER=0,
	/*! immediate draw mode (canvas always redrawn)*/
	GF_DRAW_MODE_IMMEDIATE,
	/*! defer debug draw mode (only modified parts of the canvas are drawn, the rest of the canvas is erased)*/
	GF_DRAW_MODE_DEFER_DEBUG,
};

/*! compositor options*/
typedef enum
{
	/*! set/get antialias flag (value: one of the AntiAlias enum) - may be ignored in OpenGL mode depending on graphic cards*/
	GF_OPT_ANTIALIAS  =0,
	/*! set/get fast mode (value: boolean) */
	GF_OPT_HIGHSPEED,
	/*! set/get fullscreen flag (value: boolean) */
	GF_OPT_FULLSCREEN,
	/*! reset top-level transform to original (value: boolean)*/
	GF_OPT_ORIGINAL_VIEW,
	/*! overrides BIFS size info for simple AV - this is not recommended since
	it will resize the window to the size of the biggest texture (thus some elements
	may be lost)*/
	GF_OPT_OVERRIDE_SIZE,
	/*! set / get audio volume (value is intensity between 0 and 100) */
	GF_OPT_AUDIO_VOLUME,
	/*! set / get audio pan (value is pan between 0 (all left) and 100(all right) )*/
	GF_OPT_AUDIO_PAN,
	/*! set / get audio mute*/
	GF_OPT_AUDIO_MUTE,
	/*! get javascript flag (no set, depends on compil) - value: boolean, true if JS enabled in build*/
	GF_OPT_HAS_JAVASCRIPT,
	/*! get selectable stream flag (no set) - value: boolean, true if audio/video/subtitle stream selection is
	possible with content (if an MPEG-4 scene description is not present). Use regular OD browsing to get streams*/
	GF_OPT_CAN_SELECT_STREAMS,
	/*! set/get control interaction, OR'ed combination of interaction flags*/
	GF_OPT_INTERACTION_LEVEL,
	/*! set display window visible / get show/hide state*/
	GF_OPT_VISIBLE,
	/*! set freeze display on/off / get freeze state freeze_display prevents any screen updates
	needed when output driver uses direct video memory access*/
	GF_OPT_FREEZE_DISPLAY,
	/*! Returns 1 if file playback is considered as done (all streams finished, no active time sensors
	and no user interactions in the scene)*/
	GF_OPT_IS_FINISHED,
	/*! Returns 1 if file timeline is considered as done (all streams finished, no active time sensors)*/
	GF_OPT_IS_OVER,
	/*! set/get aspect ratio (value: one of AspectRatio enum) */
	GF_OPT_ASPECT_RATIO,
	/*! send a redraw message (SetOption only): all graphics info (display list, vectorial path) is
	recomputed, and textures are reloaded in HW*/
	GF_OPT_REFRESH,
	/*! set/get stress mode (value: boolean) - in stress mode a GF_OPT_FORCE_REDRAW is emulated at each frame*/
	GF_OPT_STRESS_MODE,
	/*! get/set bounding volume drawing (value: one of the above option)*/
	GF_OPT_DRAW_BOUNDS,
	/*! get/set texture text option - when enabled and usable (that depends on content), text is first rendered
	to a texture and only the texture is drawn, rather than drawing all the text each time (CPU intensive)*/
	GF_OPT_TEXTURE_TEXT,
	/*! reload config file (set only), including drivers. Plugins configs are not reloaded*/
	GF_OPT_RELOAD_CONFIG,
	/*! get: returns whether the content enable navigation and if it's 2D or 3D.
	set: reset viewpoint (whatever value is given)*/
	GF_OPT_NAVIGATION_TYPE,
	/*! get current navigation mode - set navigation mode if allowed by content - this is not a resident
	option (eg not stored in cfg)*/
	GF_OPT_NAVIGATION,
	/*! get/set Play state - cf above states for set*/
	GF_OPT_PLAY_STATE,
	/*! get only: returns 1 if main addon is playing, 0 if regular scene is playing*/
	GF_OPT_MAIN_ADDON,
	/*! get/set bench mode - if enabled, video frames are drawn as soon as possible witthout checking synchronisation*/
	GF_OPT_VIDEO_BENCH,
	/*! get/set OpenGL force mode - returns error if OpenGL is not supported*/
	GF_OPT_USE_OPENGL,
	/*! set/get draw mode*/
	GF_OPT_DRAW_MODE,
	/*! set/get scalable zoom (value: boolean)*/
	GF_OPT_SCALABLE_ZOOM,
	/*! set/get YUV acceleration (value: boolean) */
	GF_OPT_YUV_HARDWARE,
	/*! get (set not supported yet) hardware YUV format (value: YUV 4CC) */
	GF_OPT_YUV_FORMAT,
	/*! max video cache size in kbytes*/
	GF_OPT_VIDEO_CACHE_SIZE,
	/*! max HTTP download rate in bits per second, 0 if no limit*/
	GF_OPT_HTTP_MAX_RATE,
	/*! set only (value: boolean). If set, the main audio mixer can no longer be reconfigured. */
	GF_OPT_FORCE_AUDIO_CONFIG,

	/*!		3D ONLY OPTIONS		*/
	/*! set/get raster outline flag (value: boolean) - when set, no vectorial outlining is done, only OpenGL raster outline*/
	GF_OPT_RASTER_OUTLINES,
	/*! set/get pow2 emulation flag (value: boolean) - when set, video textures with non power of 2 dimensions
	are emulated as pow2 by expanding the video buffer (image is not scaled). Otherwise the entire image
	is rescaled. This flag does not affect image textures, which are always rescaled*/
	GF_OPT_EMULATE_POW2,
	/*! get/set polygon antialiasing flag (value: boolean) (may be ugly with some cards)*/
	GF_OPT_POLYGON_ANTIALIAS,
	/*! get/set wireframe flag (value: cf above) (may be ugly with some cards)*/
	GF_OPT_WIREFRAME,
	/*! get/set wireframe flag (value: cf above) (may be ugly with some cards)*/
	GF_OPT_NORMALS,
	/*! disable backface culling*/
	GF_OPT_BACK_CULL,
	/*! get/set RECT Ext flag (value: boolean) - when set, GL rectangular texture extension is not used
	(but NPO2 texturing is if available)*/
	GF_OPT_NO_RECT_TEXTURE,
	/*! set/get headlight (value: boolean)*/
	GF_OPT_HEADLIGHT,
	/*! set/get collision (value: cf above)*/
	GF_OPT_COLLISION,
	/*! set/get gravity*/
	GF_OPT_GRAVITY,
	/*! get the number of offscreen views in stereo mode, or 1 if no offscreen stereo views are available*/
	GF_OPT_NUM_STEREO_VIEWS,
	/*! set the mode of display of HEVC multiview videos, 0 to display the two views/layers and 1 to display just the first view/layer*/
	GF_OPT_MULTIVIEW_MODE,
	/*! get orientation sensors flag, true if sensors are activated false if not*/
	GF_OPT_ORIENTATION_SENSORS_ACTIVE,
} GF_CompositorOption;


/*! sets compositor run-time option
\param compositor the target compositor
\param type the target option type
\param value the target option value
\return error if any
*/
GF_Err gf_sc_set_option(GF_Compositor *compositor, GF_CompositorOption type, u32 value);

/*! gets compositor run-time option.
\param compositor the target compositor
\param type the target option type
\return the option value
*/
u32 gf_sc_get_option(GF_Compositor *compositor, GF_CompositorOption type);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_COMPOSITOR_H_*/

