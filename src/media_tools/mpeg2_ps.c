/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MPEG4IP.
 *
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2004.  All Rights Reserved.
 *
 * Contributor(s):
 *		Bill May wmay@cisco.com
 */

/*
 * mpeg2ps.c - parse program stream and vob files
 */
#include "mpeg2_ps.h"

#ifndef GPAC_DISABLE_MPEG2PS


static GFINLINE u16 convert16 (u8 *p)
{
#ifdef GPAC_BIG_ENDIAN
	return *(u16 *)p;
#else
	u16 val = p[0];
	val <<= 8;
	return (val | p[1]);
#endif
}

static GFINLINE u32 convert32 (u8 *p)
{
#ifdef GPAC_BIG_ENDIAN
	return *(u32 *)p;
#else
	u32 val;
	val = p[0];
	val <<= 8;
	val |= p[1];
	val <<= 8;
	val |= p[2];
	val <<= 8;
	val |= p[3];
	return val;
#endif
}

#define FDNULL 0

/*
 * structure for passing timestamps around
 */
typedef struct mpeg2ps_ts_t
{
	Bool have_pts;
	Bool have_dts;
	u64 pts;
	u64 dts;
} mpeg2ps_ts_t;

typedef struct mpeg2ps_record_pes_t
{
	struct mpeg2ps_record_pes_t *next_rec;
	u64 dts;
	u64 location;
} mpeg2ps_record_pes_t;

/*
 * information about reading a stream
 */
typedef struct mpeg2ps_stream_t
{
	mpeg2ps_record_pes_t *record_first, *record_last;
	FILE *m_fd;
	Bool is_video;
	u8 m_stream_id;    // program stream id
	u8 m_substream_id; // substream, for program stream id == 0xbd

	mpeg2ps_ts_t next_pes_ts, frame_ts;
	u32 frames_since_last_ts;
	u64 last_ts;

	Bool have_frame_loaded;
	/*
	 * pes_buffer processing.  this contains the raw elementary stream data
	 */
	u8 *pes_buffer;
	u32 pes_buffer_size;
	u32 pes_buffer_size_max;
	u32 pes_buffer_on;
	u32 frame_len;
	u32 pict_header_offset; // for mpeg video

	// timing information and locations.
	s64 first_pes_loc;
	u64 start_dts;
	Bool first_pes_has_dts;
	s64    end_dts_loc;
	u64 end_dts;
	// audio stuff
	u32 freq;
	u32 channels;
	u32 bitrate;
	u32 samples_per_frame;
	u32 layer;
	// video stuff
	u32 h, w, par;
	Double frame_rate;
	s32 have_mpeg2;
	Double bit_rate;
	u64 ticks_per_frame;

} mpeg2ps_stream_t;

/*
 * main interface structure - contains stream pointers and other
 * information
 */
struct mpeg2ps_ {
	mpeg2ps_stream_t *video_streams[16];
	mpeg2ps_stream_t *audio_streams[32];
	char *filename;
	FILE *fd;
	u64 first_dts;
	u32 audio_cnt, video_cnt;
	s64 end_loc;
	u64 max_dts;
	u64 max_time;  // time is in msec.
};

/*************************************************************************
 * File access routines.  Could all be inline
 *************************************************************************/
static FILE *file_open (const char *name)
{
	return gf_fopen(name, "rb");
}

static Bool file_okay (FILE *fd)
{
	return (fd!=NULL) ? 1 : 0;
}

static void file_close (FILE *fd)
{
	gf_fclose(fd);
}

static Bool file_read_bytes(FILE *fd,
                            u8 *buffer,
                            u32 len)
{
	u32 readval = (u32) gf_fread(buffer, len, fd);
	return readval == len;
}

// note: len could be negative.
static void file_skip_bytes (FILE *fd, s32 len)
{
	gf_fseek(fd, len, SEEK_CUR);
}

#define file_location(__f) gf_ftell(__f)
#define file_seek_to(__f, __off) gf_fseek(__f, __off, SEEK_SET)

static u64 file_size(FILE *fd)
{
	return gf_fsize(fd);
}

static mpeg2ps_record_pes_t *create_record (s64 loc, u64 ts)
{
	mpeg2ps_record_pes_t *ret;
	GF_SAFEALLOC(ret, mpeg2ps_record_pes_t);
	if (!ret) return NULL;
	ret->next_rec = NULL;
	ret->dts = ts;
	ret->location = loc;
	return ret;
}

#define MPEG2PS_RECORD_TIME ((u64) (5 * 90000))
void mpeg2ps_record_pts (mpeg2ps_stream_t *sptr, s64 location, mpeg2ps_ts_t *pTs)
{
	u64 ts;
	mpeg2ps_record_pes_t *p, *q;
	if (sptr->is_video) {
		if (pTs->have_dts == 0) return;
		ts = pTs->dts;
	} else {
		if (pTs->have_pts == 0) return;
		ts = pTs->pts;
	}

	if (sptr->record_first == NULL) {
		sptr->record_first = sptr->record_last = create_record(location, ts);
		return;
	}
	if (ts > sptr->record_last->dts) {
		if (ts < MPEG2PS_RECORD_TIME + sptr->record_last->dts) return;
		sptr->record_last->next_rec = create_record(location, ts);
		sptr->record_last = sptr->record_last->next_rec;
		return;
	}
	if (ts < sptr->record_first->dts) {
		if (ts < MPEG2PS_RECORD_TIME + sptr->record_first->dts) return;
		p = create_record(location, ts);
		p->next_rec = sptr->record_first;
		sptr->record_first = p;
		return;
	}
	p = sptr->record_first;
	q = p->next_rec;

	while (q != NULL && q->dts < ts) {
		p = q;
		q = q->next_rec;
	}
	if (q) {
		if (p->dts + MPEG2PS_RECORD_TIME <= ts &&
				ts + MPEG2PS_RECORD_TIME <= q->dts) {
			p->next_rec = create_record(location, ts);
			p->next_rec->next_rec = q;
		}
	}
}
static Double mpeg12_frame_rate_table[16] =
{
	0.0,   /* Pad */
	24000.0/1001.0,       /* Official frame rates */
	24.0,
	25.0,
	30000.0/1001.0,
	30.0,
	50.0,
	((60.0*1000.0)/1001.0),
	60.0,

	1,                    /* Unofficial economy rates */
	5,
	10,
	12,
	15,
	0,
	0,
};

#define SEQ_ID 1
int MPEG12_ParseSeqHdr(unsigned char *pbuffer, u32 buflen, s32 *have_mpeg2, u32 *height, u32 *width,
                       Double *frame_rate, Double *bitrate, u32 *aspect_ratio)
{
	u32 aspect_code;
	u32 framerate_code;
	u32 bitrate_int;
	u32 bitrate_ext;
	u32 scode, ix;
	s32 found = -1;
	*have_mpeg2 = 0;
	buflen -= 6;
	bitrate_int = 0;
	for (ix = 0; ix < buflen; ix++, pbuffer++) {
		scode = ((u32)pbuffer[0] << 24) | (pbuffer[1] << 16) | (pbuffer[2] << 8) |
		        pbuffer[3];

		if (scode == MPEG12_SEQUENCE_START_CODE) {
			pbuffer += sizeof(u32);
			*width = (pbuffer[0]);
			*width <<= 4;
			*width |= ((pbuffer[1] >> 4) &0xf);
			*height = (pbuffer[1] & 0xf);
			*height <<= 8;
			*height |= pbuffer[2];
			aspect_code = (pbuffer[3] >> 4) & 0xf;
			if (aspect_ratio != NULL) {
				u32 par = 0;
				switch (aspect_code) {
				default:
					*aspect_ratio = 0;
					break;
				case 2:
					par = 4;
					par<<=16;
					par |= 3;
					break;
				case 3:
					par = 16;
					par<<=16;
					par |= 9;
					break;
				case 4:
					par = 2;
					par<<=16;
					par |= 21;
					break;
				}
				*aspect_ratio = par;
			}


			framerate_code = pbuffer[3] & 0xf;
			*frame_rate = mpeg12_frame_rate_table[framerate_code];
			// 18 bits
			bitrate_int = (pbuffer[4] << 10) |
			              (pbuffer[5] << 2) |
			              ((pbuffer[6] >> 6) & 0x3);
			*bitrate = bitrate_int;
			*bitrate *= 400.0;
			ix += sizeof(u32) + 7;
			pbuffer += 7;
			found = 0;
		} else if (found == 0) {
			if (scode == MPEG12_EXT_START_CODE) {
				pbuffer += sizeof(u32);
				ix += sizeof(u32);
				switch ((pbuffer[0] >> 4) & 0xf) {
				case SEQ_ID:
					*have_mpeg2 = 1;
					*height = ((pbuffer[1] & 0x1) << 13) |
					          ((pbuffer[2] & 0x80) << 5) |
					          (*height & 0x0fff);
					*width = (((pbuffer[2] >> 5) & 0x3) << 12) | (*width & 0x0fff);
					bitrate_ext = (pbuffer[2] & 0x1f) << 7;
					bitrate_ext |= (pbuffer[3] >> 1) & 0x7f;
					bitrate_int |= (bitrate_ext << 18);
					*bitrate = bitrate_int;
					*bitrate *= 400.0;
					break;
				default:
					break;
				}
				pbuffer++;
				ix++;
			} else if (scode == MPEG12_PICTURE_START_CODE) {
				return found;
			}
		}
	}
	return found;
}


s32 MPEG12_PictHdrType (unsigned char *pbuffer)
{
	pbuffer += sizeof(u32);
	return ((pbuffer[1] >> 3) & 0x7);
}

#if 0 //unused
u16 MPEG12_PictHdrTempRef(unsigned char *pbuffer)
{
	pbuffer += sizeof(u32);
	return ((pbuffer[0] << 2) | ((pbuffer[1] >> 6) & 0x3));
}
#endif


static u64 read_pts (u8 *pak)
{
	u64 pts;
	u16 temp;

	pts = ((pak[0] >> 1) & 0x7);
	pts <<= 15;
	temp = convert16(&pak[1]) >> 1;
	pts |= temp;
	pts <<= 15;
	temp = convert16(&pak[3]) >> 1;
	pts |= temp;
	return pts;
}


static mpeg2ps_stream_t *mpeg2ps_stream_create (u8 stream_id,
        u8 substream)
{
	mpeg2ps_stream_t *ptr;
	GF_SAFEALLOC(ptr, mpeg2ps_stream_t);
	if (!ptr) return NULL;
	ptr->m_stream_id = stream_id;
	ptr->m_substream_id = substream;
	ptr->is_video = stream_id >= 0xe0;
	ptr->pes_buffer = (u8 *)gf_malloc(4*4096);
	ptr->pes_buffer_size_max = 4 * 4096;
	return ptr;
}

static void mpeg2ps_stream_destroy (mpeg2ps_stream_t *sptr)
{
	mpeg2ps_record_pes_t *p;
	while (sptr->record_first != NULL) {
		p = sptr->record_first;
		sptr->record_first = p->next_rec;
		gf_free(p);
	}
	if (sptr->m_fd != FDNULL) {
		file_close(sptr->m_fd);
		sptr->m_fd = FDNULL;
	}
	if (sptr->pes_buffer) gf_free(sptr->pes_buffer);
	gf_free(sptr);
}


/*
 * adv_past_pack_hdr - read the pack header, advance past it
 * we don't do anything with the data
 */
static void adv_past_pack_hdr (FILE *fd,
                               u8 *pak,
                               u32 read_from_start)
{
	u8 stuffed;
	u8 readbyte;
	u8 val;
	if (read_from_start < 5) {
		file_skip_bytes(fd, 5 - read_from_start);
		file_read_bytes(fd, &readbyte, 1);
		val = readbyte;
	} else {
		val = pak[4];
	}

	// we've read 6 bytes
	if ((val & 0xc0) != 0x40) {
		// mpeg1
		file_skip_bytes(fd, 12 - read_from_start); // skip 6 more bytes
		return;
	}
	file_skip_bytes(fd, 13 - read_from_start);
	file_read_bytes(fd, &readbyte, 1);
	stuffed = readbyte & 0x7;
	file_skip_bytes(fd, stuffed);
}

/*
 * find_pack_start
 * look for the pack start code in the file - read 512 bytes at a time,
 * searching for that code.
 * Note: we may also be okay looking for >= 00 00 01 bb
 */
static Bool find_pack_start (FILE *fd,
                             u8 *saved,
                             u32 len)
{
	u8 buffer[512];
	u32 buffer_on = 0, new_offset, scode;
	memcpy(buffer, saved, len);
	if (file_read_bytes(fd, buffer + len, sizeof(buffer) - len) == 0) {
		return 0;
	}
	while (1) {
		if (gf_mv12_next_start_code(buffer + buffer_on,
		                            sizeof(buffer) - buffer_on,
		                            &new_offset,
		                            &scode) >= 0) {
			buffer_on += new_offset;
			if (scode == MPEG2_PS_PACKSTART) {
				file_skip_bytes(fd, buffer_on - 512); // go back to header
				return 1;
			}
			buffer_on += 1;
		} else {
			len = 0;
			if (buffer[sizeof(buffer) - 3] == 0 &&
			        buffer[sizeof(buffer) - 2] == 0 &&
			        buffer[sizeof(buffer) - 1] == 1) {
				buffer[0] = 0;
				buffer[1] = 0;
				buffer[2] = 1;
				len = 3;
			} else if (*(u16 *)(buffer + sizeof(buffer) - 2) == 0) {
				buffer[0] = 0;
				buffer[1] = 0;
				len = 2;
			} else if (buffer[sizeof(buffer) - 1] == 0) {
				buffer[0] = 0;
				len = 1;
			}
			if (file_read_bytes(fd, buffer + len, sizeof(buffer) - len) == 0) {
				return 0;
			}
			buffer_on = 0;
		}
	}
	return 0;
}

/*
 * copy_bytes_to_pes_buffer - read pes_len bytes into the buffer,
 * adjusting it if we need it
 */
static void copy_bytes_to_pes_buffer (mpeg2ps_stream_t *sptr,
                                      u16 pes_len)
{
	u32 to_move;

	if (sptr->pes_buffer_size + pes_len > sptr->pes_buffer_size_max) {
		// if no room in the buffer, we'll move it - otherwise, just fill
		// note - we might want a better strategy about moving the buffer -
		// right now, we might be moving a number of bytes if we have a large
		// followed by large frame.
		to_move = sptr->pes_buffer_size - sptr->pes_buffer_on;
		memmove(sptr->pes_buffer,
		        sptr->pes_buffer + sptr->pes_buffer_on,
		        to_move);
		sptr->pes_buffer_size = to_move;
		sptr->pes_buffer_on = 0;
		if (to_move + pes_len > sptr->pes_buffer_size_max) {
			sptr->pes_buffer = (u8 *)gf_realloc(sptr->pes_buffer,
			                                    to_move + pes_len + 2048);
			sptr->pes_buffer_size_max = to_move + pes_len + 2048;
		}
	}
	file_read_bytes(sptr->m_fd, sptr->pes_buffer + sptr->pes_buffer_size, pes_len);
	sptr->pes_buffer_size += pes_len;
}

/*
 * read_to_next_pes_header - read the file, look for the next valid
 * pes header.  We will skip over PACK headers, but not over any of the
 * headers listed in 13818-1, table 2-18 - basically, anything with the
 * 00 00 01 and the next byte > 0xbb.
 * We return the pes len to read, and the "next byte"
 */
static Bool read_to_next_pes_header (FILE *fd,
                                     u8 *stream_id,
                                     u16 *pes_len)
{
	u32 hdr;
	u8 local[6];

	while (1) {
		// read the pes header
		if (file_read_bytes(fd, local, 6) == 0) {
			return 0;
		}

		hdr = convert32(local);
		// if we're not a 00 00 01, read until we get the next pack start
		// we might want to also read until next PES - look into that.
		if (((hdr & MPEG2_PS_START_MASK) != MPEG2_PS_START) ||
		        (hdr < MPEG2_PS_END)) {
			if (find_pack_start(fd, local, 6) == 0) {
				return 0;
			}
			continue;
		}
		if (hdr == MPEG2_PS_PACKSTART) {
			// pack start code - we can skip down
			adv_past_pack_hdr(fd, local, 6);
			continue;
		}
		if (hdr == MPEG2_PS_END) {
			file_skip_bytes(fd, -2);
			continue;
		}

		// we should have a valid stream and pes_len here...
		*stream_id = hdr & 0xff;
		*pes_len = convert16(local + 4);
		return 1;
	}
	return 0;
}

/*
 * read_pes_header_data
 * this should read past the pes header for the audio and video streams
 * it will store the timestamps if it reads them
 */
static Bool read_pes_header_data (FILE *fd,
                                  u16 orig_pes_len,
                                  u16 *pes_left,
                                  Bool *have_ts,
                                  mpeg2ps_ts_t *ts)
{
	u16 pes_len = orig_pes_len;
	u8 local[10];
	u32 hdr_len;

	ts->have_pts = 0;
	ts->have_dts = 0;
	if (have_ts) *have_ts = 0;
	if (file_read_bytes(fd, local, 1) == 0) {
		return 0;
	}
	pes_len--; // remove this first byte from length
	while (*local == 0xff) {
		if (file_read_bytes(fd, local, 1) == 0) {
			return 0;
		}
		pes_len--;
		if (pes_len == 0) {
			*pes_left = 0;
			return 1;
		}
	}
	if ((*local & 0xc0) == 0x40) {
		// buffer scale & size
		file_skip_bytes(fd, 1);
		if (file_read_bytes(fd, local, 1) == 0) {
			return 0;
		}
		pes_len -= 2;
	}

	if ((*local & 0xf0) == 0x20) {
		// mpeg-1 with pts
		if (file_read_bytes(fd, local + 1, 4) == 0) {
			return 0;
		}
		ts->have_pts = 1;
		ts->pts = ts->dts = read_pts(local);
		*have_ts = 1;
		pes_len -= 4;
	} else if ((*local & 0xf0) == 0x30) {
		// have mpeg 1 pts and dts
		if (file_read_bytes(fd, local + 1, 9) == 0) {
			return 0;
		}
		ts->have_pts = 1;
		ts->have_dts = 1;
		*have_ts = 1;
		ts->pts = read_pts(local);
		ts->dts = read_pts(local + 5);
		pes_len -= 9;
	} else if ((*local & 0xc0) == 0x80) {
		// mpeg2 pes header  - we're pointing at the flags field now
		if (file_read_bytes(fd, local + 1, 2) == 0) {
			return 0;
		}
		hdr_len = local[2];
		pes_len -= hdr_len + 2; // first byte removed already
		if ((local[1] & 0xc0) == 0x80) {
			// just pts
			ts->have_pts = 1;
			file_read_bytes(fd, local, 5);
			ts->pts = ts->dts = read_pts(local);
			*have_ts = 1;
			hdr_len -= 5;
		} else if ((local[1] & 0xc0) == 0xc0) {
			// pts and dts
			ts->have_pts = 1;
			ts->have_dts = 1;
			*have_ts = 1;
			file_read_bytes(fd, local, 10);
			ts->pts = read_pts(local);
			ts->dts = read_pts(local  + 5);
			hdr_len -= 10;
		}
		file_skip_bytes(fd, hdr_len);
	} else if (*local != 0xf) {
		file_skip_bytes(fd, pes_len);
		pes_len = 0;
	}
	*pes_left = pes_len;
	return 1;
}

static Bool search_for_next_pes_header (mpeg2ps_stream_t *sptr,
                                        u16 *pes_len,
                                        Bool *have_ts,
                                        s64 *found_loc)
{
	u8 stream_id;
	u8 local;
	s64 loc;
	while (1) {
		// this will read until we find the next pes.  We don't know if the
		// stream matches - this will read over pack headers
		if (read_to_next_pes_header(sptr->m_fd, &stream_id, pes_len) == 0) {
			return 0;
		}

		if (stream_id != sptr->m_stream_id) {
			file_skip_bytes(sptr->m_fd, *pes_len);
			continue;
		}
		loc = file_location(sptr->m_fd) - 6;
		// advance past header, reading pts
		if (read_pes_header_data(sptr->m_fd,
		                         *pes_len,
		                         pes_len,
		                         have_ts,
		                         &sptr->next_pes_ts) == 0) {
			return 0;
		}

		// If we're looking at a private stream, make sure that the sub-stream
		// matches.
		if (sptr->m_stream_id == 0xbd) {
			// ac3 or pcm
			file_read_bytes(sptr->m_fd, &local, 1);
			*pes_len -= 1;
			if (local != sptr->m_substream_id) {
				file_skip_bytes(sptr->m_fd, *pes_len);
				continue; // skip to the next one
			}
			*pes_len -= 3;
			file_skip_bytes(sptr->m_fd, 3); // 4 bytes - we don't need now...
			// we need more here...
		}
		if (have_ts) {
			mpeg2ps_record_pts(sptr, loc, &sptr->next_pes_ts);
		}
		if (found_loc != NULL) *found_loc = loc;
		return 1;
	}
	return 0;
}

/*
 * mpeg2ps_stream_read_next_pes_buffer - for the given stream,
 * go forward in the file until the next PES for the stream is read.  Read
 * the header (pts, dts), and read the data into the pes_buffer pointer
 */
static Bool mpeg2ps_stream_read_next_pes_buffer (mpeg2ps_stream_t *sptr)
{
	u16 pes_len;
	Bool have_ts;

	if (search_for_next_pes_header(sptr, &pes_len, &have_ts, NULL) == 0) {
		return 0;
	}

	copy_bytes_to_pes_buffer(sptr, pes_len);

	return 1;
}


/***************************************************************************
 * Frame reading routine.  For each stream, the fd's should be different.
 * we will read from the pes stream, and save it in the stream's pes buffer.
 * This will give us raw data that we can search through for frame headers,
 * and the like.  We shouldn't read more than we need - when we need to read,
 * we'll put the whole next pes buffer in the buffer
 *
 * Audio routines are of the format:
 *   look for header
 *   determine length
 *   make sure length is in buffer
 *
 * Video routines
 *   look for start header (GOP, SEQ, Picture)
 *   look for pict header
 *   look for next start (END, GOP, SEQ, Picture)
 *
 ***************************************************************************/
#define IS_MPEG_START(a) ((a) == 0xb3 || (a) == 0x00 || (a) == 0xb8)

static Bool
mpeg2ps_stream_find_mpeg_video_frame (mpeg2ps_stream_t *sptr)
{
	u32 offset, scode;
	Bool have_pict;
	Bool started_new_pes = 0;
	u32 start;
	/*
	 * First thing - determine if we have enough bytes to read the header.
	 * if we do, we have the correct timestamp.  If not, we read the new
	 * pes, so we'd want to use the timestamp we read.
	 */
	sptr->frame_ts = sptr->next_pes_ts;
	if (sptr->pes_buffer_size <= sptr->pes_buffer_on + 4) {
		if (sptr->pes_buffer_size != sptr->pes_buffer_on)
			started_new_pes = 1;
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	while (gf_mv12_next_start_code(sptr->pes_buffer + sptr->pes_buffer_on,
	                               sptr->pes_buffer_size - sptr->pes_buffer_on,
	                               &offset,
	                               &scode) < 0 ||
	        (!IS_MPEG_START(scode & 0xff))) {
		if (sptr->pes_buffer_size > 3)
			sptr->pes_buffer_on = sptr->pes_buffer_size - 3;
		else {
			sptr->pes_buffer_on = sptr->pes_buffer_size;
			started_new_pes = 1;
		}
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	sptr->pes_buffer_on += offset;
	if (offset == 0 && started_new_pes) {
		// nothing...  we've copied the timestamp already.
	} else {
		// we found the new start, but we pulled in a new pes header before
		// starting.  So, we want to use the header that we read.
		sptr->frame_ts = sptr->next_pes_ts; // set timestamp after searching
		// clear timestamp indication
		sptr->next_pes_ts.have_pts = sptr->next_pes_ts.have_dts = 0;
	}

	if (scode == MPEG12_PICTURE_START_CODE) {
		sptr->pict_header_offset = sptr->pes_buffer_on;
		have_pict = 1;
	} else have_pict = 0;

	start = 4 + sptr->pes_buffer_on;
	while (1) {

		if (gf_mv12_next_start_code(sptr->pes_buffer + start,
		                            sptr->pes_buffer_size - start,
		                            &offset,
		                            &scode) < 0) {
			start = sptr->pes_buffer_size - 3;
			start -= sptr->pes_buffer_on;
			sptr->pict_header_offset -= sptr->pes_buffer_on;
			if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
				return 0;
			}
			start += sptr->pes_buffer_on;
			sptr->pict_header_offset += sptr->pes_buffer_on;
		} else {
			start += offset;
			if (have_pict == 0) {
				if (scode == MPEG12_PICTURE_START_CODE) {
					have_pict = 1;
					sptr->pict_header_offset = start;
				}
			} else {
				if (IS_MPEG_START(scode & 0xff) ||
				        scode == MPEG12_SEQUENCE_END_START_CODE) {
					sptr->frame_len = start - sptr->pes_buffer_on;
					sptr->have_frame_loaded = 1;
					return 1;
				}
			}
			start += 4;
		}
	}
	return 0;
}

static Bool mpeg2ps_stream_find_ac3_frame (mpeg2ps_stream_t *sptr)
{
	u32 diff;
	Bool started_new_pes = 0;
	GF_AC3Config hdr;
	memset(&hdr, 0, sizeof(GF_AC3Config));
	sptr->frame_ts = sptr->next_pes_ts; // set timestamp after searching
	if (sptr->pes_buffer_size <= sptr->pes_buffer_on + 6) {
		if (sptr->pes_buffer_size != sptr->pes_buffer_on)
			started_new_pes = 1;
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	while (gf_ac3_parser(sptr->pes_buffer + sptr->pes_buffer_on,
	                     sptr->pes_buffer_size - sptr->pes_buffer_on,
	                     &diff,
	                     &hdr, 0) <= 0) {
		// don't have frame
		if (sptr->pes_buffer_size > 6) {
			sptr->pes_buffer_on = sptr->pes_buffer_size - 6;
			started_new_pes = 1;
		} else {
			sptr->pes_buffer_on = sptr->pes_buffer_size;
		}
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	sptr->frame_len = hdr.framesize;
	sptr->pes_buffer_on += diff;
	if (diff == 0 && started_new_pes) {
		// we might have a new PTS - but it's not here
	} else {
		sptr->frame_ts = sptr->next_pes_ts;
		sptr->next_pes_ts.have_dts = sptr->next_pes_ts.have_pts = 0;
	}
	while (sptr->pes_buffer_size - sptr->pes_buffer_on < sptr->frame_len) {
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	sptr->have_frame_loaded = 1;
	return 1;
}

static Bool mpeg2ps_stream_find_mp3_frame (mpeg2ps_stream_t *sptr)
{
	u32 diff, hdr;
	Bool started_new_pes = 0;

	sptr->frame_ts = sptr->next_pes_ts;
	if (sptr->pes_buffer_size <= sptr->pes_buffer_on + 4) {
		if (sptr->pes_buffer_size != sptr->pes_buffer_on)
			started_new_pes = 1;
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	while ((hdr=gf_mp3_get_next_header_mem(sptr->pes_buffer + sptr->pes_buffer_on,
	                                       sptr->pes_buffer_size - sptr->pes_buffer_on,
	                                       &diff) ) == 0) {
		// don't have frame
		if (sptr->pes_buffer_size > 3) {
			if (sptr->pes_buffer_on != sptr->pes_buffer_size) {
				sptr->pes_buffer_on = sptr->pes_buffer_size - 3;
			}
			started_new_pes = 1; // we have left over bytes...
		} else {
			sptr->pes_buffer_on = sptr->pes_buffer_size;
		}
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	// have frame.
	sptr->frame_len = gf_mp3_frame_size(hdr);
	sptr->pes_buffer_on += diff;
	if (diff == 0 && started_new_pes) {

	} else {
		sptr->frame_ts = sptr->next_pes_ts;
		sptr->next_pes_ts.have_dts = sptr->next_pes_ts.have_pts = 0;
	}
	while (sptr->pes_buffer_size - sptr->pes_buffer_on < sptr->frame_len) {
		if (mpeg2ps_stream_read_next_pes_buffer(sptr) == 0) {
			return 0;
		}
	}
	sptr->have_frame_loaded = 1;
	return 1;
}

/*
 * mpeg2ps_stream_read_frame.  read the correct frame based on stream type.
 * advance_pointers is 0 when we want to use the data
 */
static Bool mpeg2ps_stream_read_frame (mpeg2ps_stream_t *sptr,
                                       u8 **buffer,
                                       u32 *buflen,
                                       Bool advance_pointers)
{
	//  Bool done = 0;
	if (sptr->is_video) {
		if (mpeg2ps_stream_find_mpeg_video_frame(sptr)) {
			*buffer = sptr->pes_buffer + sptr->pes_buffer_on;
			*buflen = sptr->frame_len;
			if (advance_pointers) {
				sptr->pes_buffer_on += sptr->frame_len;
			}
			return 1;
		}
		return 0;
	} else if (sptr->m_stream_id == 0xbd) {
		// would need to handle LPCM here
		if (mpeg2ps_stream_find_ac3_frame(sptr)) {
			*buffer = sptr->pes_buffer + sptr->pes_buffer_on;
			*buflen = sptr->frame_len;
			if (advance_pointers)
				sptr->pes_buffer_on += sptr->frame_len;
			return 1;
		}
		return 0;
	} else if (mpeg2ps_stream_find_mp3_frame(sptr)) {
		*buffer = sptr->pes_buffer + sptr->pes_buffer_on;
		*buflen = sptr->frame_len;
		if (advance_pointers)
			sptr->pes_buffer_on += sptr->frame_len;
		return 1;
	}
	return 0;
}

/*
 * get_info_from_frame - we have a frame, get the info from it.
 */
static void get_info_from_frame (mpeg2ps_stream_t *sptr,
                                 u8 *buffer,
                                 u32 buflen)
{
	if (sptr->is_video) {
		if (MPEG12_ParseSeqHdr(buffer, buflen,
		                       &sptr->have_mpeg2,
		                       &sptr->h,
		                       &sptr->w,
		                       &sptr->frame_rate,
		                       &sptr->bit_rate,
		                       &sptr->par) < 0) {
			sptr->m_stream_id = 0;
			sptr->m_fd = FDNULL;
			return;
		}
		sptr->ticks_per_frame = (u64)(90000.0 / sptr->frame_rate);
		return;
	}

	if (sptr->m_stream_id >= 0xc0) {
		// mpeg audio
		u32 hdr = GF_4CC((u32)buffer[0],buffer[1],buffer[2],buffer[3]);

		sptr->channels = gf_mp3_num_channels(hdr);
		sptr->freq = gf_mp3_sampling_rate(hdr);
		sptr->samples_per_frame = gf_mp3_window_size(hdr);
		sptr->bitrate = gf_mp3_bit_rate(hdr) * 1000; // give bps, not kbps
		sptr->layer = gf_mp3_layer(hdr);
	} else if (sptr->m_stream_id == 0xbd) {
		if (sptr->m_substream_id >= 0xa0) {
			// PCM - ???
		} else if (sptr->m_substream_id >= 0x80) {
			u32 pos;
			GF_AC3Config hdr;
			memset(&hdr, 0, sizeof(GF_AC3Config));
			gf_ac3_parser(buffer, buflen, &pos, &hdr, 0);
			sptr->bitrate = gf_ac3_get_bitrate(hdr.brcode);
			sptr->freq = hdr.sample_rate;
			sptr->channels = hdr.streams[0].channels;
			sptr->samples_per_frame = 256 * 6;
		} else {
			return;
		}
	} else {
		return;
	}
}

/*
 * clear_stream_buffer - called when we seek to clear out any data in
 * the buffers
 */
static void clear_stream_buffer (mpeg2ps_stream_t *sptr)
{
	sptr->pes_buffer_on = sptr->pes_buffer_size = 0;
	sptr->frame_len = 0;
	sptr->have_frame_loaded = 0;
	sptr->next_pes_ts.have_dts = sptr->next_pes_ts.have_pts = 0;
	sptr->frame_ts.have_dts = sptr->frame_ts.have_pts = 0;
}

/*
 * convert_to_msec - convert ts (at 90000) to msec, based on base_ts and
 * frames_since_last_ts.
 */
static u64 convert_ts (mpeg2ps_stream_t *sptr,
                       mpeg2ps_ts_type_t ts_type,
                       u64 ts,
                       u64 base_ts,
                       u32 frames_since_ts)
{
	u64 ret, calc;
	ret = ts - base_ts;
	if (sptr->is_video) {
		// video
		ret += frames_since_ts * sptr->ticks_per_frame;
	} else if (sptr->freq) {
		// audio
		calc = (frames_since_ts * 90000 * sptr->samples_per_frame) / sptr->freq;
		ret += calc;
	}
	if (ts_type == TS_MSEC)
		ret /= (u64) (90); // * 1000 / 90000

	return ret;
}

/*
 * find_stream_from_id - given the stream, get the sptr.
 * only used in inital set up, really.  APIs use index into
 * video_streams and audio_streams arrays.
 */
static mpeg2ps_stream_t *find_stream_from_id (mpeg2ps_t *ps,
        u8 stream_id,
        u8 substream)
{
	u8 ix;
	if (stream_id >= 0xe0) {
		for (ix = 0; ix < ps->video_cnt; ix++) {
			if (ps->video_streams[ix]->m_stream_id == stream_id) {
				return ps->video_streams[ix];
			}
		}
	} else {
		for (ix = 0; ix < ps->audio_cnt; ix++) {
			if (ps->audio_streams[ix]->m_stream_id == stream_id &&
			        (stream_id != 0xbd ||
			         substream == ps->audio_streams[ix]->m_substream_id)) {
				return ps->audio_streams[ix];
			}
		}
	}
	return NULL;
}

/*
 * add_stream - add a new stream
 */
static Bool add_stream (mpeg2ps_t *ps,
                        u8 stream_id,
                        u8 substream,
                        s64 first_loc,
                        mpeg2ps_ts_t *ts)
{
	mpeg2ps_stream_t *sptr;

	sptr = find_stream_from_id(ps, stream_id, substream);
	if (sptr != NULL) return 0;

	// need to add

	sptr = mpeg2ps_stream_create(stream_id, substream);
	sptr->first_pes_loc = first_loc;
	if (ts == NULL ||
	        (ts->have_dts == 0 && ts->have_pts == 0)) {
		sptr->first_pes_has_dts = 0;
	} else {
		sptr->start_dts = ts->have_dts ? ts->dts : ts->pts;
		sptr->first_pes_has_dts = 1;
	}
	if (sptr->is_video) {
		// can't be more than 16 - e0 to ef...
		ps->video_streams[ps->video_cnt] = sptr;
		ps->video_cnt++;
	} else {
		if (ps->audio_cnt >= 32) {
			mpeg2ps_stream_destroy(sptr);
			return 0;
		}
		ps->audio_streams[ps->audio_cnt] = sptr;
		ps->audio_cnt++;
	}
	return 1;
}

static void check_fd_for_stream (mpeg2ps_t *ps,
                                 mpeg2ps_stream_t *sptr)
{
	if (sptr->m_fd != FDNULL) return;

	sptr->m_fd = file_open(ps->filename);
}

/*
 * advance_frame - when we're reading frames, this indicates that we're
 * done.  We will call this when we read a frame, but not when we
 * seek.  It allows us to leave the last frame we're seeking in the
 * buffer
 */
static void advance_frame (mpeg2ps_stream_t *sptr)
{
	sptr->pes_buffer_on += sptr->frame_len;
	sptr->have_frame_loaded = 0;
	if (sptr->frame_ts.have_dts || sptr->frame_ts.have_pts) {
		if (sptr->frame_ts.have_dts)
			sptr->last_ts = sptr->frame_ts.dts;
		else
			sptr->last_ts = sptr->frame_ts.pts;
		sptr->frames_since_last_ts = 0;
	} else {
		sptr->frames_since_last_ts++;
	}
}
/*
 * get_info_for_all_streams - loop through found streams - read an
 * figure out the info
 */
static void get_info_for_all_streams (mpeg2ps_t *ps)
{
	u8 stream_ix, max_ix, av;
	mpeg2ps_stream_t *sptr;
	u8 *buffer;
	u32 buflen;

	file_seek_to(ps->fd, 0);

	// av will be 0 for video streams, 1 for audio streams
	// av is just so I don't have to dup a lot of code that does the
	// same thing.
	for (av = 0; av < 2; av++) {
		if (av == 0) max_ix = ps->video_cnt;
		else max_ix = ps->audio_cnt;
		for (stream_ix = 0; stream_ix < max_ix; stream_ix++) {
			if (av == 0) sptr = ps->video_streams[stream_ix];
			else sptr = ps->audio_streams[stream_ix];

			// we don't open a separate file descriptor yet (only when they
			// start reading or seeking).  Use the one from the ps.
			sptr->m_fd = ps->fd; // for now
			clear_stream_buffer(sptr);
			if (mpeg2ps_stream_read_frame(sptr,
			                              &buffer,
			                              &buflen,
			                              0) == 0) {
				sptr->m_stream_id = 0;
				sptr->m_fd = FDNULL;
				continue;
			}
			get_info_from_frame(sptr, buffer, buflen);
			// here - if (sptr->first_pes_has_dts == 0) should be processed
			if ((sptr->first_pes_has_dts == 0) && sptr->m_fd) {
				u32 frames_from_beg = 0;
				Bool have_frame;
				do {
					advance_frame(sptr);
					have_frame =
					    mpeg2ps_stream_read_frame(sptr, &buffer, &buflen, 0);
					frames_from_beg++;
				} while (have_frame &&
				         sptr->frame_ts.have_dts == 0 &&
				         sptr->frame_ts.have_pts == 0 &&
				         frames_from_beg < 1000);
				if (have_frame == 0 ||
				        (sptr->frame_ts.have_dts == 0 &&
				         sptr->frame_ts.have_pts == 0)) {
				} else {
					sptr->start_dts = sptr->frame_ts.have_dts ? sptr->frame_ts.dts :
					                  sptr->frame_ts.pts;
					if (sptr->is_video) {
						sptr->start_dts -= frames_from_beg * sptr->ticks_per_frame;
					} else {
						u64 conv;
						conv = sptr->samples_per_frame * 90000;
						conv /= (u64)sptr->freq;
						sptr->start_dts -= conv;
					}
				}
			}
			clear_stream_buffer(sptr);
			sptr->m_fd = FDNULL;
		}
	}
}

/*
 * mpeg2ps_scan_file - read file, grabbing all the information that
 * we can out of it (what streams exist, timing, etc).
 */
static void mpeg2ps_scan_file (mpeg2ps_t *ps)
{
	u8 stream_id, stream_ix, substream, av_ix, max_cnt;
	u16 pes_len, pes_left;
	mpeg2ps_ts_t ts;
	s64 loc, first_video_loc = 0, first_audio_loc = 0;
	s64 check, orig_check;
	mpeg2ps_stream_t *sptr;
	Bool valid_stream;
	u8 *buffer;
	u32 buflen;
	Bool have_ts;

	ps->end_loc = file_size(ps->fd);
	orig_check = check = MAX(ps->end_loc / 50, 200 * 1024);

	/*
	 * This part reads and finds the streams.  We check up until we
	 * find audio and video plus a little, with a max of either 200K or
	 * the file size / 50
	 */
	loc = 0;
	while (read_to_next_pes_header(ps->fd, &stream_id, &pes_len) &&
	        loc < check) {
		pes_left = pes_len;
		if (stream_id >= 0xbd && stream_id < 0xf0) {
			loc = file_location(ps->fd) - 6;
			if (read_pes_header_data(ps->fd,
			                         pes_len,
			                         &pes_left,
			                         &have_ts,
			                         &ts) == 0) {
				return;
			}
			valid_stream = 0;
			substream = 0;
			if (stream_id == 0xbd) {
				if (file_read_bytes(ps->fd, &substream, 1) == 0) {
					return;
				}
				pes_left--; // remove byte we just read
				if ((substream >= 0x80 && substream < 0x90) ||
				        (substream >= 0xa0 && substream < 0xb0)) {
					valid_stream = 1;
				}
			} else if (stream_id >= 0xc0) {
				// audio and video
				valid_stream = 1;
			}
			if (valid_stream) {
				if (add_stream(ps, stream_id, substream, loc, &ts)) {
					// added
					if (stream_id >= 0xe0) {
						if (ps->video_cnt == 1) {
							first_video_loc = loc;
						}
					} else if (ps->audio_cnt == 1) {
						first_audio_loc = loc;
					}
					if (ps->audio_cnt > 0 && ps->video_cnt > 0) {
						s64 diff;
						if (first_audio_loc > first_video_loc)
							diff = first_audio_loc - first_video_loc;
						else
							diff = first_video_loc - first_audio_loc;
						diff *= 2;
						diff += first_video_loc;
						if (diff < check) {
							check = diff;
						}
					}
				}
			}
		}
		file_skip_bytes(ps->fd, pes_left);
	}
	if (ps->video_cnt == 0 && ps->audio_cnt == 0) {
		return;
	}
	/*
	 * Now, we go to close to the end, and try to find the last
	 * dts that we can
	 */
	file_seek_to(ps->fd, ps->end_loc - orig_check);

	while (read_to_next_pes_header(ps->fd, &stream_id, &pes_len)) {
		loc = file_location(ps->fd) - 6;
		if (stream_id == 0xbd || (stream_id >= 0xc0 && stream_id < 0xf0)) {
			if (read_pes_header_data(ps->fd,
			                         pes_len,
			                         &pes_left,
			                         &have_ts,
			                         &ts) == 0) {
				return;
			}
			if (stream_id == 0xbd) {
				if (file_read_bytes(ps->fd, &substream, 1) == 0) {
					return;
				}
				pes_left--; // remove byte we just read
				if (!((substream >= 0x80 && substream < 0x90) ||
				        (substream >= 0xa0 && substream < 0xb0))) {
					file_skip_bytes(ps->fd, pes_left);
					continue;
				}
			} else {
				substream = 0;
			}
			sptr = find_stream_from_id(ps, stream_id, substream);
			if (sptr == NULL) {
				add_stream(ps, stream_id, substream, 0, NULL);
				sptr = find_stream_from_id(ps, stream_id, substream);
			}
			if (sptr != NULL && have_ts) {
				sptr->end_dts = ts.have_dts ? ts.dts : ts.pts;
				sptr->end_dts_loc = loc;
			}
			file_skip_bytes(ps->fd, pes_left);
		}
	}

	/*
	 * Now, get the info for all streams, so we can use it again
	 * we could do this before the above, I suppose
	 */
	get_info_for_all_streams(ps);

	ps->first_dts = (u64) -1;

	/*
	 * we need to find the earliest start pts - we use that to calc
	 * the rest of the timing, so we're 0 based.
	 */
	for (av_ix = 0; av_ix < 2; av_ix++) {
		if (av_ix == 0) max_cnt = ps->video_cnt;
		else max_cnt = ps->audio_cnt;

		for (stream_ix = 0; stream_ix < max_cnt; stream_ix++) {
			sptr = av_ix == 0 ? ps->video_streams[stream_ix] :
			       ps->audio_streams[stream_ix];
			if (sptr != NULL && sptr->start_dts < ps->first_dts) {
				ps->first_dts = sptr->start_dts;
			}
		}
	}

	/*
	 * Now, for each thread, we'll start at the last pts location, and
	 * read the number of frames.  This will give us a max time
	 */
	for (av_ix = 0; av_ix < 2; av_ix++) {
		if (av_ix == 0) max_cnt = ps->video_cnt;
		else max_cnt = ps->audio_cnt;
		for (stream_ix = 0; stream_ix < max_cnt; stream_ix++) {
			u32 frame_cnt_since_last;
			sptr = av_ix == 0 ? ps->video_streams[stream_ix] :
			       ps->audio_streams[stream_ix];

			// pick up here - find the final time...
			if (sptr && (sptr->end_dts_loc != 0)) {
				file_seek_to(ps->fd, sptr->end_dts_loc);
				sptr->m_fd = ps->fd;
				frame_cnt_since_last = 0;
				clear_stream_buffer(sptr);
				while (mpeg2ps_stream_read_frame(sptr,
				                                 &buffer,
				                                 &buflen,
				                                 1)) {
					frame_cnt_since_last++;
				}
				sptr->m_fd = FDNULL;
				clear_stream_buffer(sptr);
				ps->max_time = MAX(ps->max_time,
				                   convert_ts(sptr,
				                              TS_MSEC,
				                              sptr->end_dts,
				                              ps->first_dts,
				                              frame_cnt_since_last));
			}
		}
	}

	ps->max_dts = (ps->max_time * 90) + ps->first_dts;
	file_seek_to(ps->fd, 0);
}

/*************************************************************************
 * API routines
 *************************************************************************/
u64 mpeg2ps_get_max_time_msec (mpeg2ps_t *ps)
{
	return ps->max_time;
}

u32 mpeg2ps_get_video_stream_count (mpeg2ps_t *ps)
{
	return ps->video_cnt;
}

#define NUM_ELEMENTS_IN_ARRAY(name) ((sizeof((name))) / (sizeof(*(name))))

// routine to check stream number passed.
static Bool invalid_video_streamno (mpeg2ps_t *ps, u32 streamno)
{
	if (streamno >= NUM_ELEMENTS_IN_ARRAY(ps->video_streams)) return 1;
	if (ps->video_streams[streamno] == NULL) return 1;
	return 0;
}

#if 0 //unused
const char *mpeg2ps_get_video_stream_name (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return 0;
	}
	if (ps->video_streams[streamno]->have_mpeg2) {
		return "Mpeg-2";
	}
	return "Mpeg-1";
}
#endif

mpeg2ps_video_type_t mpeg2ps_get_video_stream_type (mpeg2ps_t *ps,
        u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return MPEG_VIDEO_UNKNOWN;
	}
	return ps->video_streams[streamno]->have_mpeg2 ? MPEG_VIDEO_MPEG2 : MPEG_VIDEO_MPEG1;
}

u32 mpeg2ps_get_video_stream_width (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return 0;
	}
	return ps->video_streams[streamno]->w;
}

u32 mpeg2ps_get_video_stream_height (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return 0;
	}
	return ps->video_streams[streamno]->h;
}

u32 mpeg2ps_get_video_stream_aspect_ratio (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return 0;
	}
	return ps->video_streams[streamno]->par;
}

Double mpeg2ps_get_video_stream_bitrate (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return 0;
	}
	return ps->video_streams[streamno]->bit_rate;
}

Double mpeg2ps_get_video_stream_framerate (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return 0;
	}
	return ps->video_streams[streamno]->frame_rate;
}

u32 mpeg2ps_get_video_stream_id(mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) {
		return 0;
	}
	return ps->video_streams[streamno]->m_stream_id;
}

static Bool invalid_audio_streamno (mpeg2ps_t *ps, u32 streamno)
{
	if (streamno >= NUM_ELEMENTS_IN_ARRAY(ps->audio_streams)) return 1;
	if (ps->audio_streams[streamno] == NULL) return 1;
	return 0;
}

u32 mpeg2ps_get_audio_stream_count (mpeg2ps_t *ps)
{
	return ps->audio_cnt;
}

#if 0 //unused
const char *mpeg2ps_get_audio_stream_name (mpeg2ps_t *ps,
        u32 streamno)
{
	if (invalid_audio_streamno(ps, streamno)) {
		return "none";
	}
	if (ps->audio_streams[streamno]->m_stream_id >= 0xc0) {
		switch (ps->audio_streams[streamno]->layer) {
		case 1:
			return "MP1";
		case 2:
			return "MP2";
		case 3:
			return "MP3";
		}
		return "unknown mpeg layer";
	}
	if (ps->audio_streams[streamno]->m_substream_id >= 0x80 &&
	        ps->audio_streams[streamno]->m_substream_id < 0x90)
		return "AC3";

	return "LPCM";
}
#endif

mpeg2ps_audio_type_t mpeg2ps_get_audio_stream_type (mpeg2ps_t *ps,
        u32 streamno)
{
	if (invalid_audio_streamno(ps, streamno)) {
		return MPEG_AUDIO_UNKNOWN;
	}
	if (ps->audio_streams[streamno]->m_stream_id >= 0xc0) {
		return MPEG_AUDIO_MPEG;
	}
	if (ps->audio_streams[streamno]->m_substream_id >= 0x80 &&
	        ps->audio_streams[streamno]->m_substream_id < 0x90)
		return MPEG_AUDIO_AC3;

	return MPEG_AUDIO_LPCM;
}

u32 mpeg2ps_get_audio_stream_sample_freq (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_audio_streamno(ps, streamno)) {
		return 0;
	}
	return ps->audio_streams[streamno]->freq;
}

u32 mpeg2ps_get_audio_stream_channels (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_audio_streamno(ps, streamno)) {
		return 0;
	}
	return ps->audio_streams[streamno]->channels;
}

u32 mpeg2ps_get_audio_stream_bitrate (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_audio_streamno(ps, streamno)) {
		return 0;
	}
	return ps->audio_streams[streamno]->bitrate;
}

u32 mpeg2ps_get_audio_stream_id (mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_audio_streamno(ps, streamno)) {
		return 0;
	}
	return ps->audio_streams[streamno]->m_stream_id;
}


mpeg2ps_t *mpeg2ps_init (const char *filename)
{
	mpeg2ps_t *ps;
	GF_SAFEALLOC(ps, mpeg2ps_t);

	if (ps == NULL) {
		return NULL;
	}
	memset(ps, 0, sizeof(*ps));
	ps->fd = file_open(filename);
	if (file_okay(ps->fd) == 0) {
		gf_free(ps);
		return NULL;
	}

	ps->filename = gf_strdup(filename);
	mpeg2ps_scan_file(ps);
	if (ps->video_cnt == 0 && ps->audio_cnt == 0) {
		mpeg2ps_close(ps);
		return NULL;
	}
	return ps;
}

void mpeg2ps_close (mpeg2ps_t *ps)
{
	u32 ix;
	if (ps == NULL) return;
	for (ix = 0; ix < ps->video_cnt; ix++) {
		mpeg2ps_stream_destroy(ps->video_streams[ix]);
		ps->video_streams[ix] = NULL;
	}
	for (ix = 0; ix < ps->audio_cnt; ix++) {
		mpeg2ps_stream_destroy(ps->audio_streams[ix]);
		ps->audio_streams[ix] = NULL;
	}

	if (ps->filename) gf_free(ps->filename);
	if (ps->fd) file_close(ps->fd);
	gf_free(ps);
}

/*
 * check_fd_for_stream will make sure we have a fd for the stream we're
 * trying to read - we use a different fd for each stream
 */

/*
 * stream_convert_frame_ts_to_msec - given a "read" frame, we'll
 * calculate the msec and freq timestamps.  This can be called more
 * than 1 time, if needed, without changing any variables, such as
 * frames_since_last_ts, which gets updated in advance_frame
 */
static u64 stream_convert_frame_ts_to_msec (mpeg2ps_stream_t *sptr,
        mpeg2ps_ts_type_t ts_type,
        u64 base_dts,
        u32 *freq_ts)
{
	u64 calc_ts;
	u32 frames_since_last = 0;
	u64 freq_conv;

	calc_ts = sptr->last_ts;
	if (sptr->frame_ts.have_dts) calc_ts = sptr->frame_ts.dts;
	else if (sptr->frame_ts.have_pts) calc_ts = sptr->frame_ts.dts;
	else frames_since_last = sptr->frames_since_last_ts + 1;

	if (freq_ts != NULL) {
		freq_conv = calc_ts - base_dts;
		freq_conv *= sptr->freq;
		freq_conv /= 90000;
		freq_conv += frames_since_last * sptr->samples_per_frame;
		*freq_ts = (u32) (freq_conv & 0xffffffff);
	}
	return convert_ts(sptr, ts_type, calc_ts, base_dts, frames_since_last);
}

/*
 * mpeg2ps_get_video_frame - gets the next frame
 */
Bool mpeg2ps_get_video_frame(mpeg2ps_t *ps, u32 streamno,
                             u8 **buffer,
                             u32 *buflen,
                             u8 *frame_type,
                             mpeg2ps_ts_type_t ts_type,
                             u64 *decode_timestamp, u64 *compose_timestamp)
{
	u64 dts, cts;
	mpeg2ps_stream_t *sptr;
	if (invalid_video_streamno(ps, streamno)) return 0;

	sptr = ps->video_streams[streamno];
	check_fd_for_stream(ps, sptr);

	if (sptr->have_frame_loaded == 0) {
		// if we don't have the frame in the buffer (like after a seek),
		// read the next frame
		if (mpeg2ps_stream_find_mpeg_video_frame(sptr) == 0) {
			return 0;
		}
	}
	*buffer = sptr->pes_buffer + sptr->pes_buffer_on;
	*buflen = sptr->frame_len;
	// determine frame type
	if (frame_type != NULL) {
		*frame_type = MPEG12_PictHdrType(sptr->pes_buffer +
		                                 sptr->pict_header_offset);
	}

	// set the timestamps
	if (sptr->frame_ts.have_pts)
		cts = sptr->frame_ts.pts;
	else
		cts = sptr->last_ts + (1+sptr->frames_since_last_ts) * sptr->ticks_per_frame;
	if (sptr->frame_ts.have_dts)
		dts = sptr->frame_ts.dts;
	else
		dts = cts;

	if (decode_timestamp) *decode_timestamp = dts;
	if (compose_timestamp) *compose_timestamp = cts;

	//indicate that we read this frame - get ready for the next one.
	advance_frame(sptr);


	return 1;
}


// see above comments
Bool mpeg2ps_get_audio_frame(mpeg2ps_t *ps, u32 streamno,
                             u8 **buffer,
                             u32 *buflen,
                             mpeg2ps_ts_type_t ts_type,
                             u32 *freq_timestamp,
                             u64 *timestamp)
{
	mpeg2ps_stream_t *sptr;
	if (invalid_audio_streamno(ps, streamno)) return 0;

	sptr = ps->audio_streams[streamno];
	check_fd_for_stream(ps, sptr);

	if (sptr->have_frame_loaded == 0) {
		if (mpeg2ps_stream_read_frame(sptr, buffer, buflen, 0) == 0)
			return 0;
	}

	if (freq_timestamp) {
		/*ts = */stream_convert_frame_ts_to_msec(sptr,
		                                     ts_type,
		                                     ps->first_dts,
		                                     freq_timestamp);
	}
	if (timestamp != NULL) {
		*timestamp = sptr->frame_ts.have_pts ? sptr->frame_ts.pts : sptr->frame_ts.dts;
	}
	advance_frame(sptr);
	return 1;
}

#if 0 //unused
u64 mpeg2ps_get_ps_size(mpeg2ps_t *ps)
{
	return file_size(ps->fd);
}
s64 mpeg2ps_get_video_pos(mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_video_streamno(ps, streamno)) return 0;
	return gf_ftell(ps->video_streams[streamno]->m_fd);
}
s64 mpeg2ps_get_audio_pos(mpeg2ps_t *ps, u32 streamno)
{
	if (invalid_audio_streamno(ps, streamno)) return 0;
	return gf_ftell(ps->audio_streams[streamno]->m_fd);
}
#endif


/***************************************************************************
 * seek routines
 ***************************************************************************/
/*
 * mpeg2ps_binary_seek - look for a pts that's close to the one that
 * we're looking for.  We have a start ts and location, an end ts and
 * location, and what we're looking for
 */
static void mpeg2ps_binary_seek (mpeg2ps_t *ps,
				 mpeg2ps_stream_t *sptr,
				 u64 search_dts,
				 u64 start_dts,
				 u64 start_loc,
				 u64 end_dts,
				u64 end_loc)
{
  u64 dts_perc;
  u64 loc;
  u16 pes_len;
  Bool have_ts = GF_FALSE;
  u64 found_loc;
  u64 found_dts;

  while (1) {
    /*
     * It's not a binary search as much as using a percentage between
     * the start and end dts to start.  We subtract off a bit, so we
     * approach from the beginning of the file - we're more likely to
     * hit a pts that way
     */
    dts_perc = (search_dts - start_dts) * 1000 / (end_dts - start_dts);
    dts_perc -= dts_perc % 10;

    loc = ((end_loc - start_loc) * dts_perc) / 1000;

    if (loc == start_loc || loc == end_loc) return;

    clear_stream_buffer(sptr);
    file_seek_to(sptr->m_fd, start_loc + loc);

    // we'll look for the next pes header for this stream that has a ts.
    do {
      if (search_for_next_pes_header(sptr,
				     &pes_len,
				     &have_ts,
				     &found_loc) == GF_FALSE) {
	return;
      }
      if (have_ts == GF_FALSE) {
	file_skip_bytes(sptr->m_fd, pes_len);
      }
    } while (have_ts == GF_FALSE);

    // record that spot...
    mpeg2ps_record_pts(sptr, found_loc, &sptr->next_pes_ts);

    found_dts = sptr->next_pes_ts.have_dts ?
      sptr->next_pes_ts.dts : sptr->next_pes_ts.pts;
    /*
     * Now, if we're before the search ts, and within 5 seconds,
     * we'll say we're close enough
     */
    if (found_dts + (5 * 90000) > search_dts &&
	found_dts < search_dts) {
      file_seek_to(sptr->m_fd, found_loc);
      return; // found it - we can seek from here
    }
    /*
     * otherwise, move the head or the tail (most likely the head).
     */
    if (found_dts > search_dts) {
      if (found_dts >= end_dts) {
	file_seek_to(sptr->m_fd, found_loc);
	return;
      }
      end_loc = found_loc;
      end_dts = found_dts;
    } else {
      if (found_dts <= start_dts) {
	file_seek_to(sptr->m_fd, found_loc);
	return;
      }
      start_loc = found_loc;
      start_dts = found_dts;
    }
  }
}



static mpeg2ps_record_pes_t *search_for_ts (mpeg2ps_stream_t *sptr,
				     u64 dts)
{
  mpeg2ps_record_pes_t *p, *q;
  u64 p_diff, q_diff;
  if (sptr->record_last == NULL) return NULL;

  if (dts > sptr->record_last->dts) return sptr->record_last;

  if (dts < sptr->record_first->dts) return NULL;
  if (dts == sptr->record_first->dts) return sptr->record_first;

  p = sptr->record_first;
  q = p->next_rec;

  while (q != NULL && q->dts > dts) {
    p = q;
    q = q->next_rec;
  }
  if (q == NULL) {
    return sptr->record_last;
  }

  p_diff = dts - p->dts;
  q_diff = q->dts - dts;

  if (p_diff < q_diff) return p;
  if (q_diff > 90000) return p;

  return q;
}


/*
 * mpeg2ps_seek_frame - seek to the next timestamp after the search timestamp
 * First, find a close DTS (usually minus 5 seconds or closer), then
 * read frames until we get the frame after the timestamp.
 */
static Bool mpeg2ps_seek_frame (mpeg2ps_t *ps,
				mpeg2ps_stream_t *sptr,
				u64 search_msec_timestamp)
{
  u64 dts;
  mpeg2ps_record_pes_t *rec;
  u64 msec_ts;
  u8 *buffer;
  u32 buflen;

  check_fd_for_stream(ps, sptr);
  clear_stream_buffer(sptr);

  if (search_msec_timestamp <= 1000) { // first second, start from begin...
    file_seek_to(sptr->m_fd, sptr->first_pes_loc);
    return GF_TRUE;
  }
  dts = search_msec_timestamp * 90; // 1000 timescale to 90000 timescale
  dts += ps->first_dts;

  /*
   * see if the recorded data has anything close
   */
  rec = search_for_ts(sptr, dts);
  if (rec != NULL) {
    // see if it is close
    // if we're plus or minus a second, seek to that.
    if (rec->dts + 90000 >= dts && rec->dts <= dts + 90000) {
      file_seek_to(sptr->m_fd, rec->location);
      return GF_TRUE;
    }
    // at this point, rec is > a distance.  If within 5 or so seconds,

    if (rec->dts + (5 * 90000) < dts) {
      // more than 5 seconds away - skip and search
      if (rec->next_rec == NULL) {
		  mpeg2ps_binary_seek(ps, sptr, dts,
			    rec->dts, rec->location,
			    sptr->end_dts, sptr->end_dts_loc);
      } else {
		  mpeg2ps_binary_seek(ps, sptr, dts,
			    rec->dts, rec->location,
			    rec->next_rec->dts, rec->next_rec->location);
      }
    }
    // otherwise, frame by frame search...
  } else {
    // we weren't able to find anything from the recording
    mpeg2ps_binary_seek(ps, sptr, dts,
			sptr->start_dts, sptr->first_pes_loc,
			sptr->end_dts, sptr->end_dts_loc);
  }
  /*
   * Now, the fun part - read frames until we're just past the time
   */
  clear_stream_buffer(sptr); // clear out any data, so we can read it
  do {
    if (mpeg2ps_stream_read_frame(sptr, &buffer, &buflen, GF_FALSE) == GF_FALSE)
      return GF_FALSE;

    msec_ts = stream_convert_frame_ts_to_msec(sptr, TS_MSEC,
					      ps->first_dts, NULL);

    if (msec_ts < search_msec_timestamp) {
      // only advance the frame if we're not greater than the timestamp
      advance_frame(sptr);
    }
  } while (msec_ts < search_msec_timestamp);

  return GF_TRUE;
}


/*
 * mpeg2ps_seek_video_frame - seek to the location that we're interested
 * in, then scroll up to the next I frame
 */
Bool mpeg2ps_seek_video_frame (mpeg2ps_t *ps, u32 streamno,
			       u64 msec_timestamp)
{
  mpeg2ps_stream_t *sptr;

  if (invalid_video_streamno(ps, streamno)) return GF_FALSE;

  sptr = ps->video_streams[streamno];
  if (mpeg2ps_seek_frame(ps,
			 sptr,
			 msec_timestamp)
			  == GF_FALSE) return GF_FALSE;

  if (sptr->have_frame_loaded == GF_FALSE) {
    return GF_FALSE;
  }
  return GF_TRUE;
}
/*
 * mpeg2ps_seek_audio_frame - go to the closest audio frame after the
 * timestamp
 */
Bool mpeg2ps_seek_audio_frame (mpeg2ps_t *ps,
			       u32 streamno,
			       u64 msec_timestamp)
{
  //  off_t closest_pes;
  mpeg2ps_stream_t *sptr;

  if (invalid_audio_streamno(ps, streamno)) return GF_FALSE;

  sptr = ps->audio_streams[streamno];
  if (mpeg2ps_seek_frame(ps,
			 sptr,
			 msec_timestamp) == GF_FALSE) return GF_FALSE;

  return GF_TRUE;
}

u64 mpeg2ps_get_first_cts(mpeg2ps_t *ps)
{
	return ps->first_dts;
}


#endif /*GPAC_DISABLE_MPEG2PS*/
