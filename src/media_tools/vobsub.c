/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Copyright (c) by  Falco (Ivan Vecera) 2006
 *                  All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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


#include <gpac/list.h>
#include <gpac/internal/vobsub.h>

typedef struct _tag_lang_type
{
	char id[3];
	char lang[4];
} lang_type;

static lang_type lang_table[] =
{
	{"--", "und" },
	{"aa", "aar" },
	{"ab", "abk" },
	{"af", "afr" },
	{"am", "amh" },
	{"ar", "ara" },
	{"as", "ast" },
	{"ay", "aym" },
	{"az", "aze" },
	{"ba", "bak" },
	{"be", "bel" },
	{"bg", "bul" },
	{"bh", "bih" },
	{"bi", "bis" },
	{"bn", "ben" },
	{"bo", "bod" }, // was "tib" (Tibetan)
	{"br", "bre" },
	{"ca", "cat" },
	{"cc", "und" },
	{"co", "cos" },
	{"cs", "ces" }, // was "cze" (Czech)
	{"cy", "cym" }, // was "wel" (Welsh)
	{"da", "dan" },
	{"de", "deu" }, // was "ger" (German)
	{"dz", "dzo" },
	{"el", "ell" }, // was "gre" (Greek, Modern (1453-))
	{"en", "eng" },
	{"eo", "epo" },
	{"es", "spa" },
	{"et", "est" },
	{"eu", "eus" }, // was "baq" (Basque)
	{"fa", "fas" }, // was "per" (Persian)
	{"fi", "fin" },
	{"fj", "fij" },
	{"fo", "fao" },
	{"fr", "fra" }, // was "fre" (French)
	{"fy", "fry" },
	{"ga", "gle" },
	{"gl", "glg" },
	{"gn", "grn" },
	{"gu", "guj" },
	{"ha", "hau" },
	{"he", "heb" },
	{"hi", "hin" },
	{"hr", "scr" },
	{"hu", "hun" },
	{"hy", "hye" }, // was "arm" (Armenian)
	{"ia", "ina" },
	{"id", "ind" },
	{"ik", "ipk" },
	{"is", "isl" }, // was "ice" (Icelandic)
	{"it", "ita" },
	{"iu", "iku" },
	{"ja", "jpn" },
	{"jv", "jav" },
	{"ka", "kat" }, // was "geo" (Georgian)
	{"kk", "kaz" },
	{"kl", "kal" },
	{"km", "khm" },
	{"kn", "kan" },
	{"ko", "kor" },
	{"ks", "kas" },
	{"ku", "kur" },
	{"ky", "kir" },
	{"la", "lat" },
	{"ln", "lin" },
	{"lo", "lao" },
	{"lt", "lit" },
	{"lv", "lav" },
	{"mg", "mlg" },
	{"mi", "mri" }, // was "mao" (Maori)
	{"mk", "mkd" }, // was "mac" (Macedonian)
	{"ml", "mlt" },
	{"mn", "mon" },
	{"mo", "mol" },
	{"mr", "mar" },
	{"ms", "msa" }, // was "may" (Malay)
	{"my", "mya" }, // was "bur" (Burmese)
	{"na", "nau" },
	{"ne", "nep" },
	{"nl", "nld" }, // was "dut" (Dutch; Flemish)
	{"no", "nor" },
	{"oc", "oci" },
	{"om", "orm" },
	{"or", "ori" },
	{"pa", "pan" },
	{"pl", "pol" },
	{"ps", "pus" },
	{"pt", "por" },
	{"qu", "que" },
	{"rm", "roh" },
	{"rn", "run" },
	{"ro", "ron" }, // was "rum" (Romanian; Moldavian; Moldovan)
	{"ru", "rus" },
	{"rw", "kin" },
	{"sa", "san" },
	{"sd", "snd" },
	{"sg", "sag" },
	{"sh", "scr" },
	{"si", "sin" },
	{"sk", "slk" }, // was "slo" (Slovak)
	{"sl", "slv" },
	{"sm", "smo" },
	{"sn", "sna" },
	{"so", "som" },
	{"sq", "sqi" }, // was "alb" (Albanian)
	{"sr", "srp" },
	{"ss", "ssw" },
	{"st", "sot" },
	{"su", "sun" },
	{"sv", "swe" },
	{"sw", "swa" },
	{"ta", "tam" },
	{"te", "tel" },
	{"tg", "tgk" },
	{"th", "tha" },
	{"ti", "tir" },
	{"tk", "tuk" },
	{"tl", "tgl" },
	{"tn", "tsn" },
	{"to", "tog" },
	{"tr", "tur" },
	{"ts", "tso" },
	{"tt", "tat" },
	{"tw", "twi" },
	{"ug", "uig" },
	{"uk", "ukr" },
	{"ur", "urd" },
	{"uz", "uzb" },
	{"vi", "vie" },
	{"vo", "vol" },
	{"wo", "wol" },
	{"xh", "xho" },
	{"yi", "yid" },
	{"yo", "yor" },
	{"za", "zha" },
	{"zh", "zho" }, // was "chi" (Chinese)
	{"zu", "zul" }
};



s32 vobsub_lang_name(u16 id)
{
	u16 lang_id;
	s32 i, count;

	count = (sizeof(lang_table) / sizeof(lang_table[0]));

	for (i = 0; i < count; i++) {
		lang_id = (lang_table[i].id[0]<<8) | lang_table[i].id[1];

		if (id == lang_id) {
			return i;
		}
	}

	return 0; /* Undefined - und */
}

char *vobsub_lang_id(char *name)
{
	s32 i, count;

	count = (sizeof(lang_table) / sizeof(lang_table[0]));

	for (i = 0; i < count; i++) {
		if (!stricmp(lang_table[i].lang, name)) {
			return lang_table[i].id;
		}
	}

	return "--"; /* Undefined */
}

static char *strltrim(char *str)
{
	if (str == NULL) {
		return NULL;
	}

	while (*str) {
		if (!isspace(*str)) {
			return str;
		}
		str++;
	}

	return str;
}

static char *strrtrim(char *str)
{
	char *end;

	if (str == NULL) {
		return NULL;
	}

	end = str + strlen(str);

	while (end-- > str) {
		if (!isspace(*end)) {
			return str;
		}
		*end = '\0';
	}

	return str;
}

static char *strtrim(char *str)
{
	return strltrim(strrtrim(str));
}

GF_Err vobsub_read_idx(FILE *file, vobsub_file *vobsub, s32 *version)
{
	char  strbuf[256];
	char *str, *pos, *entry;
	s32   line, id =-1, delay = 0;
	Bool  error = 0;

	for (line = 0; !error && fgets(strbuf, sizeof(strbuf), file); line++)
	{
		str = strtrim(strbuf);

		if (line == 0)
		{
			char *buf = "VobSub index file, v";

			pos = strstr(str, buf);
			if (pos == NULL || sscanf(pos + strlen(buf), "%d", version) != 1 || *version > VOBSUBIDXVER)
			{
				error = 1;
				continue;
			}
		}
		else if (strlen(str) == 0)
		{
			continue;
		}
		else if (str[0] == '#')
		{
			continue;
		}

		pos = strchr(str, ':');
		if (pos == NULL || pos == str)
		{
			continue;
		}

		entry = str;
		*pos  = '\0';

		str = strtrim(pos + 1);
		if (strlen(str) == 0)
		{
			continue;
		}

		if (stricmp(entry, "size") == 0)
		{
			s32 w, h;
			if (sscanf(str, "%dx%d", &w, &h) != 2)
			{
				error = 1;
			}
			vobsub->width  = w;
			vobsub->height = h;
		}
		else if (stricmp(entry, "palette") == 0)
		{
			s32 c;
			u8  palette[16][4];

			if (sscanf(str, "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
			           (u32 *) &palette[0], (u32 *) &palette[1], (u32 *) &palette[2], (u32 *) &palette[3],
			           (u32 *) &palette[4], (u32 *) &palette[5], (u32 *) &palette[6], (u32 *) &palette[7],
			           (u32 *) &palette[8], (u32 *) &palette[9], (u32 *) &palette[10], (u32 *) &palette[11],
			           (u32 *) &palette[12],(u32 *) &palette[13],(u32 *) &palette[14], (u32 *) &palette[15]) != 16)
			{
				error = 1;
				continue;
			}

			for (c = 0; c < 16; c++)
			{
				u8 r, g, b;

				r = palette[c][2];
				g = palette[c][1];
				b = palette[c][0];
				vobsub->palette[c][0] = 0;
				vobsub->palette[c][1] = (( 66 * r + 129 * g +  25 * b + 128 +  4096) >> 8) & 0xff;
				vobsub->palette[c][2] = ((112 * r -  94 * g -  18 * b + 128 + 32768) >> 8) & 0xff;
				vobsub->palette[c][3] = ((-38 * r -  74 * g + 112 * b + 128 + 32768) >> 8) & 0xff;
			}
		}
		else if (stricmp(entry, "id") == 0)
		{
			char *buf = "index:";
			s32   lang_id;

			strlwr(str);
			lang_id = ((str[0] & 0xff) << 8) | (str[1] & 0xff);

			pos = strstr(str, buf);
			if (pos == NULL)
			{
				error = 1;
				continue;
			}

			if (sscanf(pos + strlen(buf), "%d", &id) != 1 || id < 0 || id >= 32)
			{
				error = 1;
				continue;
			}

			vobsub->langs[id].id   = lang_id;
			vobsub->langs[id].name = lang_table[vobsub_lang_name((u16)lang_id)].lang;
			vobsub->langs[id].idx = id;

			vobsub->langs[id].subpos = gf_list_new();
			if (vobsub->langs[id].subpos == NULL)
			{
				error = 1;
				continue;
			}

			delay = 0;
			vobsub->num_langs++;
		}
		else if (id >= 0 && stricmp(entry, "delay") == 0)
		{
			s32  hh, mm, ss, ms;
			char c;
			s32  sign = (str[0] == '-') ? -1 : 1;

			pos = str;
			while (*pos == '-' || *pos == '+') pos++;

			if (sscanf(pos, "%d%c%d%c%d%c%d", &hh, &c, &mm, &c, &ss, &c, &ms) != 7)
			{
				error = 1;
				continue;
			}

			delay += (hh*60*60*1000 + mm*60*1000 + ss*1000 + ms) * sign;
		}
		else if (id >= 0 && stricmp(entry, "timestamp") == 0)
		{
			vobsub_pos *vspos;
			s32         sign;
			char        c;
			s32         hh, mm, ss, ms;
			char       *buf = "filepos:";

			vspos = (vobsub_pos*)gf_calloc(1, sizeof(vobsub_pos));
			if (vspos == NULL) {
				error = 1;
				continue;
			}

			sign = (str[0] == '-') ? -1 : 1;
			while (*str == '-' || *str == '+') str++;

			if (sscanf(str, "%d%c%d%c%d%c%d", &hh, &c, &mm, &c, &ss, &c, &ms) != 7)
			{
				gf_free(vspos);
				error = 1;
				continue;
			}

			vspos->start = (((hh*60 + mm)*60 + ss)*1000 + ms) * sign + delay;

			pos = strstr(str, buf);
			if (pos == NULL)
			{
				gf_free(vspos);
				error = 1;
				continue;
			}

			if (sscanf(pos + strlen(buf), LLX, &vspos->filepos) != 1)
			{
				gf_free(vspos);
				error = 1;
				continue;
			}

			if (delay < 0 && gf_list_count(vobsub->langs[id].subpos) > 0)
			{
				vobsub_pos *pos;

				pos = (vobsub_pos*)gf_list_get(vobsub->langs[id].subpos, gf_list_count(vobsub->langs[id].subpos) - 1);
				if (vspos->start < pos->start)
				{
					delay += (s32)(pos->start - vspos->start);
					vspos->start = pos->start;
				}
			}

			if (gf_list_add(vobsub->langs[id].subpos, vspos) != GF_OK)
			{
				gf_free(vspos);
				error = 1;
				continue;
			}
		}
	}

	return error ? GF_CORRUPTED_DATA : GF_OK;
}

void vobsub_free(vobsub_file *vobsub)
{
	s32 i;

	if (vobsub == NULL)
		return;

	for (i = 0; i < 32; i++) {
		if (vobsub->langs[i].subpos) {
			GF_List *list = vobsub->langs[i].subpos;
			vobsub_pos *vspos;
			u32 pos = 0;

			do {
				vspos = (vobsub_pos*)gf_list_enum(list, &pos);
				gf_free(vspos);
			}
			while (vspos != NULL);

			gf_list_del(list);
		}
	}
	gf_free(vobsub);
}

GF_Err vobsub_get_subpic_duration(u8 *_data, u32 psize, u32 dsize, u32 *duration)
{
	u32 i, dcsq_stm, nxt_dcsq, start_stm, stop_stm;
	u8 *data = (u8 *)_data;
	start_stm = 0;
	stop_stm  = 0;
	nxt_dcsq  = dsize;

	if (psize) do {
		i = nxt_dcsq;
		dcsq_stm = (data[i+0] << 8) | data[i+1];
		nxt_dcsq = (data[i+2] << 8) | data[i+3];
		i += 4;

		if (nxt_dcsq > psize || nxt_dcsq < dsize) {
			return GF_CORRUPTED_DATA;
		}

		while (1) {
			u8  cmd;
			int len;

			cmd = data[i++];
			switch (cmd)
			{
			case 0x00:
				len = 0;
				break;
			case 0x01:
				len = 0;
				break;
			case 0x02:
				len = 0;
				break;
			case 0x03:
				len = 2;
				break;
			case 0x04:
				len = 2;
				break;
			case 0x05:
				len = 6;
				break;
			case 0x06:
				len = 4;
				break;
			default:
				len = 0;
				break;
			}

			if (i + len > psize) {
				return GF_CORRUPTED_DATA;
			}

			i += len;

			if (cmd == 0x00 || cmd == 0x01) {
				/* start normal or forced displaying */
				start_stm = dcsq_stm * 1024;
			} else if (cmd == 0x02) {
				/* stop displaying */
				stop_stm = dcsq_stm * 1024;
			} else if (cmd > 0x06) {
				/* unknown command or end of control block */
				break;
			}
		}
	} while (i <= nxt_dcsq && i < psize);

	*duration = stop_stm - start_stm;

	return GF_OK;
}

GF_Err vobsub_packetize_subpicture(FILE *fsub, u64 pts, u8 *data, u32 dataSize)
{
	u8	buf[0x800], ptsbuf[5];
	u8	*p;
	int	put_pts = 1;

	/* Build PTS buffer */
	ptsbuf[0] = (u8)(((pts >> 29) & 0x0e) | 0x21);
	ptsbuf[1] = (u8)(((pts >> 22) & 0xff));
	ptsbuf[2] = (u8)(((pts >> 14) & 0xfe) | 0x01);
	ptsbuf[3] = (u8)(((pts >>  7) & 0xff));
	ptsbuf[4] = (u8)(((pts <<  1) & 0xfe) | 0x01);

	while (dataSize > 0)
	{
		u32 padLen = 0;
		u32 dataLen = sizeof(buf);
		u32 packLen;

		/* Zerofill packet */
		memset(buf, 0, sizeof(buf));
		p = buf;

		/* Put pack header */
		*p++ = 0x00;
		*p++ = 0x00;
		*p++ = 0x01;
		*p++ = 0xba;
		*p++ = 0x40;

		/* Jump to PES header */
		p += 9;

		/* Put PES header */
		*p++ = 0x00;
		*p++ = 0x00;
		*p++ = 0x01;
		*p++ = 0xbd;

		/* Compute max size of content */
		dataLen -= 14; /* Pack header */
		dataLen -=  4; /* Start code + Stream ID */
		dataLen -=  2; /* PES packet size */
		dataLen -=  3; /* PES header extension */
		dataLen -= put_pts ? 5 : 0; /* PTS */
		dataLen -=  1; /* Substream ID */

		/* Check if the subpicture data fits in packet */
		if (dataSize <= dataLen) {
			padLen  = dataLen - dataSize;
			dataLen = dataSize;
		}

		/* Compute and put packet size (PES header extension + PTS + Substream ID + data + padding) */
		packLen = 3 + (put_pts ? 5 : 0) + 1 + dataLen + ((padLen < 6) ? padLen : 0);
		*p++ = (packLen >> 8) & 0xff;
		*p++ = packLen & 0xff;

		/* Put PES header extension */
		*p++ = 0x80;
		*p++ = put_pts ? 0x80 : 0x00;
		*p++ = (put_pts ? 5 : 0) + ((padLen < 6) ? padLen : 0);

		/* Put PTS */
		if (put_pts) {
			*p++ = ptsbuf[0];
			*p++ = ptsbuf[1];
			*p++ = ptsbuf[2];
			*p++ = ptsbuf[3];
			*p++ = ptsbuf[4];
		}

		/* Skip padding bytes */
		if (padLen < 6) {
			p += padLen;
		}

		/* Put Substream ID */
		*p++ = 0x20;

		/* Copy data into packet buffer */
		memcpy(p, data, dataLen);
		p += dataLen;

		/* Put padding bytes if padding len >= 6 */
		if (padLen >= 6) {
			padLen -= 6;
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x01;
			*p++ = 0xbe;
			*p++ = (padLen >> 8) & 0xff;
			*p++ = padLen & 0xff;
			memset(p, 0, padLen);
		}

		/* Write packet into file */
		if (gf_fwrite(buf, sizeof(buf), 1, fsub) != 1) {
			return GF_IO_ERR;
		}

		/* Move data pointer... */
		data += dataLen;
		dataSize -= dataLen;

		/* Next packet (if any) will not contain PTS */
		put_pts = 0;
	}

	return GF_OK;
}
