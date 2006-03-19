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

#ifndef _GF_RENDERER_H_
#define _GF_RENDERER_H_

#ifdef __cplusplus
extern "C" {
#endif


/*include scene graph API*/
#include <gpac/scenegraph.h>
/*GF_User and GF_Terminal */
#include <gpac/user.h>
/*frame buffer definition*/
#include <gpac/color.h>

typedef struct __tag_base_renderer GF_Renderer;

/*creates default renderer 
if self_threaded, video renderer uses a dedicated thread, otherwise visual rendering is done by the user
audio renderer always runs in its own thread if enabled
term may be NULL, in which case InputSensors won't be enabled
*/
GF_Renderer *gf_sr_new(GF_User *user_interface, Bool self_threaded, GF_Terminal *term);
void gf_sr_del(GF_Renderer *sr);

/*sets simulation frame rate*/
void gf_sr_set_fps(GF_Renderer *sr, Double fps);

/*set the root scene graph of the renderer - if NULL remove current and reset simulation time*/
GF_Err gf_sr_set_scene(GF_Renderer *sr, GF_SceneGraph *scene_graph);

/*if the renderer doesn't use its own thread for visual, this will perform a render pass*/
Bool gf_sr_render_frame(GF_Renderer *sr);

/*inits rendering info for the node - shall be called for all nodes the parent system doesn't handle*/
void gf_sr_on_node_init(GF_Renderer *sr, GF_Node *node);

/*notify the given node has been modified. The renderer filters object to decide whether the scene graph has to be 
traversed or not- if object is NULL, this means complete traversing of the graph is requested (use carefully since it
can be a time consuming operation)*/
void gf_sr_invalidate(GF_Renderer *sr, GF_Node *byObj);

/*return the renderer time - this is the time every time line syncs on*/
u32 gf_sr_get_clock(GF_Renderer *sr);


/*locks/unlocks the visual scene rendering - modification of the scene tree shall only happen when scene renderer is locked*/
void gf_sr_lock(GF_Renderer *sr, Bool doLock);
/*locks/unlocks the audio scene rendering - this is needed whenever an audio object changes config on the fly*/
void gf_sr_lock_audio(GF_Renderer *sr, Bool doLock);

/*notify user input - only GF_EventMouse and GF_EventKey are checked, depending on the renderer used...*/
void gf_sr_user_event(GF_Renderer *sr, GF_Event *event);

/*maps screen coordinates to bifs 2D coordinates for the current zoom/pan settings
X and Y are point coordinates in the display expressed in BIFS-like fashion (0,0) at center of 
display and Y increasing from bottom to top*/
void gf_sr_map_point(GF_Renderer *sr, s32 X, s32 Y, Fixed *bifsX, Fixed *bifsY);

/*signal the size of the display area has been changed*/
GF_Err gf_sr_size_changed(GF_Renderer *sr, u32 NewWidth, u32 NewHeight);

/*set/get user options - options are as defined in user.h*/
GF_Err gf_sr_set_option(GF_Renderer *sr, u32 type, u32 value);
u32 gf_sr_get_option(GF_Renderer *sr, u32 type);

/*returns current FPS
if @absoluteFPS is set, the return value is the absolute framerate, eg NbFrameCount/NbTimeSpent regardless of
whether a frame has been drawn or not, which means the FPS returned can be much greater than the renderer FPS
if @absoluteFPS is not set, the return value is the FPS taking into account not drawn frames (eg, less than or equal to
renderer FPS)
*/
Double gf_sr_get_fps(GF_Renderer *sr, Bool absoluteFPS);


/*user-define management: this is used for instant visual rendering of the scene graph, 
for exporting or authoring tools preview. User is responsible for calling render when desired and shall also maintain
scene timing*/

/*force render tick*/
void gf_sr_render(GF_Renderer *sr);
/*gets screen buffer - this locks the scene graph too until released is called*/
GF_Err gf_sr_get_screen_buffer(GF_Renderer *sr, GF_VideoSurface *framebuffer);
/*releases screen buffer and unlocks graph*/
GF_Err gf_sr_release_screen_buffer(GF_Renderer *sr, GF_VideoSurface *framebuffer);

/*renders one frame*/
void gf_sr_simulation_tick(GF_Renderer *sr);

/*forces graphics cache recompute*/
void gf_sr_reset_graphics(GF_Renderer *sr);

/*picks a node (may return NULL) - coords are given in OS client system coordinate, as in UserInput*/
GF_Node *gf_sr_pick_node(GF_Renderer *sr, s32 X, s32 Y);

/*get viewpoints/viewports for main scene - idx is 1-based, and if greater than number of viewpoints return GF_EOS*/
GF_Err gf_sr_get_viewpoint(GF_Renderer *sr, u32 viewpoint_idx, const char **outName, Bool *is_bound);
/*set viewpoints/viewports for main scene given its name - idx is 1-based, or 0 to retrieve by viewpoint name
if only one viewpoint is present in the scene, this will bind/unbind it*/
GF_Err gf_sr_set_viewpoint(GF_Renderer *sr, u32 viewpoint_idx, const char *viewpoint_name);

/*render subscene root node. rs is the current traverse stack
this is needed to handle graph metrics changes between scenes...*/
void gf_sr_render_inline(GF_Renderer *sr, GF_Node *inline_root, void *rs);

/*set outupt size*/
GF_Err gf_sr_set_size(GF_Renderer *sr, u32 NewWidth, u32 NewHeight);

/*returns total length of audio hardware buffer in ms, 0 if no audio*/
u32 gf_sr_get_audio_buffer_length(GF_Renderer *sr);

/*add/remove extra scene from renderer (extra scenes are OSDs or any other scene graphs not directly
usable by main scene, like 3GP text streams*/
void gf_sr_register_extra_graph(GF_Renderer *sr, GF_SceneGraph *extra_scene, Bool do_remove);

/*gets audio hardware delay*/
u32 gf_sr_get_audio_delay(GF_Renderer *sr);

/*returns total length of audio hardware buffer in ms, 0 if no audio*/
u32 gf_sr_get_audio_buffer_length(GF_Renderer *sr);

void *gf_sr_get_visual_renderer(GF_Renderer *sr);

#ifdef __cplusplus
}
#endif

#endif	/*_GF_RENDERER_H_*/

