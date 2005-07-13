/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
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


#ifndef _GF_BIFSENGINE_H_
#define _GF_BIFSENGINE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

#ifndef GPAC_READ_ONLY
	
typedef struct __tag_bifs_engine GF_BifsEngine;

/**
 * @calling_object is the calling object on which call back will be called
 * @inputContext is the name of a scene file (bt, xmt or mp4) to initialize the coding context
 * @extension for future extensions, NULL for now
 *
 * must be called only one time (by process calling the DLL) before other calls
 */
GF_BifsEngine *gf_beng_init(void *calling_object, char *inputContext, void *extension);


/**
 * @beng, pointer to the GF_BifsEngine returned by BENC_Init
 * @config: pointer to the encoded BIFS config (memory is not allocated)
 * @config_len: length of the buffer
 *
 * must be called after BENC_Init
 */
void gf_beng_get_stream_config(GF_BifsEngine *beng, char **config, u32 *config_len);

/**
 * Encodes the AU context which is not encoded when calling BENC_EncodeAUFromString/File
 * Should be called after Aggregate.
 *
 * @beng, pointer to the GF_BifsEngine returned by BENC_Init
 * @AUCallback, pointer on a callback function to get the result of the coding the AU using the current context
 * @extension for future extensions, NULL for now
 *
 */
GF_Err gf_beng_encode_context(GF_BifsEngine *beng, GF_Err (*AUCallback)(void *, char *data, u32 size, u32 ts), void *extension);

/**
 * @beng, pointer to the GF_BifsEngine returned by BENC_Init
 * @auFile, name of a file containing a description for an access unit (BT or XMT)
 * @AUCallback, pointer on a callback function to get the result of the coding the AU using the current context
 * @extension for future extensions, NULL for now
 *
 */
GF_Err gf_beng_encode_from_file(GF_BifsEngine *beng, char *auFile, GF_Err (*AUCallback)(void *, char *data, u32 size, u32 ts), void *extension);

/**
 * @beng, pointer to the GF_BifsEngine returned by BENC_Init
 * @auString, a char string to encode (must one or several complete nodes in BT
 * @AUCallback, pointer on a callback function to get the result of the coding the AU using the current context
 * @extension for future extensions, NULL for now
 *
 */
GF_Err gf_beng_encode_from_string(GF_BifsEngine *beng, char *auString, GF_Err (*AUCallback)(void *, char *data, u32 size, u32 ts), void *extension);

/**
 * @beng, pointer to the GF_BifsEngine returned by BENC_Init
 * @ctxFileName, name of the file to save the current state of the BIFS scene to
 * @extension for future extensions, NULL for now
 *
 * save the current context of the beng.
 * if you want to save an aggregate context, use BENC_AggregateCurrentContext before
 *
 */
GF_Err gf_beng_save_context(GF_BifsEngine *beng, char *ctxFileName, void *extension);

/**
 * @beng, pointer to the GF_BifsEngine returned by BENC_Init
 * @extension for future extensions, NULL for now
 *
 * aggregates the current context of the beng, creates a scene replace
 *
 */
GF_Err gf_beng_aggregate_context(GF_BifsEngine *beng, void *extension);

/**
 * @beng, pointer to the GF_BifsEngine returned by BENC_Init
 *
 * release the memory used by this beng, no more call on the beng should happen after this
 *
 */
void gf_beng_terminate(GF_BifsEngine *beng);


#endif


#ifdef __cplusplus
}
#endif // __cplusplus


#endif	/*_GF_BIFSENGINE_H_*/

