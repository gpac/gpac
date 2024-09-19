/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau, Jean Le Feuvre
 *			Copyright (c) Motion Spell, Telecom ParisTech 2024
 *			All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include <gpac/xml.h>
#include <gpac/internal/isomedia_dev.h> //GF_EventMessageBox

#define XML_SCAN_INT(_fmt, _value)	\
	{\
	if (strstr(att->value, "0x")) { u32 __i; sscanf(att->value+2, "%x", &__i); _value = __i; }\
	else if (strstr(att->value, "0X")) { u32 __i; sscanf(att->value+2, "%X", &__i); _value = __i; }\
	else sscanf(att->value, _fmt, &_value); \
	}\

// SCTE-35 XML parsing ----------------------------------------------

static void xml_scte35_parse_splice_time(GF_XMLNode *root, GF_BitStream *bs)
{
	u32 i = 0, j = 0;
	GF_XMLNode *node = NULL;
	GF_XMLAttribute *att = NULL;

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &j))) {
		if (!strcmp(att->name, "ptsTime")) {
			u64 ptsTime = 0;
			if (sscanf(att->value, LLU, &ptsTime) != 1)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Invalid value for ptsTime=\"%s\"\n", att->value));

			Bool time_specified_flag = GF_TRUE;
			gf_bs_write_int(bs, time_specified_flag, 1);
			if (time_specified_flag == GF_TRUE) {
				gf_bs_write_int(bs, 0xFF/*reserved*/, 6);
				gf_bs_write_long_int(bs, ptsTime, 33);
			} else {
				gf_bs_write_int(bs, 0xFF/*reserved*/, 7);
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in SpliceTime\n", att->name));
		}
	}

	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown node \"%s\" in SpliceTime\n", node->name));
	}
}

static void xml_scte35_parse_break_duration(GF_XMLNode *root, GF_BitStream *bs)
{
	u32 i = 0, j = 0;
	GF_XMLNode *node = NULL;
	GF_XMLAttribute *att = NULL;

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &j))) {
		if (!strcmp(att->name, "duration")) {
			u64 duration = 0;
			if (sscanf(att->value, LLU, &duration) != 1)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Invalid value for duration=\"%s\"\n", att->value));

			gf_bs_write_int(bs, 0/*auto_return*/, 1);
			gf_bs_write_int(bs, 0/*reserved*/, 6);
			gf_bs_write_long_int(bs, duration, 33);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in SpliceTime\n", att->name));
		}
	}

	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown node \"%s\" in SpliceTime\n", node->name));
	}
}

static void xml_scte35_parse_time_signal(GF_XMLNode *root, GF_BitStream *bs)
{
	u32 i = 0, j = 0;
	GF_XMLNode *node = NULL;
	GF_XMLAttribute *att = NULL;

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &j))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in TimeSignal\n", att->name));
	}

	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		if (!strcmp(node->name, "SpliceTime")) {
			xml_scte35_parse_splice_time(node, bs);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown node \"%s\" in TimeSignal\n", node->name));
		}
	}
}

static void xml_scte35_parse_segmentation_descriptor(GF_XMLNode *root, GF_BitStream *bs)
{
	u64 descriptor_loop_length_pos = gf_bs_get_position(bs);
	gf_bs_write_u16(bs, 0/*descriptor_loop_length*/); //placeholder
	//for (u16 i=0; i<descriptor_loop_length; i++) {
	gf_bs_write_u8(bs, 0x02/*splice_descriptor_tag*/);
	gf_bs_write_u8(bs, 0/*descriptor_length*/); //placeholder
	gf_bs_write_u32(bs, GF_4CC('C', 'U', 'E', 'I'));

	u32 i = 0, j = 0;
	GF_XMLNode *node = NULL;
	GF_XMLAttribute *att = NULL;
	int segmentationEventId;
	Bool segmentationEventCancelIndicator = GF_FALSE, webDeliveryAllowedFlag = GF_FALSE, noRegionalBlackoutFlag = GF_FALSE, archiveAllowedFlag = GF_FALSE;
	Bool segmentationEventIdComplianceIndicator = GF_FALSE;
	Bool programSegmentationFlag = GF_FALSE, segmentationDurationFlag = GF_FALSE, deliveryNotRestrictedFlag = GF_FALSE;
	u8 deviceRestrictions = 0;
	u8 segmentationTypeId = 0, segmentNum = 0, segmentsExpected = 0;
	u8 subSegmentNum = 0, subSegmentsExpected = 0;
	u64 segmentationDuration = 0;

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &j))) {
		if (!strcmp(att->name, "segmentationEventId")) {
			segmentationEventId = atoi(att->value);
		} else if (!strcmp(att->name, "segmentationEventCancelIndicator")) {
			segmentationEventCancelIndicator = atoi(att->value);
		} else if (!strcmp(att->name, "segmentationEventIdComplianceIndicator")) {
			segmentationEventIdComplianceIndicator = atoi(att->value);
		} else if (!strcmp(att->name, "webDeliveryAllowedFlag")) {
			webDeliveryAllowedFlag = atoi(att->value);
		} else if (!strcmp(att->name, "noRegionalBlackoutFlag")) {
			noRegionalBlackoutFlag = atoi(att->value);
		} else if (!strcmp(att->name, "archiveAllowedFlag")) {
			archiveAllowedFlag = atoi(att->value);
		} else if (!strcmp(att->name, "deviceRestrictions")) {
			deviceRestrictions = atoi(att->value);
		} else if (!strcmp(att->name, "segmentationTypeId")) {
			segmentationTypeId = atoi(att->value);
		} else if (!strcmp(att->name, "segmentNum")) {
			segmentNum = atoi(att->value);
		} else if (!strcmp(att->name, "segmentsExpected")) {
			segmentsExpected = atoi(att->value);
		} else if (!strcmp(att->name, "programSegmentationFlag")) {
			programSegmentationFlag = atoi(att->value);
		} else if (!strcmp(att->name, "segmentationDurationFlag")) {
			segmentationDurationFlag = atoi(att->value);
		} else if (!strcmp(att->name, "deliveryNotRestrictedFlag")) {
			deliveryNotRestrictedFlag = atoi(att->value);
		} else if (!strcmp(att->name, "segmentationDuration")) {
			segmentationDuration = atoi(att->value);
		} else if (!strcmp(att->name, "subSegmentNum")) {
			subSegmentNum = atoi(att->value);
		} else if (!strcmp(att->name, "subSegmentsExpected")) {
			subSegmentsExpected = atoi(att->value);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in SegmentationDescriptor\n", att->name));
		}
	}

	gf_bs_write_u32(bs, segmentationEventId);

	gf_bs_write_int(bs, segmentationEventCancelIndicator, 1);
	gf_bs_write_int(bs, segmentationEventIdComplianceIndicator, 1);
	gf_bs_write_int(bs, 0x3F/*reserved*/, 6);
	if (segmentationEventCancelIndicator == 0) {
		gf_bs_write_int(bs, programSegmentationFlag, 1);
		gf_bs_write_int(bs, segmentationDurationFlag, 1);
		gf_bs_write_int(bs, deliveryNotRestrictedFlag, 1);
		if (deliveryNotRestrictedFlag == 0) {
			gf_bs_write_int(bs, webDeliveryAllowedFlag, 1);
			gf_bs_write_int(bs, noRegionalBlackoutFlag, 1);
			gf_bs_write_int(bs, archiveAllowedFlag, 1);
			gf_bs_write_int(bs, deviceRestrictions, 2);
		} else {
			gf_bs_write_int(bs, 0x1F/*reserved*/, 5);
		}
		if (programSegmentationFlag == 0) {
			assert(0);
#if 0 //not implemented
			gf_bs_write_u8(bs, component_count);
			for (u8 i=0; i<component_count; i++) {
				gf_bs_write_u8(bs, componentTag);
				gf_bs_write_int(bs, 7, 0x7F/*reserved*/);
				gf_bs_write_u8(bs, componentTag);
				gf_bs_write_long_int(bs, ptsOffset, 33);
			}
#endif
		}
		if (segmentationDurationFlag == 1) {
			gf_bs_write_long_int(bs, segmentationDuration, 40);
		}

		gf_bs_write_u8(bs, 0/*u8 segmentation_upid_type*/);
		gf_bs_write_u8(bs, 0/*u8 segmentation_upid_length*/);

		gf_bs_write_u8(bs, segmentationTypeId);
		gf_bs_write_u8(bs, segmentNum);
		gf_bs_write_u8(bs, segmentsExpected);
		if (segmentationTypeId == 0x34 || segmentationTypeId == 0x30 || segmentationTypeId == 0x32
		 || segmentationTypeId == 0x36 || segmentationTypeId == 0x38 || segmentationTypeId == 0x3A
		 || segmentationTypeId == 0x44 || segmentationTypeId == 0x46) {
			gf_bs_write_u8(bs, subSegmentNum);
			gf_bs_write_u8(bs, subSegmentsExpected);
		}
	}

	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		if (!strcmp(node->name, "SegmentationUpid")) {
			GF_XMLAttribute *attx = NULL;
			while ((attx = (GF_XMLAttribute *)gf_list_enum(root->attributes, &j))) {
				if (!strcmp(attx->name, "segmentationUpidType")) {
				gf_bs_write_u8(bs, segmentationTypeId);
				gf_bs_write_u8(bs, segmentNum);
				gf_bs_write_u8(bs, segmentsExpected);
#if 0 //not implemented: seen at the end of TimeSignal
   <SegmentationDescriptor segmentationEventId="1207959603" segmentationEventCancelIndicator="0" segmentationEventIdComplianceIndicator="1"
    webDeliveryAllowedFlag="1" noRegionalBlackoutFlag="1" archiveAllowedFlag="1" deviceRestrictions="3"
	segmentationTypeId="53" segmentNum="0" segmentsExpected="0">
    <SegmentationUpid segmentationUpidType="8">000000003484D6D5</SegmentationUpid>
   </SegmentationDescriptor>
#endif
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in SegmentationUpid\n", attx->name));
				}
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown node \"%s\" in TimeSignal\n", node->name));
		}
	}

	// write descriptor_loop_length and descriptor_length
	{
		u64 pos = gf_bs_get_position(bs);
		int descriptor_length = pos - descriptor_loop_length_pos - 2;
		gf_bs_seek(bs, descriptor_loop_length_pos + 1);
		gf_bs_write_int(bs, descriptor_length, 8);   //descriptor_loop_length
		gf_bs_seek(bs, descriptor_loop_length_pos + 3);
		gf_bs_write_int(bs, descriptor_length-2, 8); //descriptor_length
		gf_bs_seek(bs, pos);
	}
}

static void xml_scte35_parse_splice_insert(GF_XMLNode *root, GF_BitStream *bs)
{
	u32 i = 0, j = 0;
	GF_XMLNode *node = NULL;
	GF_XMLAttribute *att = NULL;

	int spliceEventId = 0, spliceEventCancelIndicator = 0, outOfNetworkIndicator = 0, segmentationEventId = 0, programSpliceFlag = 0, durationFlag = 0, spliceImmediateFlag = 0;

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &j))) {
		if (!strcmp(att->name, "spliceEventId")) {
			spliceEventId = atoi(att->value);
		} else if (!strcmp(att->name, "spliceEventCancelIndicator")) {
			spliceEventCancelIndicator = atoi(att->value);
		} else if (!strcmp(att->name, "outOfNetworkIndicator")) {
			outOfNetworkIndicator = atoi(att->value);
		} else if (!strcmp(att->name, "segmentationEventId")) {
			segmentationEventId = atoi(att->value);
		} else if (!strcmp(att->name, "programSpliceFlag")) {
			programSpliceFlag = atoi(att->value);
		} else if (!strcmp(att->name, "durationFlag")) {
			durationFlag = atoi(att->value);
		} else if (!strcmp(att->name, "spliceImmediateFlag")) {
			spliceImmediateFlag = atoi(att->value);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in SpliceInsert\n", att->name));
		}
	}

	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		if (!strcmp(node->name, "SpliceTime")) {
			xml_scte35_parse_splice_time(node, bs);
		} else if (!strcmp(node->name, "BreakDuration")) {
			xml_scte35_parse_break_duration(node, bs);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown node \"%s\" in SpliceInsert\n", node->name));
		}
	}

#if 0 //not implemented
	u64 splice_time = 0;
	gf_bs_write_u32(bs, spliceEventId);
	gf_bs_write_int(bs, 1, spliceEventCancelIndicator);
	/*reserved = */gf_bs_write_int(bs, 7, 0x7F);
	if (spliceEventCancelIndicator == 0) {
		gf_bs_write_int(bs, 1, outOfNetworkIndicator);
		gf_bs_write_int(bs, 1, programSpliceFlag);
		gf_bs_write_int(bs, 1, durationFlag);
		gf_bs_write_int(bs, 1, spliceImmediateFlag);
		/*reserved = */gf_bs_write_int(bs, 4, 0xF);

		if ((programSpliceFlag == 1) && (spliceImmediateFlag == 0)) {
			splice_time = scte35dec_parse_splice_time(bs);
			*pts = splice_time + pts_adjustment;
		}

		if (programSpliceFlag == 0) {
			u32 i;
			u32 component_count = gf_bs_read_u8(bs);
			for (i=0; i<component_count; i++) {
				/*u8 component_tag = */gf_bs_read_u8(bs);
				if (spliceImmediateFlag == 0) {
					splice_time = scte35dec_parse_splice_time(bs);
					*pts = splice_time + pts_adjustment;
				}
			}
		}
		if (durationFlag == 1) {
			//break_duration()
			/*Bool auto_return = */gf_bs_write_int(bs, 1, 0);
			/*reserved = */gf_bs_write_int(bs, 6, 0x3F);
			gf_bs_write_long_int(bs, 33, dur);
		}
	}
#else
	(void)spliceEventId;
	(void)spliceEventCancelIndicator;
	(void)outOfNetworkIndicator;
	(void)segmentationEventId;
	(void)programSpliceFlag;
	(void)durationFlag;
	(void)spliceImmediateFlag;
#endif
}

static void xml_scte35_parse_splice_info(GF_XMLNode *root, GF_BitStream *bs)
{
	u32 i = 0, j = 0;
	GF_XMLNode *node = NULL;
	GF_XMLAttribute *att = NULL;
	char xmlns[256] = "http://www.scte.org/schemas/35";
	u32 sap_type = 0;
	u32 protocol_version = 0;
	u64 pts_adjustment = 0;
	u32 tier = 0;
	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &j))) {
		if (!strcmp(att->name, "xmlns")) {
			snprintf(xmlns, 255, "%s", att->value);
		} else if (!strcmp(att->name, "sapType")) {
			XML_SCAN_INT("%u", sap_type);
		} else if (!strcmp(att->name, "protocolVersion")) {
			XML_SCAN_INT("%u", protocol_version);
		} else if (!strcmp(att->name, "ptsAdjustment")) {
			XML_SCAN_INT(LLU, pts_adjustment);
		} else if (!strcmp(att->name, "tier")) {
			XML_SCAN_INT("%u", tier);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in SpliceInfoSection\n", att->name));
		}
	}

	gf_bs_write_u8(bs, 0xFC); //table_id
	gf_bs_write_int(bs, 0/*section_syntax_indicator*/, 1);
	gf_bs_write_int(bs, 0/*private_indicator*/, 1);
	gf_bs_write_int(bs, sap_type, 2);
	u64 section_length_pos = gf_bs_get_position(bs);
	gf_bs_write_int(bs, 0, 12); //placeholder

	gf_bs_write_u8(bs, protocol_version);
	gf_bs_write_int(bs, 0/*encrypted_packet*/, 1);
	gf_bs_write_int(bs, 0/*encryption_algorithm*/, 6);
	gf_bs_write_long_int(bs, pts_adjustment, 33);
	gf_bs_write_u8(bs, 0/*cw_index*/);
	gf_bs_write_int(bs, tier, 12);
	u64 splice_command_length_pos = gf_bs_get_position(bs);
	gf_bs_write_int(bs, 0, 12); //placeholder

#define WRITE_CMD_LEN() {                               \
		u64 pos = gf_bs_get_position(bs);               \
		int splice_command_length = pos - splice_command_length_pos - 3; \
		gf_bs_seek(bs, splice_command_length_pos + 1);  \
		gf_bs_write_int(bs, splice_command_length, 8); \
		gf_bs_seek(bs, pos);                            \
	}

	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		if (!strcmp(node->name, "TimeSignal")) {
			gf_bs_write_u8(bs, 0x06);
			xml_scte35_parse_time_signal(node, bs);
			WRITE_CMD_LEN();
			continue;
		} else if (!strcmp(node->name, "SpliceInsert")) {
			gf_bs_write_u8(bs, 0x05);
			xml_scte35_parse_splice_insert(node, bs);
			WRITE_CMD_LEN();
			break;
		} else if (!strcmp(node->name, "SegmentationDescriptor")) {
			xml_scte35_parse_segmentation_descriptor(node, bs);
			break;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown node \"%s\" in SpliceInfoSection\n", node->name));
		}
	}

	// post write section length
	{
		u64 pos = gf_bs_get_position(bs);
		int section_length = pos + 4 /*CRC32*/ - section_length_pos - 2;
		gf_bs_seek(bs, section_length_pos);
		gf_bs_write_u16(bs, (sap_type << 12) | section_length);
		gf_bs_seek(bs, pos);
	}

	// write CRC32
	{
		u8 *data = NULL;
		u32 size = 0;
		GF_BitStream *bs_tmp = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_transfer(bs_tmp, bs, GF_TRUE);
		gf_bs_get_content(bs_tmp, &data, &size);
		gf_bs_seek(bs, gf_bs_get_size(bs));
		gf_bs_write_u32(bs, gf_crc_32(data+(section_length_pos-1), size-(section_length_pos-1)));
		gf_bs_del(bs_tmp);
	}

#undef WRITE_CMD_LEN
}

// raw SCTE-35 payload
void xml_scte35_parse(GF_XMLNode *root, GF_BitStream *bs)
{
	u32 i=0;
	GF_XMLNode *node = NULL;
	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		if (!strcmp(node->name, "SpliceInfoSection")) {
			xml_scte35_parse_splice_info(node, bs);
			continue;
		}
	}
}

// SCTE-35 encapsulated in an EMEB box
void xml_emeb_parse(GF_XMLNode *root, GF_BitStream *bs)
{
	GF_EventMessageBox *emeb = (GF_EventMessageBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_EMEB);
	gf_isom_box_size((GF_Box*)emeb);
	if (gf_isom_box_write((GF_Box*)emeb, bs) != GF_OK)
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] EventMessageEmptyBox serialization failed\n"));
	gf_isom_box_del((GF_Box*)emeb);
}

// SCTE-35 encapsulated in an EMIB box
void xml_emib_parse(GF_XMLNode *root, GF_BitStream *bs)
{
	int i = 0;
	GF_XMLAttribute *att = NULL;
	GF_EventMessageBox *emib = (GF_EventMessageBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_EMIB);

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &i))) {
		if (!stricmp(att->name, "version")) {
			if (sscanf(att->value, "%hu", &emib->version) != 1)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Invalid value for version=\"%s\"\n", att->value));
		} else if (!stricmp(att->name, "flags")) {
			if (sscanf(att->value, "%u", &emib->flags) != 1)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Invalid value for flags=\"%s\"\n", att->value));
		} else if (!stricmp(att->name, "scheme_id_uri")) {
			emib->scheme_id_uri = gf_strdup(att->value);
		} else if (!stricmp(att->name, "value")) {
			emib->value = gf_strdup(att->value);
		} else if (!stricmp(att->name, "presentation_time_delta")) {
			if (sscanf(att->value, LLD, &emib->presentation_time_delta) != 1)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Invalid value for presentation_time_delta=\"%s\"\n", att->value));
		} else if (!stricmp(att->name, "event_duration")) {
			if (sscanf(att->value, "%u", &emib->event_duration) != 1)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Invalid value for event_duration=\"%s\"\n", att->value));
		} else if (!stricmp(att->name, "event_id")) {
			if (sscanf(att->value, "%u", &emib->event_id) != 1)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Invalid value for event_id=\"%s\"\n", att->value));
		} else if (!stricmp(att->name, "message_data")) {
			u8 *ptr = att->value;
			if (!strnicmp(ptr, "0x", 2)) ptr +=2;
			u32 len = (u32)strlen(ptr)/2;
			emib->message_data = gf_malloc(len);
			emib->message_data_size = len;
			for (u32 i=0; i<len; ++i, ptr+=2) {
				int val=0;
				sscanf(ptr, "%02x", &val);
				emib->message_data[i] = (u8)val;
			}
		} else if (!stricmp(att->name, "Size")) {
		} else if (!stricmp(att->name, "Type")) {
		} else if (!stricmp(att->name, "Specification")) {
		} else if (!stricmp(att->name, "Container")) {
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] Unknown attribute \"%s\" in EventMessageInstanceBox parsing\n", att->name));
		}
	}

	u64 start = gf_bs_get_position(bs);
	gf_isom_box_size((GF_Box*)emib);
	if (gf_isom_box_write((GF_Box*)emib, bs) != GF_OK)
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML] EventMessageInstanceBox serialization failed\n"));

	if (!emib->message_data) {
		// append emib->message_data as it is the last field
		u64 before = gf_bs_get_position(bs);
		xml_scte35_parse(root, bs);
		u64 end = gf_bs_get_position(bs);

		gf_bs_seek(bs, start);
		emib->message_data = (u8*)before;          //dumb non-null pointer
		emib->message_data_size = end - before;
		gf_isom_box_size((GF_Box*)emib);
		gf_isom_full_box_write((GF_Box*)emib, bs); //rewrite size
		gf_bs_seek(bs, end);

		emib->message_data = NULL;
		emib->message_data_size = 0;
	}

	gf_isom_box_del((GF_Box*)emib);
}
