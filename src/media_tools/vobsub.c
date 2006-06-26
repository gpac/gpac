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

typedef	struct _tag_lang_type
{
	char id[2];
	char lang[4];
} lang_type;

static lang_type lang_table[] =
{
	{"--", "und" },
	{"cc", "und" },
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
	{"bo", "tib" },
	{"br", "bre" },
	{"ca", "cat" },
	{"co", "cos" },
	{"cs", "cze" },
	{"cy", "wel" },
	{"da", "dan" },
	{"de", "ger" },
	{"dz", "dzo" },
	{"el", "gre" },
	{"en", "eng" },
	{"eo", "epo" },
	{"es", "spa" },
	{"et", "est" },
	{"eu", "baq" },
	{"fa", "per" },
	{"fi", "fin" },
	{"fj", "fij" },
	{"fo", "fao" },
	{"fr", "fre" },
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
	{"hy", "arm" },
	{"ia", "ina" },
	{"id", "ind" },
	{"ik", "ipk" },
	{"is", "ice" },
	{"it", "ita" },
	{"iu", "iku" },
	{"ja", "jpn" },
	{"jv", "jav" },
	{"ka", "geo" },
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
	{"mi", "mao" },
	{"mk", "mac" },
	{"ml", "mlt" },
	{"mn", "mon" },
	{"mo", "mol" },
	{"mr", "mar" },
	{"ms", "may" },
	{"my", "bur" },
	{"na", "nau" },
	{"ne", "nep" },
	{"nl", "dut" },
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
	{"ro", "rum" },
	{"ru", "rus" },
	{"rw", "kin" },
	{"sa", "san" },
	{"sd", "snd" },
	{"sg", "sag" },
	{"sh", "scr" },
	{"si", "sin" },
	{"sk", "slo" },
	{"sl", "slv" },
	{"sm", "smo" },
	{"sn", "sna" },
	{"so", "som" },
	{"sq", "alb" },
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
	{"zh", "chi" },
	{"zu", "zul" }
};

static s32 find_lang(u16 id)
{
	u16	lang_id;
	s32	mid, lo, hi;

	lo = 0;
	hi = (sizeof(lang_table) / sizeof(lang_table[0])) -	1;

	while (lo <	hi)
	{
		mid	= (lo +	hi)	>> 1;
		lang_id	= (lang_table[mid].id[0]<<8) | lang_table[mid].id[1];

		if (id < lang_id) {
			hi = mid;
		} else if (id >	lang_id) {
			lo = mid;
		} else {
			return mid;
		}
	}

	lang_id	= (lang_table[lo].id[0]<<8)	| lang_table[lo].id[1];
	return (id == lang_id ?	lo : 0);
}

static char	*strltrim(char *str)
{
	if (str	== NULL) {
		return NULL;
	}

	while (*str) {
		if (!isspace(*str))	{
			return str;
		}
		str++;
	}

	return str;
}

static char	*strrtrim(char *str)
{
	char *end;

	if (str	== NULL) {
		return NULL;
	}

	end	= str +	strlen(str);

	while (end-- > str)	{
		if (!isspace(*end))	{
			return str;
		}
		*end = '\0';
	}

	return str;
}

static char	*strtrim(char *str)
{
	return strltrim(strrtrim(str));
}

GF_Err vobsub_read_idx(FILE	*file, vobsub_file *vobsub,	s32	*version)
{
	char  strbuf[256];
	char *str, *pos, *entry;
	s32	  line,	id =-1,	delay =	0;
	Bool  error	= 0;

	for	(line =	0; !error && fgets(strbuf, sizeof(strbuf), file); line++)
	{
		str	= strtrim(strbuf);

		if (line ==	0)
		{
			char *buf =	"VobSub	index file,	v";

			pos	= strstr(str, buf);
			if (pos	== NULL	|| sscanf(pos +	strlen(buf), "%d", version)	!= 1 ||	*version > VOBSUBIDXVER)
			{
				error =	1;
				continue;
			}
		}
		else if	(strlen(str) ==	0)
		{
			continue;
		}
		else if	(str[0]	== '#')
		{
			continue;
		}

		pos	= strchr(str, ':');
		if (pos	== NULL	|| pos == str)
		{
			continue;
		}

		entry =	str;
		*pos  =	'\0';

		str	= strtrim(pos +	1);
		if (strlen(str)	== 0)
		{
			continue;
		}

		if (stricmp(entry, "size") == 0)
		{
			s32	w, h;
			if (sscanf(str,	"%dx%d", &w, &h) !=	2)
			{
				error =	1;
			}
			vobsub->width  = w;
			vobsub->height = h;
		}
		else if	(stricmp(entry,	"palette") == 0)
		{
			s32	c;
			u8	palette[16][4];

			if (sscanf(str,	"%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
					  (u32 *) &palette[0], (u32	*) &palette[1],	(u32 *)	&palette[2], (u32 *) &palette[3],
					  (u32 *) &palette[4], (u32	*) &palette[5],	(u32 *)	&palette[6], (u32 *) &palette[7],
					  (u32 *) &palette[8], (u32	*) &palette[9],	(u32 *)	&palette[10], (u32 *) &palette[11],
					  (u32 *) &palette[12],(u32	*) &palette[13],(u32 *)	&palette[14], (u32 *) &palette[15])	!= 16)
			{
				error =	1;
				continue;
			}

			for	(c = 0;	c <	16;	c++)
			{
				u8 r, g, b,	y, u, v;

				r =	palette[c][2];
				g =	palette[c][1];
				b =	palette[c][0];
				y =	abs(r *	 2104 +	g *	 4130 +	b *	 802 + 4096	+  131072) >> 13;
				u =	abs(r *	-1214 +	g *	-2384 +	b *	3598 + 4096	+ 1048576) >> 13;
				v =	abs(r *	 3598 +	g *	-3013 +	b *	-585 + 4096	+ 1048576) >> 13;
				vobsub->palette[c][0] =	0;
				vobsub->palette[c][1] =	(y > 235) ?	235	: y;
				vobsub->palette[c][2] =	(v > 240) ?	240	: v;
				vobsub->palette[c][3] =	(u > 240) ?	240	: u;
			}
		}
		else if	(stricmp(entry,	"id") == 0)
		{
			char *buf =	"index:";
			s32	  lang_id;

			strlwr(str);
			lang_id	= ((str[0] & 0xff) << 8) | (str[1] & 0xff);

			pos	= strstr(str, buf);
			if (pos	== NULL)
			{
				error =	1;
				continue;
			}

			if (sscanf(pos + strlen(buf), "%d",	&id) !=	1 || id	< 0	|| id >= 32)
			{
				error =	1;
				continue;
			}

			vobsub->langs[id].id   = lang_id;
			vobsub->langs[id].name = lang_table[find_lang((u16)lang_id)].lang;

			vobsub->langs[id].subpos = gf_list_new();
			if (vobsub->langs[id].subpos ==	NULL)
			{
				error =	1;
				continue;
			}

			delay =	0;
			vobsub->num_langs++;
		}
		else if	(id	>= 0 &&	stricmp(entry, "delay")	== 0)
		{
			s32	 hh, mm, ss, ms;
			char c;
			s32	 sign =	(str[0]	== '-')	? -1 : 1;

			pos	= str;
			while (*pos	== '-' || *pos == '+') pos++;

			if (sscanf(pos,	"%d%c%d%c%d%c%d", &hh, &c, &mm,	&c,	&ss, &c, &ms) != 7)
			{
				error =	1;
				continue;
			}

			delay += (hh*60*60*1000	+ mm*60*1000 + ss*1000 + ms) * sign;
		}
		else if	(id	>= 0 &&	stricmp(entry, "timestamp")	== 0)
		{
			vobsub_pos *vspos;
			s32			sign;
			char		c;
			s32			hh,	mm,	ss,	ms;
			char	   *buf	= "filepos:";

			vspos =	(vobsub_pos*)calloc(1, sizeof(vobsub_pos));
			if (vspos == NULL) {
				error =	1;
				continue;
			}

			sign = (str[0] == '-') ? -1	: 1;
			while (*str	== '-' || *str == '+') str++;

			if (sscanf(str,	"%d%c%d%c%d%c%d", &hh, &c, &mm,	&c,	&ss, &c, &ms) != 7)
			{
				free(vspos);
				error =	1;
				continue;
			}

			vspos->start = (((hh*60	+ mm)*60 + ss)*1000	+ ms) *	sign + delay;

			pos	= strstr(str, buf);
			if (pos	== NULL)
			{
				free(vspos);
				error =	1;
				continue;
			}

#if	defined	(_MSC_VER) || defined(__MINGW32__)
			if (sscanf(pos + strlen(buf), "%I64x", &vspos->filepos)	!= 1)
#else
			if (sscanf(pos + strlen(buf), "%llx", &vspos->filepos) != 1)
#endif
			{
				free(vspos);
				error =	1;
				continue;
			}

			if (delay <	0 && gf_list_count(vobsub->langs[id].subpos) > 0)
			{
				vobsub_pos *pos;

				pos	= (vobsub_pos*)gf_list_get(vobsub->langs[id].subpos, gf_list_count(vobsub->langs[id].subpos) - 1);
				if (vspos->start < pos->start)
				{
					delay += (s32)(pos->start -	vspos->start);
					vspos->start = pos->start;
				}
			}

			if (gf_list_add(vobsub->langs[id].subpos, vspos) !=	GF_OK)
			{
				free(vspos);
				error =	1;
				continue;
			}
		}
	}

	return error ? GF_CORRUPTED_DATA : GF_OK;
}

void vobsub_free(vobsub_file *vobsub)
{
	s32	c;

	if (vobsub == NULL)
	{
		return;
	}

	for	(c = 0;	c <	32;	c++)
	{
		if (vobsub->langs[c].subpos)
		{
			GF_List	   *list = vobsub->langs[c].subpos;
			vobsub_pos *vspos;
			u32			pos	= 0;

			do
			{
				vspos =	(vobsub_pos*)gf_list_enum(list,	&pos);
				free(vspos);
			}
			while (vspos !=	NULL);
		}
	}
}

GF_Err vobsub_get_subpic_duration(u8 *data,	u32	psize, u32 dsize, u32 *duration)
{
	u32	i, dcsq_stm, nxt_dcsq, start_stm, stop_stm;

	start_stm =	0;
	stop_stm  =	0;
	nxt_dcsq  =	dsize;

	do {
		i =	nxt_dcsq;
		dcsq_stm = (data[i+0] << 8)	| data[i+1];
		nxt_dcsq = (data[i+2] << 8)	| data[i+3];
		i += 4;

		if (nxt_dcsq > psize ||	nxt_dcsq < dsize) {
			return GF_CORRUPTED_DATA;
		}

		while (1) {
			u8	cmd;
			int	len;

			cmd	= data[i++];
			switch (cmd) {
				case 0x00: len = 0;	break;
				case 0x01: len = 0;	break;
				case 0x02: len = 0;	break;
				case 0x03: len = 2;	break;
				case 0x04: len = 2;	break;
				case 0x05: len = 6;	break;
				case 0x06: len = 4;	break;
				default:   len = 0;	break;
			}

			if (i +	len	> psize) {
				return GF_CORRUPTED_DATA;
			}

			i += len;

			if (cmd	== 0x00	|| cmd == 0x01)	{
				/* start normal	or forced displaying */
				start_stm =	dcsq_stm * 1024;
			} else if (cmd == 0x02)	{
				/* stop	displaying */
				stop_stm = dcsq_stm	* 1024;
			} else if (cmd > 0x06) {
				/* unknown command or end of control block */
				break;
			}
		}
	} while	(i <= nxt_dcsq && i	< psize);

	*duration =	stop_stm - start_stm;

	return GF_OK;
}
