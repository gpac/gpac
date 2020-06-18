/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS codec sub-project
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

#ifndef _GF_BIFS_H_
#define _GF_BIFS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/bifs.h>
\brief MPEG-4 BIFS encoding and decoding.
*/
	
/*!
\addtogroup bifs_grp MPEG-4 BIFS
\ingroup mpeg4sys_grp
\brief MPEG-4 BIFS  encoding and decoding

This section documents the BIFS encoding and decoding of the GPAC framework. For scene graph documentation, check scenegraph.h
@{
 */
	

#include <gpac/nodes_mpeg4.h>
/*for BIFSConfig*/
#include <gpac/mpeg4_odf.h>

#ifndef GPAC_DISABLE_BIFS

/*! BIFS decoder*/
typedef struct __tag_bifs_dec GF_BifsDecoder;

/*! creates a new BIFS decoder
\param scenegraph the scene graph on which the decoder operates
\param command_dec if set, the decoder will only work in memory mode (creating commands for the graph)
 otherwise the decoder will always apply commands while decoding them
\return a new BIFS decoder, NULL if error
*/
GF_BifsDecoder *gf_bifs_decoder_new(GF_SceneGraph *scenegraph, Bool command_dec);
/*! destroys a BIFS decoder
\param codec the decoder to destroy
*/
void gf_bifs_decoder_del(GF_BifsDecoder *codec);

/*! sets up a new BIFS stream
\param codec the BIFS decoder to use
\param ESID the ESID of the stream
\param DecoderSpecificInfo the decoder config of the stream
\param DecoderSpecificInfoLength the size of the decoder config of the stream
\param objectTypeIndication the object type indication for this stream
\return error if any
*/
GF_Err gf_bifs_decoder_configure_stream(GF_BifsDecoder *codec, u16 ESID, u8 *DecoderSpecificInfo, u32 DecoderSpecificInfoLength, u32 objectTypeIndication);

/*! decodes a BIFS access unit and applies it to the graph (non-memory mode only)
\param codec the BIFS decoder to use
\param ESID the ESID of the stream
\param data the compressed BIFS access unit
\param data_length the size of the compressed BIFS access unit
\param ts_offset the time in seconds of the access unit since the start of the scene
\return error if any
*/
GF_Err gf_bifs_decode_au(GF_BifsDecoder *codec, u16 ESID, const u8 *data, u32 data_length, Double ts_offset);

/*! decodes a BIFS access unit in memory - cf scenegraph_vrml.h for commands usage
\param codec the BIFS decoder to use
\param ESID the ESID of the stream
\param data the compressed BIFS access unit
\param data_length the size of the compressed BIFS access unit
\param com_list list object to retrieve the list of decoded commands
\return error if any
*/
GF_Err gf_bifs_decode_command_list(GF_BifsDecoder *codec, u16 ESID, u8 *data, u32 data_length, GF_List *com_list);

/*! checks if conditionnals have been defined for the decoder
\param codec the BIFS decoder to check
\return GF_TRUE if the decoder has conditionnal nodes attached
*/
Bool gf_bifs_decode_has_conditionnals(GF_BifsDecoder *codec);

#ifndef GPAC_DISABLE_BIFS_ENC
/*! BIFS encoder*/
typedef struct __tag_bifs_enc GF_BifsEncoder;

/*! creates a new BIFS encoder
\param graph the scene graph being encoded
\return a new BIFS encoder, NULL if error
*/
GF_BifsEncoder *gf_bifs_encoder_new(GF_SceneGraph *graph);
/*! destroys a BIFS encoder
\param codec the BIFS encoder
*/
void gf_bifs_encoder_del(GF_BifsEncoder *codec);
/*! configures a new destination stream
\param codec the BIFS encoder to use
\param ESID the ESID of the created stream
\param cfg object describing the configuration of the stream
\param encodeNames if set, names of nodes with IDs will be encoded (as strings)
\param has_predictive set to GF_TRUE if the stream uses predictive field coding
\return error if any
*/
GF_Err gf_bifs_encoder_new_stream(GF_BifsEncoder *codec, u16 ESID, GF_BIFSConfig *cfg, Bool encodeNames, Bool has_predictive);
/*! encodes a list of commands for the given stream in the output buffer - data is dynamically allocated for output
the scenegraph used is the one described in SceneReplace command, hence scalable streams shall be encoded in time order
\param codec the BIFS encoder to use
\param ESID the ESID of the stream owning the command list
\param command_list the list of commands to encode
\param out_data set to the allocated output buffer, to be freed by the caller
\param out_data_length set to the size of the allocated output buffer
\return error if any
*/
GF_Err gf_bifs_encode_au(GF_BifsEncoder *codec, u16 ESID, GF_List *command_list, u8 **out_data, u32 *out_data_length);
/*! returns the encoded decoder configuration of a given stream
\param codec the BIFS encoder to use
\param ESID the ESID of the stream
\param out_data set to the allocated output buffer, to be freed by the caller
\param out_data_length set to the size of the allocated output buffer
\return error if any
*/
GF_Err gf_bifs_encoder_get_config(GF_BifsEncoder *codec, u16 ESID, u8 **out_data, u32 *out_data_length);
/*! returns the BIFS version used by the encoder for a given stream
\param codec the BIFS encoder to use
\param ESID the ESID of the stream
\return BIFS version used
*/
u8 gf_bifs_encoder_get_version(GF_BifsEncoder *codec, u16 ESID);

/*! encodes current graph as a scene replacement command
\param codec the BIFS encoder to use
\param out_data set to the allocated output buffer, to be freed by the caller
\param out_data_length set to the size of the allocated output buffer
\return error if any
*/
GF_Err gf_bifs_encoder_get_rap(GF_BifsEncoder *codec, u8 **out_data, u32 *out_data_length);
/*! sets the URL of the source content being encoded, used for opening relative path files in some nodes attributes
\param codec the BIFS encoder to use
\param src_url URL (parent directory or source file) of content being encoded
\return error if any
*/
GF_Err gf_bifs_encoder_set_source_url(GF_BifsEncoder *codec, const char *src_url);

#endif /*GPAC_DISABLE_BIFS_ENC*/

#endif /*GPAC_DISABLE_BIFS*/


/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_BIFS_H_*/

