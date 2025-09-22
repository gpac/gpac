/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2019
 *					All rights reserved
 *
 *  This file is part of GPAC
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


#ifndef _GF_SCENEENGINE_H_
#define _GF_SCENEENGINE_H_

#ifdef __cplusplus
extern "C" {
#endif


/*!
\file <gpac/scene_engine.h>
\brief Live scene encoding engine with RAP generation support.
*/
	
/*!
\addtogroup seng Scene Engine
\ingroup scene_grp
\brief Live scene encoding engine with RAP generation support.

This section documents the live scene encoding tools of GPAC.

@{
 */

#include <gpac/scene_manager.h>

#ifndef GPAC_DISABLE_SENG

/*! scene encoding engine object*/
typedef struct __tag_scene_engine GF_SceneEngine;

/*! callback function prototype for scene engine*/
typedef void (*gf_seng_callback)(void *udta, u16 ESID, u8 *data, u32 size, u64 ts);

/*! creates a scene engine
\param calling_object is the calling object on which call back will be called
\param inputContext is the name of a scene file (bt, xmt or mp4) to initialize the coding context
\param load_type is the preferred loader type for the content (e.g. SVG vs DIMS)
\param dump_path is the path where scenes are dumped
\param embed_resources indicates if images and scripts should be encoded inline with the content
\return e scene engine object
*/
GF_SceneEngine *gf_seng_init(void *calling_object, char *inputContext, u32 load_type, char *dump_path, Bool embed_resources);

/*! get the number of streams in the scene
\param seng the target scene engine
\return the number of streams
*/
u32 gf_seng_get_stream_count(GF_SceneEngine *seng);

/*! gets carousel nformation for a stream
\param seng the target scene engine
\param ESID ID of the stream to query
\param carousel_period set to the carousel_period in millisenconds
\param aggregate_on_es_id set to the target carousel stream ID
\return error if any
 */
GF_Err gf_seng_get_stream_carousel_info(GF_SceneEngine *seng, u16 ESID, u32 *carousel_period, u16 *aggregate_on_es_id);

/*! gets the stream decoder configuration
\param seng the target scene engine
\param idx stream index
\param ESID set to the stream ID
\param config set to the encoded BIFS config (memory is not allocated)
\param config_len length of the buffer
\param streamType pointer to get stream type
\param objectType pointer to get object type
\param timeScale pointer to get time scale
\return error if any
*/
GF_Err gf_seng_get_stream_config(GF_SceneEngine *seng, u32 idx, u16 *ESID, const u8 **config, u32 *config_len, u32 *streamType, u32 *objectType, u32 *timeScale);

/*! Encodes the AU context which is not encoded when calling BENC_EncodeAUFromString/File. This should be called after Aggregate.
\param seng the target scene engine
\param callback pointer on a callback function to get the result of the coding the AU using the current context
\return error if any
*/
GF_Err gf_seng_encode_context(GF_SceneEngine *seng, gf_seng_callback callback);

/*! Encodes an AU from file
\param seng the target scene engine
\param ESID target streams when no indication is present in the file (eg, no atES_ID )
\param disable_aggregation do not aggregate the access unit on its target stream
\param auFile name of a file containing a description for an access unit (BT or XMT)
\param callback pointer on a callback function to get the result of the coding the AU using the current context
\return error if any
*/
GF_Err gf_seng_encode_from_file(GF_SceneEngine *seng, u16 ESID, Bool disable_aggregation, char *auFile, gf_seng_callback callback);

/*! Encodes an AU from string
\param seng the target scene engine
\param ESID target streams when no indication is present in the file (eg, no atES_ID )
\param disable_aggregation do not aggregate the access unit on its target stream
\param auString a char string to encode (must one or several complete nodes in BT
\param callback pointer on a callback function to get the result of the coding the AU using the current context
\return error if any
*/
GF_Err gf_seng_encode_from_string(GF_SceneEngine *seng, u16 ESID, Bool disable_aggregation, char *auString, gf_seng_callback callback);


/*! saves the live context to a file. Use BENC_AggregateCurrentContext before to save an aggregated context.
\param seng the target scene engine
\param ctxFileName name of the file to save the current state of the BIFS scene to
\return error if any
*/
GF_Err gf_seng_save_context(GF_SceneEngine *seng, char *ctxFileName);

/*! marks the stream as carrying its own "rap" in the first AU of the stream
\param seng the target scene engine
\param ESID stream ID
\param onESID set stream aggragation on to the specified stream, or off if onESID is 0
\return error if any
*/
GF_Err gf_seng_enable_aggregation(GF_SceneEngine *seng, u16 ESID, u16 onESID);

/*! aggregates the current context of the seng, creates a scene replace
\param seng the target scene engine
\param ESID stream ID. If not 0, only aggregate commands for this stream; otherwise aggregates for the all streams
\return error if any
*/
GF_Err gf_seng_aggregate_context(GF_SceneEngine *seng, u16 ESID);

/*! destroys the scene engine
\param seng the target scene engine
*/
void gf_seng_terminate(GF_SceneEngine *seng);

/*! encodes the IOD for this scene engine into Base64
\param seng the target scene engine
\return the base64 encoded IOD, shall be freed by the caller
 */
char *gf_seng_get_base64_iod(GF_SceneEngine *seng);

/*! returns the IOD for a scene engine
\param seng the target scene engine
\return the IOD object - shall be destroyed by caller
*/
GF_Descriptor *gf_seng_get_iod(GF_SceneEngine *seng);

#endif /*GPAC_DISABLE_SENG*/

/*! @} */

#ifdef __cplusplus
}
#endif // __cplusplus


#endif	/*_GF_SCENEENGINE_H_*/

