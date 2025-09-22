/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato
 *          Copyright (c) Telecom ParisTech 2000-2012
 *                  All rights reserved
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

#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_ISOM

#ifndef GPAC_DISABLE_TTML

/* blindly adds text to a sample following 3GPP Timed Text style */
GF_Err gf_isom_xml_subtitle_sample_add_text(GF_GenericSubtitleSample *samp, char *text_data, u32 text_len)
{
	if (!samp) return GF_BAD_PARAM;
	if (!text_len) return GF_OK;
	samp->text = (char*)gf_realloc(samp->text, sizeof(char) * (samp->len + text_len) );
	memcpy(samp->text + samp->len, text_data, sizeof(char) * text_len);
	samp->len += text_len;
	return GF_OK;
}

/*
 * Writing the xml sample structure into a sample buffer
 * - putting the text or XML in the sample directly
 * - optionally handle secondary resources (TODO)
*/
GF_ISOSample *gf_isom_xml_subtitle_to_sample(GF_GenericSubtitleSample *samp)
{
	GF_ISOSample *res;
	GF_BitStream *bs;
	if (!samp) return NULL;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (samp->len) gf_bs_write_data(bs, samp->text, samp->len);
	else     gf_bs_write_data(bs, "", 1);

	res = gf_isom_sample_new();
	if (!res) {
		gf_bs_del(bs);
		return NULL;
	}
	gf_bs_get_content(bs, &res->data, &res->dataLength);
	gf_bs_del(bs);
	res->IsRAP = RAP;
	return res;
}

GF_GenericSubtitleSample *gf_isom_new_xml_subtitle_sample()
{
	GF_GenericSubtitleSample *res;
	GF_SAFEALLOC(res, GF_GenericSubtitleSample);
	if (!res) return NULL;
	return res;
}

GF_Err gf_isom_xml_subtitle_reset(GF_GenericSubtitleSample *samp)
{
	if (!samp) return GF_BAD_PARAM;
	if (samp->text) gf_free(samp->text);
	samp->text = NULL;
	samp->len = 0;
	return GF_OK;
}

GF_EXPORT
void gf_isom_delete_xml_subtitle_sample(GF_GenericSubtitleSample * samp)
{
	gf_isom_xml_subtitle_reset(samp);
	gf_free(samp);
}

GF_EXPORT
GF_GenericSubtitleSample *gf_isom_parse_xml_subtitle_sample(GF_BitStream *bs)
{
	GF_GenericSubtitleSample *s = gf_isom_new_xml_subtitle_sample();

	/*empty sample*/
	if (!bs || !gf_bs_available(bs)) return s;

	s->len = gf_bs_read_u16(bs);
	if (s->len) {
		/*2 extra bytes for UTF-16 term char just in case (we don't know if a BOM marker is present or
		not since this may be a sample carried over RTP*/
		s->text = (char *) gf_malloc(sizeof(char)*(s->len+2) );
		if (!s->text) return NULL;
		s->text[s->len] = 0;
		s->text[s->len+1] = 0;
		gf_bs_read_data(bs, s->text, s->len);
	}
	return s;
}

GF_GenericSubtitleSample *gf_isom_parse_xml_subtitle_sample_from_data(char *data, u32 dataLength)
{
	GF_GenericSubtitleSample *s;
	GF_BitStream *bs;
	/*empty text sample*/
	if (!data || !dataLength) {
		return gf_isom_new_xml_subtitle_sample();
	}

	bs = gf_bs_new(data, dataLength, GF_BITSTREAM_READ);
	s = gf_isom_parse_xml_subtitle_sample(bs);
	gf_bs_del(bs);
	return s;
}


#endif /* GPAC_DISABLE_TTML */

#endif /*GPAC_DISABLE_ISOM*/
