/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2006-2019
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

#ifndef _GF_LASER_H_
#define _GF_LASER_H_

#ifdef __cplusplus
extern "C" {
#endif


/*!
\file <gpac/laser.h>
\brief MPEG-4 LASeR encoding and decoding.
*/
	
/*!
\addtogroup laser_grp MPEG-4 LASER
\ingroup mpeg4sys_grp
\brief MPEG-4 LASeR encoding and decoding

This section documents the LASeR encoding and decoding functions of the GPAC framework. For scene graph documentation, check scenegraph.h

@{
 */
	
#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_LASER

/*for LASeRConfig*/
#include <gpac/mpeg4_odf.h>

/*! a LASeR codec*/
typedef struct __tag_laser_codec GF_LASeRCodec;


/*! creates a new LASeR decoder
\param scenegraph the scenegraph on which the decoder operates
\return a newly allocated LASeR decoder*/
GF_LASeRCodec *gf_laser_decoder_new(GF_SceneGraph *scenegraph);
/*! destroys a LASeR decoder
\param codec the target LASeR decoder
*/
void gf_laser_decoder_del(GF_LASeRCodec *codec);

/*! sets the scene time. Scene time is the real clock of the bifs stream in secs
\param codec the target LASeR decoder
\param GetSceneTime the scene clock query callback function
\param st_cbk opaque data for the callback function
*/
void gf_laser_decoder_set_clock(GF_LASeRCodec *codec, Double (*GetSceneTime)(void *st_cbk), void *st_cbk );

/*! sets up a stream
\param codec the target LASeR decoder
\param ESID the ESID of the stream
\param DecoderSpecificInfo the decoder configuration data of the LASeR stream
\param DecoderSpecificInfoLength the size in bytes of the decoder configuration data
\return error if any
*/
GF_Err gf_laser_decoder_configure_stream(GF_LASeRCodec *codec, u16 ESID, u8 *DecoderSpecificInfo, u32 DecoderSpecificInfoLength);
/*! removes a stream
\param codec the target LASeR decoder
\param ESID the ESID of the stream
\return error if any
*/
GF_Err gf_laser_decoder_remove_stream(GF_LASeRCodec *codec, u16 ESID);

/*! decodes a LASeR AU and applies it to the graph (non-memory mode only)
\param codec the target LASeR decoder
\param ESID the ESID of the stream
\param data the access unit payload
\param data_length the access unit size in bytes
\return error if any
*/
GF_Err gf_laser_decode_au(GF_LASeRCodec *codec, u16 ESID, const u8 *data, u32 data_length);

/*! decodes a LASeR AU in memory - fills the command list with the content of the AU - cf scenegraph_vrml.h for commands usage
\param codec the target LASeR decoder
\param ESID the ESID of the stream
\param data the access unit payload
\param data_length the access unit size in bytes
\param com_list a list to be filled with decoded commands
\return error if any
*/
GF_Err gf_laser_decode_command_list(GF_LASeRCodec *codec, u16 ESID, u8 *data, u32 data_length, GF_List *com_list);

/*! checks if a LASeR decoder has associated conditionnals
 \param codec the target LASeR decoder
\return GF_TRUE if conditionnals are attached to this decoder
*/
Bool gf_laser_decode_has_conditionnals(GF_LASeRCodec *codec);

/*! creates a new LASeR encoder
\param scenegraph the scenegraph on which the encoder operates
\return a newly allocated LASeR encoder*/
GF_LASeRCodec *gf_laser_encoder_new(GF_SceneGraph *scenegraph);
/*! destroys a LASeR encoder
\param codec the target LASeR encoder
*/
void gf_laser_encoder_del(GF_LASeRCodec *codec);
/*! sets up a destination stream
\param codec the target LASeR encoder
\param ESID the ID of the stream
\param cfg the LASeR configuration descriptor
\return error if any
*/
GF_Err gf_laser_encoder_new_stream(GF_LASeRCodec *codec, u16 ESID, GF_LASERConfig *cfg);

/*! encodes a list of commands for the given stream in the output buffer - data is dynamically allocated for output
\param codec the target LASeR encoder
\param ESID the ID of the stream
\param command_list a list of commands to encode
\param reset_encoding_context if GF_TRUE, resets all color and font tables of LASeR streams (used for clean RAPs)
\param out_data set to an allocated buffer containing the encoded AU - shall be destroyed by caller
\param out_data_length set to the size of the encoded AU
\return error if any
*/
GF_Err gf_laser_encode_au(GF_LASeRCodec *codec, u16 ESID, GF_List *command_list, Bool reset_encoding_context, u8 **out_data, u32 *out_data_length);
/*! gets a stream encoded config description
\param codec the target LASeR encoder
\param ESID the ID of the stream
\param out_data set to an allocated buffer containing the encoded configuration - shall be destroyed by caller
\param out_data_length set to the size of the encoded configuration
\return error if any
*/
GF_Err gf_laser_encoder_get_config(GF_LASeRCodec *codec, u16 ESID, u8 **out_data, u32 *out_data_length);

/*! encodes current graph as a scene replace
\param codec the target LASeR encoder
\param out_data set to an allocated buffer containing the encoded scene replace AU - shall be destroyed by caller
\param out_data_length set to the size of the encoded AU
\return error if any
*/
GF_Err gf_laser_encoder_get_rap(GF_LASeRCodec *codec, u8 **out_data, u32 *out_data_length);


#endif /*GPAC_DISABLE_LASER*/

/*! @} */

#ifdef __cplusplus
}
#endif

#endif

