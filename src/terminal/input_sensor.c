/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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

#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/utf.h>
#include <gpac/nodes_x3d.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_VRML

#include "input_sensor.h"

#if GPAC_HTK_DEMO

void HTK_Init(const char *htk_path);
void HTK_Close(void);
void HTK_SetDictionary(char *dicofile);
void HTK_DoDetection(void);
const char *HTK_GetWord();
int HTK_GetWordIndex();
float HTK_GetWordScore();
void StartHTK(ISPriv *is_dec);

static u32 htk_num_users = 0;
#endif


/*
				input sensor decoder(s) handling
*/

GF_Err gf_isdec_configure(GF_BaseDecoder *plug, GF_Scene *scene, Bool is_remote)
{
	ISPriv *is = (ISPriv *)plug->privateStack;
	/*we can only deal with encoded content (for now)*/
	if (!scene->scene_codec) return GF_NOT_SUPPORTED;
	is->scene = scene;
	is->is_local = !is_remote;
	return GF_OK;
}


static void add_field(ISPriv *priv, u32 fieldType, const char *fieldName)
{
	GF_FieldInfo *field = (GF_FieldInfo *) gf_malloc(sizeof(GF_FieldInfo));
	memset(field, 0, sizeof(GF_FieldInfo));
	field->fieldType = fieldType;
	field->far_ptr = gf_sg_vrml_field_pointer_new(fieldType);
	field->name = (const char *) fieldName;
	field->fieldIndex = gf_list_count(priv->ddf);
	gf_list_add(priv->ddf, field);
}

static void isdev_add_field(GF_InputSensorDevice *dev, u32 fieldType, const char *fieldName)
{
	ISPriv *is = (ISPriv *)dev->input_decoder->privateStack;
	add_field(is, fieldType, fieldName);
}

static void isdev_dispatch_frame(struct __input_device *dev, u8 *data, u32 data_len)
{
	GF_SLHeader slh;
	GF_Codec*cod;
	u32 i;
	ISPriv *priv = (ISPriv *)dev->input_decoder->privateStack;

	memset(&slh, 0, sizeof(GF_SLHeader));
	slh.accessUnitStartFlag = slh.accessUnitEndFlag = 1;
	slh.compositionTimeStampFlag = 1;
	/*note we could use an exact TS but it's not needed: since the input is generated locally
	we want it to be decoded as soon as possible, thus using 0 emulates permanent seeking on
	InputSensor stream, hence forces input frame resync*/
	slh.compositionTimeStamp = 0;

	/*get all decoders and send frame*/
	i=0;
	while ((cod = (GF_Codec*)gf_list_enum(priv->scene->root_od->term->input_streams, &i))) {
		ISPriv *is = (ISPriv *)cod->decio->privateStack;
		if (is->type==priv->type) {
			GF_Channel *ch = (GF_Channel *)gf_list_get(cod->inChannels, 0);
			gf_es_receive_sl_packet(ch->service, ch, (char *)data, data_len, &slh, GF_OK);
		}
	}
}

static GF_Err IS_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	GF_BitStream *bs;
	u32 len, size, i;
	char devName[255];
	u16 termSeq[20];

	ISPriv *is = (ISPriv *)plug->privateStack;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->dataLength) return GF_NON_COMPLIANT_BITSTREAM;

	/*no more than one UI stream per object*/
	if (is->ES_ID) return GF_NOT_SUPPORTED;
	is->ES_ID = esd->ESID;
	/*parse config*/
	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	len = gf_bs_read_int(bs, 8);
	for (i=0; i<len; i++) {
		devName[i] = gf_bs_read_int(bs, 8);
	}
	devName[i] = 0;
	is->type = gf_crc_32(devName, len);
	size = len + 1;

	if (!stricmp(devName, "KeySensor")) {
		is->type = IS_KeySensor;
		add_field(is, GF_SG_VRML_SFINT32, "keyPressed");
		add_field(is, GF_SG_VRML_SFINT32, "keyReleased");
		add_field(is, GF_SG_VRML_SFINT32, "actionKeyPressed");
		add_field(is, GF_SG_VRML_SFINT32, "actionKeyReleased");
		add_field(is, GF_SG_VRML_SFBOOL, "shiftKeyPressed");
		add_field(is, GF_SG_VRML_SFBOOL, "controlKeyPressed");
		add_field(is, GF_SG_VRML_SFBOOL, "altKeyPressed");

	} else if (!stricmp(devName, "StringSensor")) {
		is->type = IS_StringSensor;
		add_field(is, GF_SG_VRML_SFSTRING, "enteredText");
		add_field(is, GF_SG_VRML_SFSTRING, "finalText");

		is->termChar = '\r';
		is->delChar = '\b';

		/*get escape chars if any specified*/
		if (size<esd->decoderConfig->decoderSpecificInfo->dataLength) {
			const char *src = esd->decoderConfig->decoderSpecificInfo->data + size;
			gf_utf8_mbstowcs(termSeq, esd->decoderConfig->decoderSpecificInfo->dataLength - size, &src);
			is->termChar = termSeq[0];
			is->delChar = termSeq[1];
		}
	} else if (!stricmp(devName, "Mouse")) {
		is->type = IS_Mouse;
		add_field(is, GF_SG_VRML_SFVEC2F, "position");
		add_field(is, GF_SG_VRML_SFBOOL, "leftButtonDown");
		add_field(is, GF_SG_VRML_SFBOOL, "middleButtonDown");
		add_field(is, GF_SG_VRML_SFBOOL, "rightButtonDown");
		add_field(is, GF_SG_VRML_SFFLOAT, "wheel");
	}
	else {
		GF_InputSensorDevice *ifce;
		/*not found, check all modules*/
		u32 plugCount = gf_modules_get_count(is->scene->root_od->term->user->modules);
		for (i = 0; i < plugCount ; i++) {
			ifce = (GF_InputSensorDevice *) gf_modules_load_interface(is->scene->root_od->term->user->modules, i, GF_INPUT_DEVICE_INTERFACE);
			if (!ifce) continue;
			ifce->input_decoder = plug;
			if (ifce->RegisterDevice && ifce->RegisterDevice(ifce, devName, bs, isdev_add_field) ) {
				is->io_dev = ifce;
				break;
			}
			gf_modules_close_interface((GF_BaseInterface *) ifce);
		}
		if (!is->io_dev) return GF_NOT_SUPPORTED;

		is->io_dev->DispatchFrame = isdev_dispatch_frame;
		plug->module_name = is->io_dev->module_name;
		plug->author_name = is->io_dev->author_name;
	}

	gf_bs_del(bs);
	return GF_OK;
}

static GF_Err IS_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	ISPriv *is = (ISPriv *)plug->privateStack;
	is->ES_ID = 0;
	return GF_OK;
}

static GF_Err IS_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *capability)
{
	capability->cap.valueInt = 0;
	return GF_OK;
}

static GF_Err IS_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

static GF_Err IS_ProcessData(GF_SceneDecoder *plug, const char *inBuffer, u32 inBufferLength,
                             u16 ES_ID, u32 AU_time, u32 mmlevel)
{
	u32 i, j, count;
	Double scene_time;
	GF_BitStream *bs;
	GF_FieldInfo *field;
	ISStack *st;
	ISPriv *priv = (ISPriv *)plug->privateStack;
	GF_Err e = GF_OK;

	/*decode data frame except if local stringSensor*/
	bs = gf_bs_new(inBuffer, inBufferLength, GF_BITSTREAM_READ);
	i=0;
	while ((field = (GF_FieldInfo *)gf_list_enum(priv->ddf, &i))) {
		/*store present flag in eventIn for command skip - this is an ugly hack but it works since DDF don't have event types*/
		field->eventType = gf_bs_read_int(bs, 1);
		/*parse val ourselves (we don't want to depend on bifs codec)*/
		if (field->eventType) {
			switch (field->fieldType) {
			case GF_SG_VRML_SFBOOL:
				* ((SFBool *) field->far_ptr) = (SFBool) gf_bs_read_int(bs, 1);
				break;
			case GF_SG_VRML_SFFLOAT:
				*((SFFloat *)field->far_ptr) = FLT2FIX( gf_bs_read_float(bs) );
				break;
			case GF_SG_VRML_SFINT32:
				*((SFInt32 *)field->far_ptr) = (s32) gf_bs_read_int(bs, 32);
				break;
			case GF_SG_VRML_SFTIME:
				*((SFTime *)field->far_ptr) = gf_bs_read_double(bs);
				break;
			case GF_SG_VRML_SFVEC2F:
				((SFVec2f *)field->far_ptr)->x = FLT2FIX( gf_bs_read_float(bs) );
				((SFVec2f *)field->far_ptr)->y = FLT2FIX( gf_bs_read_float(bs) );
				break;
			case GF_SG_VRML_SFVEC3F:
				((SFVec3f *)field->far_ptr)->x = FLT2FIX( gf_bs_read_float(bs) );
				((SFVec3f *)field->far_ptr)->y = FLT2FIX( gf_bs_read_float(bs) );
				((SFVec3f *)field->far_ptr)->z = FLT2FIX( gf_bs_read_float(bs) );
				break;
			case GF_SG_VRML_SFCOLOR:
				((SFColor *)field->far_ptr)->red = FLT2FIX( gf_bs_read_float(bs) );
				((SFColor *)field->far_ptr)->green = FLT2FIX( gf_bs_read_float(bs) );
				((SFColor *)field->far_ptr)->blue = FLT2FIX( gf_bs_read_float(bs) );
				break;
			case GF_SG_VRML_SFVEC4F:
			case GF_SG_VRML_SFROTATION:
				((SFRotation *)field->far_ptr)->x = FLT2FIX( gf_bs_read_float(bs) );
				((SFRotation *)field->far_ptr)->y = FLT2FIX( gf_bs_read_float(bs) );
				((SFRotation *)field->far_ptr)->z = FLT2FIX( gf_bs_read_float(bs) );
				((SFRotation *)field->far_ptr)->q = FLT2FIX( gf_bs_read_float(bs) );
				break;

			case GF_SG_VRML_SFSTRING:
			{
				u32 size, length;
				size = gf_bs_read_int(bs, 5);
				length = gf_bs_read_int(bs, size);
				if (gf_bs_available(bs) < length) return GF_NON_COMPLIANT_BITSTREAM;

				if ( ((SFString *)field->far_ptr)->buffer ) gf_free( ((SFString *)field->far_ptr)->buffer);
				((SFString *)field->far_ptr)->buffer = (char*)gf_malloc(sizeof(char)*(length+1));
				memset(((SFString *)field->far_ptr)->buffer , 0, length+1);
				for (j=0; j<length; j++) {
					((SFString *)field->far_ptr)->buffer[j] = gf_bs_read_int(bs, 8);
				}
			}
			break;
			}
		}
	}
	gf_bs_del(bs);

	/*special case for StringSensor in local mode: lookup for special chars*/
	if ((priv->type == IS_StringSensor) && priv->is_local) {
		char tmp_utf8[5000];
		const unsigned short *ptr;
		u32 len;
		GF_FieldInfo *field1 = (GF_FieldInfo *)gf_list_get(priv->ddf, 0);
		GF_FieldInfo *field2 = (GF_FieldInfo *)gf_list_get(priv->ddf, 1);
		SFString *inText = (SFString *) field1->far_ptr;
		SFString *outText = (SFString *) field2->far_ptr;

		field1->eventType = field2->eventType = 0;
		priv->enteredText[priv->text_len] = (short) '\0';

		len = (u32) gf_utf8_wcslen(priv->enteredText);
		if (len && (priv->enteredText[len-1] == priv->termChar)) {
			ptr = priv->enteredText;
			len = (u32) gf_utf8_wcstombs(tmp_utf8, 5000, &ptr);
			if (outText->buffer) gf_free(outText->buffer);
			outText->buffer = (char*)gf_malloc(sizeof(char) * (len));
			memcpy(outText->buffer, tmp_utf8, sizeof(char) * len-1);
			outText->buffer[len-1] = 0;
			if (inText->buffer) gf_free(inText->buffer);
			inText->buffer = NULL;
			priv->text_len = 0;

			field1->eventType = field2->eventType = 1;
		} else {
			if (priv->delChar) {
				/*remove chars*/
				if ((len>1) && (priv->enteredText[len-1] == priv->delChar)) {
					priv->enteredText[len-1] = (short) '\0';
					len--;
					if (len) {
						priv->enteredText[len-1] = (short) '\0';
						len--;
					}
				}
			}
			priv->text_len = len;
			ptr = priv->enteredText;
			len = (u32) gf_utf8_wcstombs(tmp_utf8, 5000, &ptr);
			if (inText->buffer) gf_free(inText->buffer);
			inText->buffer = (char*)gf_malloc(sizeof(char) * (len+1));
			memcpy(inText->buffer, tmp_utf8, sizeof(char) * len);
			inText->buffer[len] = 0;
			field1->eventType = 1;
		}
	}

	gf_term_lock_compositor(priv->scene->root_od->term, 1);

	/*apply it*/
	i=0;
	while ((st = (ISStack*)gf_list_enum(priv->is_nodes, &i))) {
		assert(st->is);
		assert(st->mo);
		if (!st->is->enabled) continue;

		count = gf_list_count(st->is->buffer.commandList);
		scene_time = gf_scene_get_time(priv->scene);
		for (j=0; j<count; j++) {
			GF_Command *com = (GF_Command *)gf_list_get(st->is->buffer.commandList, j);
			GF_FieldInfo *field = (GF_FieldInfo *)gf_list_get(priv->ddf, j);
			GF_CommandField *info = (GF_CommandField *)gf_list_get(com->command_fields, 0);
			if (info && field && field->eventType) {
				gf_sg_vrml_field_copy(info->field_ptr, field->far_ptr, field->fieldType);
				gf_sg_command_apply(priv->scene->graph, com, scene_time);
			}
		}
	}
	gf_term_lock_compositor(priv->scene->root_od->term, 0);
	return e;
}

void gf_isdec_del(GF_BaseDecoder *plug)
{
	ISPriv *priv = (ISPriv *)plug->privateStack;
	gf_list_del(priv->is_nodes);

	while (gf_list_count(priv->ddf)) {
		GF_FieldInfo *fi = (GF_FieldInfo *)gf_list_get(priv->ddf, 0);
		gf_list_rem(priv->ddf, 0);
		gf_sg_vrml_field_pointer_del(fi->far_ptr, fi->fieldType);
		gf_free(fi);
	}
	gf_list_del(priv->ddf);
	gf_free(priv);
	gf_free(plug);
}


GF_BaseDecoder *gf_isdec_new(GF_ESD *esd, u32 PL)
{
	ISPriv *priv;
	GF_SceneDecoder *tmp;

	tmp = (GF_SceneDecoder*) gf_malloc(sizeof(GF_SceneDecoder));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_SceneDecoder));

	priv = (ISPriv *) gf_malloc(sizeof(ISPriv));
	memset(priv, 0, sizeof(ISPriv));

	priv->is_nodes = gf_list_new();
	priv->ddf = gf_list_new();

	tmp->privateStack = priv;

	tmp->AttachStream = IS_AttachStream;
	tmp->DetachStream = IS_DetachStream;
	tmp->GetCapabilities = IS_GetCapabilities;
	tmp->SetCapabilities = IS_SetCapabilities;
	tmp->ProcessData = IS_ProcessData;
	/*we don't use this...*/
	tmp->AttachScene = NULL;

	GF_REGISTER_MODULE_INTERFACE(tmp, GF_SCENE_DECODER_INTERFACE, "GPAC InputSensor Decoder", "gpac distribution")
	return (GF_BaseDecoder *) tmp;
}

/*
				input sensor node handling
*/
static void IS_Unregister(GF_Node *node, ISStack *st)
{
	u32 i;
	GF_ObjectManager *odm;
	ISPriv *is_dec;

	gf_mo_unregister(node, st->mo);

	odm = st->mo->odm;
	if (!odm) return;

	assert(odm->codec && (odm->codec->type == GF_STREAM_INTERACT));

	/*get IS dec*/
	is_dec = (ISPriv*)odm->codec->decio->privateStack;
	for (i=0; i<gf_list_count(is_dec->is_nodes); i++) {
		ISStack *tmp = (ISStack *)gf_list_get(is_dec->is_nodes, i);
		if (tmp == st) {
			gf_list_rem(is_dec->is_nodes, i);
			i--;
		}
	}
	/*stop stream*/
	if (st->mo->num_open) gf_mo_stop(st->mo);
	st->mo = NULL;
	if (st->registered) {
		st->registered = 0;
		if (is_dec->io_dev && is_dec->io_dev->Stop) is_dec->io_dev->Stop(is_dec->io_dev);
	}
}

static void IS_Register(GF_Node *n)
{
	GF_ObjectManager *odm;
	ISPriv *is_dec;
	u32 i;
	ISStack *st = (ISStack *)gf_node_get_private(n);
	odm = st->mo->odm;
	if (!odm) return;

	assert(odm->codec && (odm->codec->type == GF_STREAM_INTERACT));

	/*get IS dec*/
	is_dec = (ISPriv*)odm->codec->decio->privateStack;
	if ( gf_list_find(is_dec->is_nodes, st) == -1 )
		gf_list_add(is_dec->is_nodes, st);

	/*start stream*/
	gf_mo_play(st->mo, 0, -1, 0);

	gf_term_unqueue_node_traverse(odm->term, n);

	/*we want at least one sensor enabled*/
	i=0;
	while ((st = gf_list_enum(is_dec->is_nodes, &i))) {
		if (st->is->enabled) {
			st->registered = 1;
			if (is_dec->io_dev && is_dec->io_dev->Start) is_dec->io_dev->Start(is_dec->io_dev);
			break;
		}
	}

}

static void TraverseInputSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	ISStack *st = (ISStack*)gf_node_get_private(node);
	M_InputSensor *is = (M_InputSensor *)node;

	if (is_destroy) {
		GF_Scene *scene;
		if (st->registered) IS_Unregister(node, st);
		scene = (GF_Scene*)gf_sg_get_private(gf_node_get_graph(node));
		gf_term_unqueue_node_traverse(scene->root_od->term, node);
		gf_free(st);
	} else {
		/*get decoder object */
		if (!st->mo) st->mo = gf_mo_register(node, &is->url, 0, 0);
		/*register with decoder*/
		if (st->mo && !st->registered) IS_Register(node);
	}
}


void InitInputSensor(GF_Scene *scene, GF_Node *node)
{
	ISStack *stack;
	GF_SAFEALLOC(stack, ISStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_INTERACT, ("[Terminal] Failed to allocate input sensor stack\n"));
		return;
	}
	stack->is = (M_InputSensor *) node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseInputSensor);
	gf_term_queue_node_traverse(scene->root_od->term, node);
}

/*check only URL changes*/
void InputSensorModified(GF_Node *node)
{
	GF_MediaObject *mo;
	ISStack *st = (ISStack *)gf_node_get_private(node);

	mo = gf_mo_register(node, &st->is->url, 0, 0);
	if ((mo!=st->mo) || !st->registered) {
		if (mo!=st->mo) {
			if (st->mo) IS_Unregister(node, st);
			st->mo = mo;
		}
		if (st->is->enabled)
			IS_Register(node);
		else
			return;
	} else if (!st->is->enabled) {
		IS_Unregister(node, st);
		return;
	}
}



/*
				input sensor DDF generations (user interface)
*/
GF_EXPORT
void gf_term_mouse_input(GF_Terminal *term, GF_EventMouse *event)
{
	s32 X, Y;
	u32 left_but_down, middle_but_down, right_but_down;
	SFFloat wheel_pos;
	u32 i;
	GF_Codec *cod;
	GF_BitStream *bs;
	GF_SLHeader slh;
	char *buf;
	u32 buf_size;
	Fixed bX, bY;

	if (!term || !gf_list_count(term->input_streams)) return;

	X = event->x;
	Y = event->y;
	left_but_down = middle_but_down = right_but_down = 0;
	wheel_pos = 0;
	switch (event->type) {
	case GF_EVENT_MOUSEDOWN:
		if (event->button==GF_MOUSE_RIGHT) right_but_down = 2;
		else if (event->button==GF_MOUSE_MIDDLE) middle_but_down = 2;
		else if (event->button==GF_MOUSE_LEFT) left_but_down = 2;
		break;
	case GF_EVENT_MOUSEUP:
		if (event->button==GF_MOUSE_RIGHT) right_but_down = 1;
		else if (event->button==GF_MOUSE_MIDDLE) middle_but_down = 1;
		else if (event->button==GF_MOUSE_LEFT) left_but_down = 1;
		break;
	case GF_EVENT_MOUSEWHEEL:
		wheel_pos = event->wheel_pos;
		break;
	case GF_EVENT_MOUSEMOVE:
		break;
	default:
		return;
	}

	/*get BIFS coordinates*/
	gf_sc_map_point(term->compositor, X, Y, &bX, &bY);
	bX = gf_divfix(bX, term->compositor->scale_x);
	bY = gf_divfix(bY, term->compositor->scale_y);

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	/*If wheel is specified disable X and Y (bug from MS wheel handling)*/
	if (wheel_pos) {
		gf_bs_write_int(bs, 0, 1);
	} else {
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, FIX2FLT(bX));
		gf_bs_write_float(bs, FIX2FLT(bY));
	}
	gf_bs_write_int(bs, left_but_down ? 1 : 0, 1);
	if (left_but_down) gf_bs_write_int(bs, left_but_down-1, 1);
	gf_bs_write_int(bs, middle_but_down ? 1 : 0, 1);
	if (middle_but_down) gf_bs_write_int(bs, middle_but_down-1, 1);
	gf_bs_write_int(bs, right_but_down ? 1 : 0, 1);
	if (right_but_down) gf_bs_write_int(bs, right_but_down-1, 1);
	if (wheel_pos==0) {
		gf_bs_write_int(bs, 0, 1);
	} else {
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, FIX2FLT(wheel_pos) );
	}

	gf_bs_align(bs);
	gf_bs_get_content(bs, &buf, &buf_size);
	gf_bs_del(bs);

	memset(&slh, 0, sizeof(GF_SLHeader));
	slh.accessUnitStartFlag = slh.accessUnitEndFlag = 1;
	slh.compositionTimeStampFlag = 1;
	/*note we could use an exact TS but it's not needed: since the input is generated locally
	we want it to be decoded as soon as possible, thus using 0 emulates permanent seeking on
	InputSensor stream, hence forces input frame resync*/
	slh.compositionTimeStamp = 0;

	/*get all IS Mouse decoders and send frame*/
	i=0;
	while ((cod = (GF_Codec*)gf_list_enum(term->input_streams, &i))) {
		ISPriv *is = (ISPriv *)cod->decio->privateStack;
		if (is->type==IS_Mouse) {
			GF_Channel *ch = (GF_Channel *)gf_list_get(cod->inChannels, 0);
			gf_es_receive_sl_packet(ch->service, ch, buf, buf_size, &slh, GF_OK);
		}
	}
	gf_free(buf);
}

GF_EXPORT
Bool gf_term_keyboard_input(GF_Terminal *term, u32 key_code, u32 hw_code, Bool isKeyUp)
{
	u32 i;
	GF_BitStream *bs;
	GF_SLHeader slh;
	char *buf;
#ifndef GPAC_DISABLE_X3D
	X_KeySensor *n;
#endif
	u32 buf_size;
	u32 actionKey = 0;
	u32 shiftKeyDown, controlKeyDown, altKeyDown;
	GF_Codec *cod;
	s32 keyPressed, keyReleased, actionKeyPressed, actionKeyReleased;

	if (!term || (!gf_list_count(term->input_streams) && !gf_list_count(term->x3d_sensors)) ) return 0;

	memset(&slh, 0, sizeof(GF_SLHeader));
	slh.accessUnitStartFlag = slh.accessUnitEndFlag = 1;
	slh.compositionTimeStampFlag = 1;
	/*cf above*/
	slh.compositionTimeStamp = 0;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	shiftKeyDown = controlKeyDown = altKeyDown = 0;
	keyPressed = keyReleased = actionKeyPressed = actionKeyReleased = 0;
	/*key-sensor codes*/
	switch (key_code) {
	case GF_KEY_F1:
		actionKey = 1;
		break;
	case GF_KEY_F2:
		actionKey = 2;
		break;
	case GF_KEY_F3:
		actionKey = 3;
		break;
	case GF_KEY_F4:
		actionKey = 4;
		break;
	case GF_KEY_F5:
		actionKey = 5;
		break;
	case GF_KEY_F6:
		actionKey = 6;
		break;
	case GF_KEY_F7:
		actionKey = 7;
		break;
	case GF_KEY_F8:
		actionKey = 8;
		break;
	case GF_KEY_F9:
		actionKey = 9;
		break;
	case GF_KEY_F10:
		actionKey = 10;
		break;
	case GF_KEY_F11:
		actionKey = 11;
		break;
	case GF_KEY_F12:
		actionKey = 12;
		break;
	case GF_KEY_HOME:
		actionKey = 13;
		break;
	case GF_KEY_END:
		actionKey = 14;
		break;
	case GF_KEY_PAGEUP:
		actionKey = 15;
		break;
	case GF_KEY_PAGEDOWN:
		actionKey = 16;
		break;
	case GF_KEY_UP:
		actionKey = 17;
		break;
	case GF_KEY_DOWN:
		actionKey = 18;
		break;
	case GF_KEY_LEFT:
		actionKey = 19;
		break;
	case GF_KEY_RIGHT:
		actionKey = 20;
		break;
	case GF_KEY_SHIFT:
		actionKey = 0;
		shiftKeyDown = isKeyUp ? 1 : 2;
		break;
	case GF_KEY_CONTROL:
		actionKey = 0;
		controlKeyDown = isKeyUp ? 1 : 2;
		break;
	case GF_KEY_ALT:
		actionKey = 0;
		altKeyDown = isKeyUp ? 1 : 2;
		break;

	default:
		actionKey = 0;
		break;
	}
	if (actionKey) {
		if (isKeyUp)
			actionKeyReleased = actionKey;
		else
			actionKeyPressed = actionKey;
	} else {
		/*handle numeric pad*/
		if ((key_code>=GF_KEY_0) && (key_code<=GF_KEY_9) ) {
			key_code = key_code + 0x30 - GF_KEY_0;
		}
		else
			key_code = hw_code;

		if (isKeyUp) keyReleased = key_code;
		else keyPressed = key_code;
	}

	gf_bs_write_int(bs, keyPressed ? 1 : 0, 1);
	if (keyPressed) gf_bs_write_int(bs, keyPressed, 32);
	gf_bs_write_int(bs, keyReleased ? 1 : 0, 1);
	if (keyReleased) gf_bs_write_int(bs, keyReleased, 32);
	gf_bs_write_int(bs, actionKeyPressed ? 1 : 0, 1);
	if (actionKeyPressed) gf_bs_write_int(bs, actionKeyPressed, 32);
	gf_bs_write_int(bs, actionKeyReleased ? 1 : 0, 1);
	if (actionKeyReleased) gf_bs_write_int(bs, actionKeyReleased, 32);
	gf_bs_write_int(bs, shiftKeyDown ? 1 : 0 , 1);
	if (shiftKeyDown) gf_bs_write_int(bs, shiftKeyDown-1, 1);
	gf_bs_write_int(bs, controlKeyDown ? 1 : 0 , 1);
	if (controlKeyDown) gf_bs_write_int(bs, controlKeyDown-1, 1);
	gf_bs_write_int(bs, altKeyDown ? 1 : 0 , 1);
	if (altKeyDown) gf_bs_write_int(bs, altKeyDown, 1);

	gf_bs_align(bs);
	gf_bs_get_content(bs, &buf, &buf_size);
	gf_bs_del(bs);

	/*get all IS keySensor decoders and send frame*/
	i=0;
	while ((cod = (GF_Codec*)gf_list_enum(term->input_streams, &i))) {
		ISPriv *is = (ISPriv *)cod->decio->privateStack;
		if (is->type==IS_KeySensor) {
//			GF_Channel *ch = gf_list_get(cod->inChannels, 0);
//			gf_es_receive_sl_packet(ch->service, ch, buf, buf_size, &slh, GF_OK);

			IS_ProcessData((GF_SceneDecoder*)cod->decio, buf, buf_size, 0, 0, 0);
		}
	}
	gf_free(buf);

#ifndef GPAC_DISABLE_X3D
	i=0;
	while ((n = (X_KeySensor*)gf_list_enum(term->x3d_sensors, &i))) {
		u16 tc[2];
		u32 len;
		char szStr[10];
		const unsigned short *ptr;
		if (gf_node_get_tag((GF_Node*)n) != TAG_X3D_KeySensor) continue;
		if (!n->enabled) return 0;

		if (keyPressed) {
			if (n->keyPress.buffer) gf_free(n->keyPress.buffer);
			tc[0] = keyPressed;
			tc[1] = 0;
			ptr = tc;
			len = (u32) gf_utf8_wcstombs(szStr, 10, &ptr);
			n->keyPress.buffer = (char*)gf_malloc(sizeof(char) * (len+1));
			memcpy(n->keyPress.buffer, szStr, sizeof(char) * len);
			n->keyPress.buffer[len] = 0;
			gf_node_event_out_str((GF_Node *)n, "keyPress");
		}
		if (keyReleased) {
			if (n->keyRelease.buffer) gf_free(n->keyRelease.buffer);
			tc[0] = keyReleased;
			tc[1] = 0;
			ptr = tc;
			len = (u32) gf_utf8_wcstombs(szStr, 10, &ptr);
			n->keyRelease.buffer = (char*)gf_malloc(sizeof(char) * (len+1));
			memcpy(n->keyRelease.buffer, szStr, sizeof(char) * len);
			n->keyRelease.buffer[len] = 0;
			gf_node_event_out_str((GF_Node *)n, "keyRelease");
		}
		if (actionKeyPressed) {
			n->actionKeyPress = actionKeyPressed;
			gf_node_event_out_str((GF_Node *)n, "actionKeyPress");
		}
		if (actionKeyReleased) {
			n->actionKeyRelease = actionKeyReleased;
			gf_node_event_out_str((GF_Node *)n, "actionKeyRelease");
		}
		if (shiftKeyDown) {
			n->shiftKey = (shiftKeyDown-1) ? 1 : 0;
			gf_node_event_out_str((GF_Node *)n, "shiftKey");
		}
		if (controlKeyDown) {
			n->controlKey = (controlKeyDown-1) ? 1 : 0;
			gf_node_event_out_str((GF_Node *)n, "controlKey");
		}
		if (altKeyDown) {
			n->altKey= (altKeyDown-1) ? 1 : 0;
			gf_node_event_out_str((GF_Node *)n, "altKey");
		}
		if (keyPressed || actionKeyPressed || (shiftKeyDown-1) || (controlKeyDown-1) || (altKeyDown-1)) {
			if (!n->isActive) {
				n->isActive = 1;
				gf_node_event_out_str((GF_Node *)n, "isActive");
			}
		} else if (n->isActive) {
			n->isActive = 0;
			gf_node_event_out_str((GF_Node *)n, "isActive");
		}
	}
#endif
	/*with KeySensor, we don't know if the key will be consumed or not, assume it is*/
	return 1;
}

GF_EXPORT
void gf_term_string_input(GF_Terminal *term, u32 character)
{
	u32 i;
	GF_BitStream *bs;
	GF_SLHeader slh;
#ifndef GPAC_DISABLE_X3D
	X_StringSensor *n;
#endif
	GF_Codec *cod;
	char *buf;
	u32 buf_size;

	if (!character || !term) return;
	if (!gf_list_count(term->input_streams) && !gf_list_count(term->x3d_sensors)) return;

	memset(&slh, 0, sizeof(GF_SLHeader));
	slh.accessUnitStartFlag = slh.accessUnitEndFlag = 1;
	slh.compositionTimeStampFlag = 1;
	/*cf above*/
	slh.compositionTimeStamp = 0;

	/*get all IS StringSensor decoders and send frame*/
	i=0;
	while ((cod = (GF_Codec*)gf_list_enum(term->input_streams, &i))) {
		ISPriv *is = (ISPriv *)cod->decio->privateStack;
		if (is->type==IS_StringSensor) {

//			GF_Channel *ch = (GF_Channel *)gf_list_get(cod->inChannels, 0);
			is->enteredText[is->text_len] = character;
			is->text_len += 1;

			/*write empty DDF*/
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_int(bs, 0, 1);
			gf_bs_write_int(bs, 0, 1);
			gf_bs_align(bs);
			gf_bs_get_content(bs, &buf, &buf_size);
			gf_bs_del(bs);

//			gf_es_receive_sl_packet(ch->service, ch, buf, buf_size, &slh, GF_OK);
			IS_ProcessData((GF_SceneDecoder*)cod->decio, buf, buf_size, 0, 0, 0);

			gf_free(buf);
		}
	}


#ifndef GPAC_DISABLE_X3D
	/*get all X3D StringSensors*/
	i=0;
	while ((n = (X_StringSensor*)gf_list_enum(term->x3d_sensors, &i))) {
		StringSensorStack *st;
		char szStr[5000];
		const unsigned short *ptr;
		u32 len;
		if (gf_node_get_tag((GF_Node*)n) != TAG_X3D_StringSensor) continue;
		if (!n->enabled) continue;

		st = (StringSensorStack *) gf_node_get_private((GF_Node *)n);

		if (character=='\b') {
			if (n->deletionAllowed && st->text_len) {
				st->text_len -= 1;
				st->enteredText[st->text_len] = 0;
				ptr = st->enteredText;
				len = (u32) gf_utf8_wcstombs(szStr, 10, &ptr);
				if (n->enteredText.buffer) gf_free(n->enteredText.buffer);
				szStr[len] = 0;
				n->enteredText.buffer = gf_strdup(szStr);
				gf_node_event_out_str((GF_Node *)n, "enteredText");
			}
		} else if (character=='\r') {
			if (n->finalText.buffer) gf_free(n->finalText.buffer);
			n->finalText.buffer = n->enteredText.buffer;
			n->enteredText.buffer = gf_strdup("");
			st->text_len = 0;
			gf_node_event_out_str((GF_Node *)n, "enteredText");
			gf_node_event_out_str((GF_Node *)n, "finalText");
		} else {
			st->enteredText[st->text_len] = character;
			st->text_len += 1;
			st->enteredText[st->text_len] = 0;
			ptr = st->enteredText;
			len = (u32) gf_utf8_wcstombs(szStr, 10, &ptr);
			if (n->enteredText.buffer) gf_free(n->enteredText.buffer);
			szStr[len] = 0;
			n->enteredText.buffer = gf_strdup(szStr);
			gf_node_event_out_str((GF_Node *)n, "enteredText");
		}
	}
#endif
}

#ifndef GPAC_DISABLE_X3D

void DestroyKeySensor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_Terminal *term = (GF_Terminal *) gf_node_get_private(node);
		gf_list_del_item(term->x3d_sensors, node);
	}
}
void InitKeySensor(GF_Scene *scene, GF_Node *node)
{
	gf_node_set_private(node, scene->root_od->term);
	gf_node_set_callback_function(node, DestroyKeySensor);
	gf_list_add(scene->root_od->term->x3d_sensors, node);
}

void DestroyStringSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		StringSensorStack *st = (StringSensorStack *) gf_node_get_private(node);
		gf_list_del_item(st->term->x3d_sensors, node);
		gf_free(st);
	}
}
void InitStringSensor(GF_Scene *scene, GF_Node *node)
{
	StringSensorStack*st;
	GF_SAFEALLOC(st, StringSensorStack)
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_INTERACT, ("[Terminal] Failed to allocate string sensor stack\n"));
		return;
	}
	st->term = scene->root_od->term;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyStringSensor);
	gf_list_add(scene->root_od->term->x3d_sensors, node);
}

#endif /*GPAC_DISABLE_X3D*/

#else

GF_EXPORT
void gf_term_mouse_input(GF_Terminal *term, GF_EventMouse *event)
{
}
GF_EXPORT
Bool gf_term_keyboard_input(GF_Terminal *term, u32 key_code, u32 hw_code, Bool isKeyUp)
{
	return GF_TRUE;
}
GF_EXPORT
void gf_term_string_input(GF_Terminal *term, u32 character)
{
}

#endif /*GPAC_DISABLE_VRML*/
