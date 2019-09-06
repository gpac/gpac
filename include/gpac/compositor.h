/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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
@{
 */
	

/*include scene graph API*/
#include <gpac/scenegraph.h>
/*GF_User and GF_Terminal */
#include <gpac/user.h>
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

/*! maps screen coordinates to bifs 2D coordinates for the current zoom/pan settings. The input coordinate X and Y are
 point coordinates in the display expressed in BIFS-like fashion (0,0) at center of display and Y increasing from bottom to top

\param compositor the target compositor
\param X horizontal point coordinate
\param Y vertical point coordinate
\param bifsX set to the scene horizontal coordinate of the point
\param bifsY set to the scene vertical coordinate of the point
*/
void gf_sc_map_point(GF_Compositor *compositor, s32 X, s32 Y, Fixed *bifsX, Fixed *bifsY);

/*! sets user options. Options are as defined in user.h
\param compositor the target compositor
\param type the target option type
\param value the target option value
\return error if any
*/
GF_Err gf_sc_set_option(GF_Compositor *compositor, u32 type, u32 value);
/*! gets user options. Options are as defined in user.h
\param compositor the target compositor
\param type the target option type
\return the option value
*/
u32 gf_sc_get_option(GF_Compositor *compositor, u32 type);

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

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_COMPOSITOR_H_*/

