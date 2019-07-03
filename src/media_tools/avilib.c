/*
 *  avilib.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  multiple audio track support Copyright (C) 2002 Thomas Östreich
 *
 *  Original code:
 *  Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *
 *  This file is part of transcode, a linux video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License aint with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/setup.h>

#ifndef GPAC_DISABLE_AVILIB

#include <gpac/internal/avilib.h>


#define INFO_LIST

// add a new riff chunk after XX MB
//#define NEW_RIFF_THRES (1900*1024*1024)
#define NEW_RIFF_THRES (1900*1024*1024)
//#define NEW_RIFF_THRES (10*1024*1024)

// Maximum number of indices per stream
#define NR_IXNN_CHUNKS 96


#define DEBUG_ODML
#undef DEBUG_ODML

/* The following variable indicates the kind of error */

int AVI_errno = 0;

#define MAX_INFO_STRLEN 64
static char id_str[MAX_INFO_STRLEN];

#define FRAME_RATE_SCALE 1000000

/*******************************************************************
 *                                                                 *
 *    Utilities for writing an AVI File                            *
 *                                                                 *
 *******************************************************************/

static u32 avi_read(FILE *fd, char *buf, u32 len)
{
	s32 n = 0;
	u32 r = 0;

	while (r < len) {
		n = (s32) fread(buf + r, 1, len - r, fd);
		if (n == 0) break;
		if (n < 0) return r;
		r += n;
	}

	return r;
}

static u32 avi_write (FILE *fd, char *buf, u32 len)
{
	s32 n = 0;
	u32 r = 0;

	while (r < len) {
		n = (u32) gf_fwrite (buf + r, 1, len - r, fd);
		if (n < 0)
			return n;

		r += n;
	}
	return r;
}

/* HEADERBYTES: The number of bytes to reserve for the header */

#define HEADERBYTES 2048

/* AVI_MAX_LEN: The maximum length of an AVI file, we stay a bit below
    the 2GB limit (Remember: 2*10^9 is smaller than 2 GB) */

#define AVI_MAX_LEN (UINT_MAX-(1<<20)*16-HEADERBYTES)

#define PAD_EVEN(x) ( ((x)+1) & ~1 )


/* Copy n into dst as a 4 or 2 byte, little endian number.
   Should also work on big endian machines */

static void long2str(unsigned char *dst, s32 n)
{
	dst[0] = (n    )&0xff;
	dst[1] = (n>> 8)&0xff;
	dst[2] = (n>>16)&0xff;
	dst[3] = (n>>24)&0xff;
}

#ifdef WORDS_BIGENDIAN
static void short2str(unsigned char *dst, s32 n)
{
	dst[0] = (n    )&0xff;
	dst[1] = (n>> 8)&0xff;
}
#endif

/* Convert a string of 4 or 2 bytes to a number,
   also working on big endian machines */

static u64 str2ullong(unsigned char *str)
{
	u64 r = (str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24));
	u64 s = (str[4] | (str[5]<<8) | (str[6]<<16) | (str[7]<<24));
#ifdef __GNUC__
	return ((s<<32)&0xffffffff00000000ULL)|(r&0xffffffff);
#else
	return ((s<<32)&0xffffffff00000000)|(r&0xffffffff);
#endif
}

static u32 str2ulong(unsigned char *str)
{
	return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}
static u32 str2ushort(unsigned char *str)
{
	return ( str[0] | (str[1]<<8) );
}

// bit 31 denotes a keyframe
static u32 str2ulong_len (unsigned char *str)
{
	return str2ulong(str) & 0x7fffffff;
}


// if bit 31 is 0, its a keyframe
static u32 str2ulong_key (unsigned char *str)
{
	u32 c = str2ulong(str);
	c &= 0x80000000;
	if (c == 0) return 0x10;
	else return 0;
}

/* Calculate audio sample size from number of bits and number of channels.
   This may have to be adjusted for eg. 12 bits and stereo */

static int avi_sampsize(avi_t *AVI, int j)
{
	int s;
	s = ((AVI->track[j].a_bits+7)/8)*AVI->track[j].a_chans;
	//   if(s==0) s=1; /* avoid possible zero divisions */
	if(s<4) s=4; /* avoid possible zero divisions */
	return s;
}

/* Add a chunk (=tag and data) to the AVI file,
   returns -1 on write error, 0 on success */

static int avi_add_chunk(avi_t *AVI, unsigned char *tag, unsigned char *data, u32 length)
{
	unsigned char c[8];
	char p=0;

	/* Copy tag and length int c, so that we need only 1 write system call
	   for these two values */

	memcpy(c,tag,4);
	long2str(c+4,length);

	/* Output tag, length and data, restore previous position
	   if the write fails */

	if( avi_write(AVI->fdes,(char *)c,8) != 8 ||
	        avi_write(AVI->fdes,(char *)data,length) != length ||
	        avi_write(AVI->fdes,&p,length&1) != (length&1)) // if len is uneven, write a pad byte
	{
		gf_fseek(AVI->fdes,AVI->pos,SEEK_SET);
		AVI_errno = AVI_ERR_WRITE;
		return -1;
	}

	/* Update file position */

	AVI->pos += 8 + PAD_EVEN(length);

	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] pos=%lu %s\n", AVI->pos, tag));

	return 0;
}

#define OUTD(n) long2str((unsigned char*) (ix00+bl),(s32)n); bl+=4
#define OUTW(n) ix00[bl] = (n)&0xff; ix00[bl+1] = (n>>8)&0xff; bl+=2
#define OUTC(n) ix00[bl] = (n)&0xff; bl+=1
#define OUTS(s) memcpy(ix00+bl,s,4); bl+=4

// this does the physical writeout of the ix## structure
static int avi_ixnn_entry(avi_t *AVI, avistdindex_chunk *ch, avisuperindex_entry *en)
{
	int bl;
	u32 k;
	unsigned int max = ch->nEntriesInUse * sizeof (u32) * ch->wLongsPerEntry + 24; // header
	char *ix00 = (char *)gf_malloc (max);
	char dfcc[5];
	memcpy (dfcc, ch->fcc, 4);
	dfcc[4] = 0;

	bl = 0;

	if (en) {
		en->qwOffset = AVI->pos;
		en->dwSize = max;
		//en->dwDuration = ch->nEntriesInUse -1; // NUMBER OF stream ticks == frames for video/samples for audio
	}

#ifdef DEBUG_ODML
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] ODML Write %s: Entries %ld size %d \n", dfcc, ch->nEntriesInUse, max));
#endif

	//OUTS(ch->fcc);
	//OUTD(max);
	OUTW(ch->wLongsPerEntry);
	OUTC(ch->bIndexSubType);
	OUTC(ch->bIndexType);
	OUTD(ch->nEntriesInUse);
	OUTS(ch->dwChunkId);
	OUTD(ch->qwBaseOffset&0xffffffff);
	OUTD((ch->qwBaseOffset>>32)&0xffffffff);
	OUTD(ch->dwReserved3);

	for (k = 0; k < ch->nEntriesInUse; k++) {
		OUTD(ch->aIndex[k].dwOffset);
		OUTD(ch->aIndex[k].dwSize);

	}
	avi_add_chunk (AVI, (unsigned char*)ch->fcc, (unsigned char*)ix00, max);

	gf_free(ix00);

	return 0;
}
#undef OUTS
#undef OUTW
#undef OUTD
#undef OUTC

// inits a super index structure including its enclosed stdindex
static int avi_init_super_index(avi_t *AVI, unsigned char *idxtag, avisuperindex_chunk **si)
{
	int k;

	avisuperindex_chunk *sil = NULL;

	if ((sil = (avisuperindex_chunk *) gf_malloc (sizeof (avisuperindex_chunk))) == NULL) {
		AVI_errno = AVI_ERR_NO_MEM;
		return -1;
	}
	memcpy (sil->fcc, "indx", 4);
	sil->dwSize = 0; // size of this chunk
	sil->wLongsPerEntry = 4;
	sil->bIndexSubType = 0;
	sil->bIndexType = AVI_INDEX_OF_INDEXES;
	sil->nEntriesInUse = 0; // none are in use
	memcpy (sil->dwChunkId, idxtag, 4);
	memset (sil->dwReserved, 0, sizeof (sil->dwReserved));

	// NR_IXNN_CHUNKS == allow 32 indices which means 32 GB files -- arbitrary
	sil->aIndex = (avisuperindex_entry *) gf_malloc (sil->wLongsPerEntry * NR_IXNN_CHUNKS * sizeof (void*));
	if (!sil->aIndex) {
		AVI_errno = AVI_ERR_NO_MEM;
		return -1;
	}
	memset (sil->aIndex, 0, sil->wLongsPerEntry * NR_IXNN_CHUNKS * sizeof (u32));

	sil->stdindex = (avistdindex_chunk **)gf_malloc (NR_IXNN_CHUNKS * sizeof (avistdindex_chunk *));
	if (!sil->stdindex) {
		AVI_errno = AVI_ERR_NO_MEM;
		return -1;
	}
	for (k = 0; k < NR_IXNN_CHUNKS; k++) {
		sil->stdindex[k] = (avistdindex_chunk *) gf_malloc (sizeof (avistdindex_chunk));
		// gets rewritten later
		sil->stdindex[k]->qwBaseOffset = (u64)k * NEW_RIFF_THRES;
		sil->stdindex[k]->aIndex = NULL;
	}

	*si = sil;

	return 0;
}

// fills an alloc'ed stdindex structure and mallocs some entries for the actual chunks
static int avi_add_std_index(avi_t *AVI, unsigned char *idxtag, unsigned char *strtag,
                             avistdindex_chunk *stdil)
{

	memcpy (stdil->fcc, idxtag, 4);
	stdil->dwSize = 4096;
	stdil->wLongsPerEntry = 2; //sizeof(avistdindex_entry)/sizeof(u32);
	stdil->bIndexSubType = 0;
	stdil->bIndexType = AVI_INDEX_OF_CHUNKS;
	stdil->nEntriesInUse = 0;

	// cp 00db ChunkId
	memcpy(stdil->dwChunkId, strtag, 4);

	//stdil->qwBaseOffset = AVI->video_superindex->aIndex[ cur_std_idx ]->qwOffset;

	stdil->aIndex = (avistdindex_entry *)gf_malloc(stdil->dwSize * sizeof (u32) * stdil->wLongsPerEntry);

	if (!stdil->aIndex) {
		AVI_errno = AVI_ERR_NO_MEM;
		return -1;
	}


	return 0;
}

static int avi_add_odml_index_entry_core(avi_t *AVI, int flags, u64 pos, unsigned int len, avistdindex_chunk *si)
{
	u32 cur_chunk_idx;
	// put new chunk into index
	si->nEntriesInUse++;
	cur_chunk_idx = si->nEntriesInUse-1;

	// need to fetch more memory
	if (cur_chunk_idx >= si->dwSize) {
		si->dwSize += 4096;
		si->aIndex = (avistdindex_entry *)gf_realloc ( si->aIndex, si->dwSize * sizeof (u32) * si->wLongsPerEntry);
	}

	if(len>AVI->max_len) AVI->max_len=len;

	// if bit 31 is set, it is NOT a keyframe
	if (flags != 0x10) {
		len |= 0x80000000;
	}

	si->aIndex [ cur_chunk_idx ].dwSize = len;
	si->aIndex [ cur_chunk_idx ].dwOffset = (u32) (pos - si->qwBaseOffset + 8);

	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] ODML: POS: 0x%lX\n", si->aIndex [ cur_chunk_idx ].dwOffset));

	return 0;
}

static int avi_add_odml_index_entry(avi_t *AVI, unsigned char *tag, int flags, u64 pos, unsigned int len)
{
	char fcc[5];

	int audio = (strchr ((char*)tag, 'w')?1:0);
	int video = !audio;

	unsigned int cur_std_idx;
	u32 audtr;
	s64 towrite = 0;

	if (video) {

		if (!AVI->video_superindex) {
			if (avi_init_super_index(AVI, (unsigned char *)"ix00", &AVI->video_superindex) < 0) return -1;
			AVI->video_superindex->nEntriesInUse++;
			cur_std_idx = AVI->video_superindex->nEntriesInUse-1;

			if (avi_add_std_index (AVI, (unsigned char *)"ix00", (unsigned char *)"00db", AVI->video_superindex->stdindex[ cur_std_idx ]) < 0)
				return -1;
		} // init

	} // video

	if (audio) {

		fcc[0] = 'i';
		fcc[1] = 'x';
		fcc[2] = tag[0];
		fcc[3] = tag[1];
		fcc[4] = '\0';
		if (!AVI->track[AVI->aptr].audio_superindex) {

#ifdef DEBUG_ODML
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] ODML: fcc = %s\n", fcc));
#endif
			if (avi_init_super_index(AVI, (unsigned char *)fcc, &AVI->track[AVI->aptr].audio_superindex) < 0) return -1;


			AVI->track[AVI->aptr].audio_superindex->nEntriesInUse++;

			sprintf(fcc, "ix%02d", AVI->aptr+1);
			if (avi_add_std_index (AVI, (unsigned char *)fcc, tag, AVI->track[AVI->aptr].audio_superindex->stdindex[
			                           AVI->track[AVI->aptr].audio_superindex->nEntriesInUse - 1 ]) < 0
			   ) return -1;
		} // init

	}

	towrite = 0;
	if (AVI->video_superindex) {

		cur_std_idx = AVI->video_superindex->nEntriesInUse-1;
		towrite += AVI->video_superindex->stdindex[cur_std_idx]->nEntriesInUse*8
		           + 4+4+2+1+1+4+4+8+4;
		if (cur_std_idx == 0) {
			towrite += AVI->n_idx*16 + 8;
			towrite += HEADERBYTES;
		}
	}

	for (audtr=0; audtr<AVI->anum; audtr++) {
		if (AVI->track[audtr].audio_superindex) {
			cur_std_idx = AVI->track[audtr].audio_superindex->nEntriesInUse-1;
			towrite += AVI->track[audtr].audio_superindex->stdindex[cur_std_idx]->nEntriesInUse*8
			           + 4+4+2+1+1+4+4+8+4;
		}
	}
	towrite += len + (len&1) + 8;

	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] ODML: towrite = 0x%llX = %"LLD"\n", towrite, towrite));

	if (AVI->video_superindex &&
	        (s64)(AVI->pos+towrite) > (s64)((s64)NEW_RIFF_THRES*AVI->video_superindex->nEntriesInUse)) {

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] Adding a new RIFF chunk: %d\n", AVI->video_superindex->nEntriesInUse));

		// rotate ALL indices
		AVI->video_superindex->nEntriesInUse++;
		cur_std_idx = AVI->video_superindex->nEntriesInUse-1;

		if (AVI->video_superindex->nEntriesInUse > NR_IXNN_CHUNKS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] Internal error in avilib - redefine NR_IXNN_CHUNKS\n"));
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] cur_std_idx=%d NR_IXNN_CHUNKS=%d"
			                                        "POS=%"LLD" towrite=%"LLD"\n",
			                                        cur_std_idx,NR_IXNN_CHUNKS, AVI->pos, towrite));
			return -1;
		}

		if (avi_add_std_index (AVI, (unsigned char *)"ix00", (unsigned char *)"00db", AVI->video_superindex->stdindex[ cur_std_idx ]) < 0)
			return -1;

		for (audtr = 0; audtr < AVI->anum; audtr++) {
			char aud[5];
			if (!AVI->track[audtr].audio_superindex) {
				// not initialized -> no index
				continue;
			}
			AVI->track[audtr].audio_superindex->nEntriesInUse++;

			sprintf(fcc, "ix%02d", audtr+1);
			sprintf(aud, "0%01dwb", audtr+1);
			if (avi_add_std_index (AVI, (unsigned char *)fcc, (unsigned char *)aud, AVI->track[audtr].audio_superindex->stdindex[
			                           AVI->track[audtr].audio_superindex->nEntriesInUse - 1 ]) < 0
			   ) return -1;
		}

		// write the new riff;
		if (cur_std_idx > 0) {

			// dump the _previous_ == already finished index
			avi_ixnn_entry (AVI, AVI->video_superindex->stdindex[cur_std_idx - 1],
			                &AVI->video_superindex->aIndex[cur_std_idx - 1]);
			AVI->video_superindex->aIndex[cur_std_idx - 1].dwDuration =
			    AVI->video_superindex->stdindex[cur_std_idx - 1]->nEntriesInUse - 1;

			for (audtr = 0; audtr < AVI->anum; audtr++) {

				if (!AVI->track[audtr].audio_superindex) {
					// not initialized -> no index
					continue;
				}
				avi_ixnn_entry (AVI, AVI->track[audtr].audio_superindex->stdindex[cur_std_idx - 1],
				                &AVI->track[audtr].audio_superindex->aIndex[cur_std_idx - 1]);

				AVI->track[audtr].audio_superindex->aIndex[cur_std_idx - 1].dwDuration =
				    AVI->track[audtr].audio_superindex->stdindex[cur_std_idx - 1]->nEntriesInUse - 1;
				if (AVI->track[audtr].a_fmt == 0x1) {
					AVI->track[audtr].audio_superindex->aIndex[cur_std_idx - 1].dwDuration *=
					    AVI->track[audtr].a_bits*AVI->track[audtr].a_rate*AVI->track[audtr].a_chans/800;
				}
			}

			// XXX: dump idx1 structure
			if (cur_std_idx == 1) {
				avi_add_chunk(AVI, (unsigned char *)"idx1", (unsigned char *)AVI->idx, AVI->n_idx*16);
				// qwBaseOffset will contain the start of the second riff chunk
			}
			// Fix the Offsets later at closing time
			avi_add_chunk(AVI, (unsigned char *)"RIFF", (unsigned char *)"AVIXLIST\0\0\0\0movi", 16);

			AVI->video_superindex->stdindex[ cur_std_idx ]->qwBaseOffset = AVI->pos -16 -8;
#ifdef DEBUG_ODML
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] ODML: RIFF No.%02d at Offset 0x%llX\n", cur_std_idx, AVI->pos -16 -8));
#endif

			for (audtr = 0; audtr < AVI->anum; audtr++) {
				if (AVI->track[audtr].audio_superindex)
					AVI->track[audtr].audio_superindex->stdindex[ cur_std_idx ]->qwBaseOffset =
					    AVI->pos -16 -8;

			}

			// now we can be sure
			AVI->is_opendml++;
		}

	}


	if (video) {
		avi_add_odml_index_entry_core(AVI, flags, AVI->pos, len,
		                              AVI->video_superindex->stdindex[ AVI->video_superindex->nEntriesInUse-1 ]);

		AVI->total_frames++;
	} // video

	if (audio) {
		avi_add_odml_index_entry_core(AVI, flags, AVI->pos, len,
		                              AVI->track[AVI->aptr].audio_superindex->stdindex[
		                                  AVI->track[AVI->aptr].audio_superindex->nEntriesInUse-1 ]);
	}


	return 0;
}

// #undef NR_IXNN_CHUNKS

static int avi_add_index_entry(avi_t *AVI, unsigned char *tag, int flags, u64 pos, u64 len)
{
	void *ptr;

	if(AVI->n_idx>=AVI->max_idx) {
		ptr = gf_realloc((void *)AVI->idx,(AVI->max_idx+4096)*16);

		if(ptr == 0) {
			AVI_errno = AVI_ERR_NO_MEM;
			return -1;
		}
		AVI->max_idx += 4096;
		AVI->idx = (unsigned char((*)[16]) ) ptr;
	}

	/* Add index entry */

	//   GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] INDEX %s %ld %lu %lu\n", tag, flags, pos, len));

	memcpy(AVI->idx[AVI->n_idx],tag,4);
	long2str(AVI->idx[AVI->n_idx]+ 4,flags);
	long2str(AVI->idx[AVI->n_idx]+ 8, (s32) pos);
	long2str(AVI->idx[AVI->n_idx]+12, (s32) len);

	/* Update counter */

	AVI->n_idx++;

	if(len>AVI->max_len) AVI->max_len=(u32) len;

	return 0;
}

#if 0
/* Returns 1 if more audio is in that video junk */
int AVI_can_read_audio(avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		return -1;
	}
	if(!AVI->video_index)         {
		return -1;
	}
	if(!AVI->track[AVI->aptr].audio_index)         {
		return -1;
	}

	// is it -1? the last ones got left out --tibit
	//if (AVI->track[AVI->aptr].audio_posc>=AVI->track[AVI->aptr].audio_chunks-1) {
	if (AVI->track[AVI->aptr].audio_posc>=AVI->track[AVI->aptr].audio_chunks) {
		return 0;
	}

	if (AVI->video_pos >= AVI->video_frames) return 1;

	if (AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].pos < AVI->video_index[AVI->video_pos].pos) return 1;
	else return 0;
}
#endif

/*
   AVI_open_output_file: Open an AVI File and write a bunch
                         of zero bytes as space for the header.

   returns a pointer to avi_t on success, a zero pointer on error
*/

GF_EXPORT
avi_t* AVI_open_output_file(char * filename)
{
	avi_t *AVI;
	int i;

	unsigned char AVI_header[HEADERBYTES];

	/* Allocate the avi_t struct and zero it */

	AVI = (avi_t *) gf_malloc(sizeof(avi_t));
	if(AVI==0)
	{
		AVI_errno = AVI_ERR_NO_MEM;
		return 0;
	}
	memset((void *)AVI,0,sizeof(avi_t));

	AVI->fdes = gf_fopen(filename, "w+b");
	if (!AVI->fdes )
	{
		AVI_errno = AVI_ERR_OPEN;
		gf_free(AVI);
		return 0;
	}

	/* Write out HEADERBYTES bytes, the header will go here
	   when we are finished with writing */

	for (i=0; i<HEADERBYTES; i++) AVI_header[i] = 0;
	i = avi_write(AVI->fdes,(char *)AVI_header,HEADERBYTES);
	if (i != HEADERBYTES)
	{
		gf_fclose(AVI->fdes);
		AVI_errno = AVI_ERR_WRITE;
		gf_free(AVI);
		return 0;
	}

	AVI->pos  = HEADERBYTES;
	AVI->mode = AVI_MODE_WRITE; /* open for writing */

	//init
	AVI->anum = 0;
	AVI->aptr = 0;

	return AVI;
}

GF_EXPORT
void AVI_set_video(avi_t *AVI, int width, int height, double fps, char *compressor)
{
	/* may only be called if file is open for writing */

	if(AVI->mode==AVI_MODE_READ) return;

	AVI->width  = width;
	AVI->height = height;
	AVI->fps    = fps;

	if(strncmp(compressor, "RGB", 3)==0) {
		memset(AVI->compressor, 0, 4);
	} else {
		memcpy(AVI->compressor,compressor,4);
	}

	AVI->compressor[4] = 0;

	avi_update_header(AVI);
}

GF_EXPORT
void AVI_set_audio(avi_t *AVI, int channels, int rate, int bits, int format, int mp3rate)
{
	/* may only be called if file is open for writing */

	if(AVI->mode==AVI_MODE_READ) return;

	//inc audio tracks
	AVI->aptr=AVI->anum;
	++AVI->anum;

	if(AVI->anum > AVI_MAX_TRACKS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] error - only %d audio tracks supported\n", AVI_MAX_TRACKS));
		exit(1);
	}

	AVI->track[AVI->aptr].a_chans = channels;
	AVI->track[AVI->aptr].a_rate  = rate;
	AVI->track[AVI->aptr].a_bits  = bits;
	AVI->track[AVI->aptr].a_fmt   = format;
	AVI->track[AVI->aptr].mp3rate = mp3rate;

	avi_update_header(AVI);
}

#define OUT4CC(s) \
   if(nhb<=HEADERBYTES-4) memcpy(AVI_header+nhb,s,4); nhb += 4

#define OUTLONG(n) \
   if(nhb<=HEADERBYTES-4) long2str(AVI_header+nhb, (s32)(n)); nhb += 4

#define OUTSHRT(n) \
   if(nhb<=HEADERBYTES-2) { \
      AVI_header[nhb  ] = (u8) ((n   )&0xff); \
      AVI_header[nhb+1] = (u8) ((n>>8)&0xff); \
   } \
   nhb += 2

#define OUTCHR(n) \
   if(nhb<=HEADERBYTES-1) { \
      AVI_header[nhb  ] = (n   )&0xff; \
   } \
   nhb += 1

#define OUTMEM(d, s) \
   { \
     u32 s_ = (u32) (s); \
     if(nhb + s_ <= HEADERBYTES) \
        memcpy(AVI_header+nhb, (d), s_); \
     nhb += s_; \
   }


//ThOe write preliminary AVI file header: 0 frames, max vid/aud size
int avi_update_header(avi_t *AVI)
{
	int njunk, sampsize, hasIndex, ms_per_frame, frate, flag;
	int movi_len, hdrl_start, strl_start;
	u32 j;
	unsigned char AVI_header[HEADERBYTES];
	u32 nhb;
	unsigned int xd_size, xd_size_align2;

	//assume max size
	movi_len = AVI_MAX_LEN - HEADERBYTES + 4;

	//assume index will be written
	hasIndex=1;

	if(AVI->fps < 0.001) {
		frate=0;
		ms_per_frame=0;
	} else {
		frate = (int) (FRAME_RATE_SCALE*AVI->fps + 0.5);
		ms_per_frame=(int) (1000000/AVI->fps + 0.5);
	}

	/* Prepare the file header */

	nhb = 0;

	/* The RIFF header */

	OUT4CC ("RIFF");
	OUTLONG(movi_len);    // assume max size
	OUT4CC ("AVI ");

	/* Start the header list */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	hdrl_start = nhb;  /* Store start position */
	OUT4CC ("hdrl");

	/* The main AVI header */

	/* The Flags in AVI File header */

#define AVIF_HASINDEX           0x00000010      /* Index at end of file */
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800      /* Use CKType to find key frames */
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000

	OUT4CC ("avih");
	OUTLONG(56);                 /* # of bytes to follow */
	OUTLONG(ms_per_frame);       /* Microseconds per frame */
	//ThOe ->0
	//   OUTLONG(10000000);           /* MaxBytesPerSec, I hope this will never be used */
	OUTLONG(0);
	OUTLONG(0);                  /* PaddingGranularity (whatever that might be) */
	/* Other sources call it 'reserved' */
	flag = AVIF_ISINTERLEAVED;
	if(hasIndex) flag |= AVIF_HASINDEX;
	if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
	OUTLONG(flag);               /* Flags */
	OUTLONG(0);                  // no frames yet
	OUTLONG(0);                  /* InitialFrames */

	OUTLONG(AVI->anum+1);

	OUTLONG(0);                  /* SuggestedBufferSize */
	OUTLONG(AVI->width);         /* Width */
	OUTLONG(AVI->height);        /* Height */
	/* MS calls the following 'reserved': */
	OUTLONG(0);                  /* TimeScale:  Unit used to measure time */
	OUTLONG(0);                  /* DataRate:   Data rate of playback     */
	OUTLONG(0);                  /* StartTime:  Starting time of AVI data */
	OUTLONG(0);                  /* DataLength: Size of AVI data chunk    */


	/* Start the video stream list ---------------------------------- */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	strl_start = nhb;  /* Store start position */
	OUT4CC ("strl");

	/* The video stream header */

	OUT4CC ("strh");
	OUTLONG(56);                 /* # of bytes to follow */
	OUT4CC ("vids");             /* Type */
	OUT4CC (AVI->compressor);    /* Handler */
	OUTLONG(0);                  /* Flags */
	OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
	OUTLONG(0);                  /* InitialFrames */
	OUTLONG(FRAME_RATE_SCALE);              /* Scale */
	OUTLONG(frate);              /* Rate: Rate/Scale == samples/second */
	OUTLONG(0);                  /* Start */
	OUTLONG(0);                  // no frames yet
	OUTLONG(0);                  /* SuggestedBufferSize */
	OUTLONG(-1);                 /* Quality */
	OUTLONG(0);                  /* SampleSize */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	//   OUTLONG(0);                  /* Frame */
	//OUTLONG(0);                  /* Frame */

	/* The video stream format */

	xd_size        = AVI->extradata_size;
	xd_size_align2 = (AVI->extradata_size+1) & ~1;

	OUT4CC ("strf");
	OUTLONG(40 + xd_size_align2);/* # of bytes to follow */
	OUTLONG(40 + xd_size);	/* Size */
	OUTLONG(AVI->width);         /* Width */
	OUTLONG(AVI->height);        /* Height */
	OUTSHRT(1);
	OUTSHRT(24);     /* Planes, Count */
	OUT4CC (AVI->compressor);    /* Compression */
	// ThOe (*3)
	OUTLONG(AVI->width*AVI->height*3);  /* SizeImage (in bytes?) */
	OUTLONG(0);                  /* XPelsPerMeter */
	OUTLONG(0);                  /* YPelsPerMeter */
	OUTLONG(0);                  /* ClrUsed: Number of colors used */
	OUTLONG(0);                  /* ClrImportant: Number of colors important */

	// write extradata
	if (xd_size > 0 && AVI->extradata) {
		OUTMEM(AVI->extradata, xd_size);
		if (xd_size != xd_size_align2) {
			OUTCHR(0);
		}
	}

	/* Finish stream list, i.e. put number of bytes in the list to proper pos */

	long2str(AVI_header+strl_start-4,nhb-strl_start);


	/* Start the audio stream list ---------------------------------- */

	for(j=0; j<AVI->anum; ++j) {

		sampsize = avi_sampsize(AVI, j);

		OUT4CC ("LIST");
		OUTLONG(0);        /* Length of list in bytes, don't know yet */
		strl_start = nhb;  /* Store start position */
		OUT4CC ("strl");

		/* The audio stream header */

		OUT4CC ("strh");
		OUTLONG(56);            /* # of bytes to follow */
		OUT4CC ("auds");

		// -----------
		// ThOe
		OUTLONG(0);             /* Format (Optionally) */
		// -----------

		OUTLONG(0);             /* Flags */
		OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
		OUTLONG(0);             /* InitialFrames */

		// ThOe /4
		OUTLONG(sampsize/4);      /* Scale */
		OUTLONG(1000*AVI->track[j].mp3rate/8);
		OUTLONG(0);             /* Start */
		OUTLONG(4*AVI->track[j].audio_bytes/sampsize);   /* Length */
		OUTLONG(0);             /* SuggestedBufferSize */
		OUTLONG(-1);            /* Quality */

		// ThOe /4
		OUTLONG(sampsize/4);    /* SampleSize */

		OUTLONG(0);             /* Frame */
		OUTLONG(0);             /* Frame */
		//       OUTLONG(0);             /* Frame */
		//OUTLONG(0);             /* Frame */

		/* The audio stream format */

		OUT4CC ("strf");
		OUTLONG(16);                   /* # of bytes to follow */
		OUTSHRT(AVI->track[j].a_fmt);           /* Format */
		OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */
		OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */
		// ThOe
		OUTLONG(1000*AVI->track[j].mp3rate/8);
		//ThOe (/4)

		OUTSHRT(sampsize/4);           /* BlockAlign */


		OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */

		/* Finish stream list, i.e. put number of bytes in the list to proper pos */

		long2str(AVI_header+strl_start-4,nhb-strl_start);
	}

	/* Finish header list */

	long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);


	/* Calculate the needed amount of junk bytes, output junk */

	njunk = HEADERBYTES - nhb - 8 - 12;

	/* Safety first: if njunk <= 0, somebody has played with
	   HEADERBYTES without knowing what (s)he did.
	   This is a fatal error */

	if(njunk<=0)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] AVI_close_output_file: # of header bytes too small\n"));
		exit(1);
	}

	OUT4CC ("JUNK");
	OUTLONG(njunk);
	memset(AVI_header+nhb,0,njunk);

	nhb += njunk;

	/* Start the movi list */

	OUT4CC ("LIST");
	OUTLONG(movi_len); /* Length of list in bytes */
	OUT4CC ("movi");

	/* Output the header, truncate the file to the number of bytes
	   actually written, report an error if someting goes wrong */

	if ( (gf_fseek(AVI->fdes, 0, SEEK_SET) ==(u64)-1) ||
	        avi_write(AVI->fdes,(char *)AVI_header,HEADERBYTES)!=HEADERBYTES ||
	        (gf_fseek(AVI->fdes,AVI->pos,SEEK_SET)==(u64)-1)
	   ) {
		AVI_errno = AVI_ERR_CLOSE;
		return -1;
	}

	return 0;
}


//SLM
#ifndef S_IRUSR
#define S_IRWXU       00700       /* read, write, execute: owner */
#define S_IRUSR       00400       /* read permission: owner */
#define S_IWUSR       00200       /* write permission: owner */
#define S_IXUSR       00100       /* execute permission: owner */
#define S_IRWXG       00070       /* read, write, execute: group */
#define S_IRGRP       00040       /* read permission: group */
#define S_IWGRP       00020       /* write permission: group */
#define S_IXGRP       00010       /* execute permission: group */
#define S_IRWXO       00007       /* read, write, execute: other */
#define S_IROTH       00004       /* read permission: other */
#define S_IWOTH       00002       /* write permission: other */
#define S_IXOTH       00001       /* execute permission: other */
#endif

/*
  Write the header of an AVI file and close it.
  returns 0 on success, -1 on write error.
*/

static int avi_close_output_file(avi_t *AVI)
{
	int ret, njunk, sampsize, hasIndex, ms_per_frame, frate, idxerror, flag;
	u64 movi_len;
	int hdrl_start, strl_start;
	u32 j;
	unsigned char AVI_header[HEADERBYTES];
	int nhb;
	unsigned int xd_size, xd_size_align2;

#ifdef INFO_LIST
	int info_len;
	int id_len, real_id_len;
	int info_start_pos;
//   time_t calptr;
#endif

	/* Calculate length of movi list */

	// dump the rest of the index
	if (AVI->is_opendml) {
		int cur_std_idx = AVI->video_superindex->nEntriesInUse-1;
		u32 audtr;

#ifdef DEBUG_ODML
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] ODML dump the rest indices\n"));
#endif
		avi_ixnn_entry (AVI, AVI->video_superindex->stdindex[cur_std_idx],
		                &AVI->video_superindex->aIndex[cur_std_idx]);

		AVI->video_superindex->aIndex[cur_std_idx].dwDuration =
		    AVI->video_superindex->stdindex[cur_std_idx]->nEntriesInUse - 1;

		for (audtr = 0; audtr < AVI->anum; audtr++) {
			if (!AVI->track[audtr].audio_superindex) {
				// not initialized -> no index
				continue;
			}
			avi_ixnn_entry (AVI, AVI->track[audtr].audio_superindex->stdindex[cur_std_idx],
			                &AVI->track[audtr].audio_superindex->aIndex[cur_std_idx]);
			AVI->track[audtr].audio_superindex->aIndex[cur_std_idx].dwDuration =
			    AVI->track[audtr].audio_superindex->stdindex[cur_std_idx]->nEntriesInUse - 1;
			if (AVI->track[audtr].a_fmt == 0x1) {
				AVI->track[audtr].audio_superindex->aIndex[cur_std_idx].dwDuration *=
				    AVI->track[audtr].a_bits*AVI->track[audtr].a_rate*AVI->track[audtr].a_chans/800;
			}
		}
		// The AVI->video_superindex->nEntriesInUse contains the offset
		AVI->video_superindex->stdindex[ cur_std_idx+1 ]->qwBaseOffset = AVI->pos;
	}

	if (AVI->is_opendml) {
		// Correct!
		movi_len = AVI->video_superindex->stdindex[ 1 ]->qwBaseOffset - HEADERBYTES+4 - AVI->n_idx*16 - 8;
	} else {
		movi_len = AVI->pos - HEADERBYTES + 4;
	}


	/* Try to ouput the index entries. This may fail e.g. if no space
	   is left on device. We will report this as an error, but we still
	   try to write the header correctly (so that the file still may be
	   readable in the most cases */

	idxerror = 0;
	hasIndex = 1;
	if (!AVI->is_opendml) {
		//   GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] pos=%lu, index_len=%ld             \n", AVI->pos, AVI->n_idx*16));
		ret = avi_add_chunk(AVI, (unsigned char *)"idx1", (unsigned char *)AVI->idx, AVI->n_idx*16);
		hasIndex = (ret==0);
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] pos=%lu, index_len=%d\n", AVI->pos, hasIndex));

		if(ret) {
			idxerror = 1;
			AVI_errno = AVI_ERR_WRITE_INDEX;
		}
	}

	/* Calculate Microseconds per frame */

	if(AVI->fps < 0.001) {
		frate=0;
		ms_per_frame=0;
	} else {
		frate = (int) (FRAME_RATE_SCALE*AVI->fps + 0.5);
		ms_per_frame=(int) (1000000/AVI->fps + 0.5);
	}

	/* Prepare the file header */

	nhb = 0;

	/* The RIFF header */

	OUT4CC ("RIFF");
	if (AVI->is_opendml) {
		OUTLONG(AVI->video_superindex->stdindex[ 1 ]->qwBaseOffset - 8);    /* # of bytes to follow */
	} else {
		OUTLONG(AVI->pos - 8);    /* # of bytes to follow */
	}

	OUT4CC ("AVI ");

	/* Start the header list */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	hdrl_start = nhb;  /* Store start position */
	OUT4CC ("hdrl");

	/* The main AVI header */

	/* The Flags in AVI File header */

#define AVIF_HASINDEX           0x00000010      /* Index at end of file */
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800      /* Use CKType to find key frames */
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000

	OUT4CC ("avih");
	OUTLONG(56);                 /* # of bytes to follow */
	OUTLONG(ms_per_frame);       /* Microseconds per frame */
	//ThOe ->0
	//   OUTLONG(10000000);           /* MaxBytesPerSec, I hope this will never be used */
	OUTLONG(0);
	OUTLONG(0);                  /* PaddingGranularity (whatever that might be) */
	/* Other sources call it 'reserved' */
	flag = AVIF_ISINTERLEAVED;
	if(hasIndex) flag |= AVIF_HASINDEX;
	if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
	OUTLONG(flag);               /* Flags */
	OUTLONG(AVI->video_frames);  /* TotalFrames */
	OUTLONG(0);                  /* InitialFrames */

	OUTLONG(AVI->anum+1);
//   if (AVI->track[0].audio_bytes)
//      { OUTLONG(2); }           /* Streams */
//   else
//      { OUTLONG(1); }           /* Streams */

	OUTLONG(0);                  /* SuggestedBufferSize */
	OUTLONG(AVI->width);         /* Width */
	OUTLONG(AVI->height);        /* Height */
	/* MS calls the following 'reserved': */
	OUTLONG(0);                  /* TimeScale:  Unit used to measure time */
	OUTLONG(0);                  /* DataRate:   Data rate of playback     */
	OUTLONG(0);                  /* StartTime:  Starting time of AVI data */
	OUTLONG(0);                  /* DataLength: Size of AVI data chunk    */


	/* Start the video stream list ---------------------------------- */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	strl_start = nhb;  /* Store start position */
	OUT4CC ("strl");

	/* The video stream header */

	OUT4CC ("strh");
	OUTLONG(56);                 /* # of bytes to follow */
	OUT4CC ("vids");             /* Type */
	OUT4CC (AVI->compressor);    /* Handler */
	OUTLONG(0);                  /* Flags */
	OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
	OUTLONG(0);                  /* InitialFrames */
	OUTLONG(FRAME_RATE_SCALE);   /* Scale */
	OUTLONG(frate);              /* Rate: Rate/Scale == samples/second */
	OUTLONG(0);                  /* Start */
	OUTLONG(AVI->video_frames);  /* Length */
	OUTLONG(AVI->max_len);       /* SuggestedBufferSize */
	OUTLONG(0);                  /* Quality */
	OUTLONG(0);                  /* SampleSize */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	//OUTLONG(0);                  /* Frame */
	//OUTLONG(0);                  /* Frame */

	/* The video stream format */

	xd_size        = AVI->extradata_size;
	xd_size_align2 = (AVI->extradata_size+1) & ~1;

	OUT4CC ("strf");
	OUTLONG(40 + xd_size_align2);/* # of bytes to follow */
	OUTLONG(40 + xd_size);	/* Size */
	OUTLONG(AVI->width);         /* Width */
	OUTLONG(AVI->height);        /* Height */
	OUTSHRT(1);
	OUTSHRT(24);     /* Planes, Count */
	OUT4CC (AVI->compressor);    /* Compression */
	// ThOe (*3)
	OUTLONG(AVI->width*AVI->height*3);  /* SizeImage (in bytes?) */
	OUTLONG(0);                  /* XPelsPerMeter */
	OUTLONG(0);                  /* YPelsPerMeter */
	OUTLONG(0);                  /* ClrUsed: Number of colors used */
	OUTLONG(0);                  /* ClrImportant: Number of colors important */

	// write extradata if present
	if (xd_size > 0 && AVI->extradata) {
		OUTMEM(AVI->extradata, xd_size);
		if (xd_size != xd_size_align2) {
			OUTCHR(0);
		}
	}

	// dump index of indices for audio
	if (AVI->is_opendml) {
		u32 k;

		OUT4CC(AVI->video_superindex->fcc);
		OUTLONG(2+1+1+4+4+3*4 + AVI->video_superindex->nEntriesInUse * (8+4+4));
		OUTSHRT(AVI->video_superindex->wLongsPerEntry);
		OUTCHR(AVI->video_superindex->bIndexSubType);
		OUTCHR(AVI->video_superindex->bIndexType);
		OUTLONG(AVI->video_superindex->nEntriesInUse);
		OUT4CC(AVI->video_superindex->dwChunkId);
		OUTLONG(0);
		OUTLONG(0);
		OUTLONG(0);


		for (k = 0; k < AVI->video_superindex->nEntriesInUse; k++) {
			u32 r = (u32) ((AVI->video_superindex->aIndex[k].qwOffset >> 32) & 0xffffffff);
			u32 s = (u32) ((AVI->video_superindex->aIndex[k].qwOffset) & 0xffffffff);

			OUTLONG(s);
			OUTLONG(r);
			OUTLONG(AVI->video_superindex->aIndex[k].dwSize);
			OUTLONG(AVI->video_superindex->aIndex[k].dwDuration);
		}

	}

	/* Finish stream list, i.e. put number of bytes in the list to proper pos */

	long2str(AVI_header+strl_start-4,nhb-strl_start);

	/* Start the audio stream list ---------------------------------- */

	for(j=0; j<AVI->anum; ++j) {

		//if (AVI->track[j].a_chans && AVI->track[j].audio_bytes)
		{
			unsigned int nBlockAlign = 0;
			unsigned int avgbsec = 0;
			unsigned int scalerate = 0;

			sampsize = avi_sampsize(AVI, j);
			sampsize = AVI->track[j].a_fmt==0x1?sampsize*4:sampsize;

			nBlockAlign = (AVI->track[j].a_rate<32000)?576:1152;
			/*
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] XXX sampsize (%d) block (%ld) rate (%ld) audio_bytes (%ld) mp3rate(%ld,%ld)\n",
			 sampsize, nBlockAlign, AVI->track[j].a_rate,
			 (int int)AVI->track[j].audio_bytes,
			 1000*AVI->track[j].mp3rate/8, AVI->track[j].mp3rate));
			 */

			if (AVI->track[j].a_fmt==0x1) {
				sampsize = (AVI->track[j].a_chans<2)?sampsize/2:sampsize;
				avgbsec = AVI->track[j].a_rate*sampsize/4;
				scalerate = AVI->track[j].a_rate*sampsize/4;
			} else  {
				avgbsec = 1000*AVI->track[j].mp3rate/8;
				scalerate = 1000*AVI->track[j].mp3rate/8;
			}

			OUT4CC ("LIST");
			OUTLONG(0);        /* Length of list in bytes, don't know yet */
			strl_start = nhb;  /* Store start position */
			OUT4CC ("strl");

			/* The audio stream header */

			OUT4CC ("strh");
			OUTLONG(56);            /* # of bytes to follow */
			OUT4CC ("auds");

			// -----------
			// ThOe
			OUTLONG(0);             /* Format (Optionally) */
			// -----------

			OUTLONG(0);             /* Flags */
			OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
			OUTLONG(0);             /* InitialFrames */

			// VBR
			if (AVI->track[j].a_fmt == 0x55 && AVI->track[j].a_vbr) {
				OUTLONG(nBlockAlign);                   /* Scale */
				OUTLONG(AVI->track[j].a_rate);          /* Rate */
				OUTLONG(0);                             /* Start */
				OUTLONG(AVI->track[j].audio_chunks);    /* Length */
				OUTLONG(0);                      /* SuggestedBufferSize */
				OUTLONG(0);                             /* Quality */
				OUTLONG(0);                             /* SampleSize */
				OUTLONG(0);                             /* Frame */
				OUTLONG(0);                             /* Frame */
			} else {
				OUTLONG(sampsize/4);                    /* Scale */
				OUTLONG(scalerate);  /* Rate */
				OUTLONG(0);                             /* Start */
				OUTLONG(4*AVI->track[j].audio_bytes/sampsize);   /* Length */
				OUTLONG(0);                             /* SuggestedBufferSize */
				OUTLONG(0xffffffff);                             /* Quality */
				OUTLONG(sampsize/4);                    /* SampleSize */
				OUTLONG(0);                             /* Frame */
				OUTLONG(0);                             /* Frame */
			}

			/* The audio stream format */

			OUT4CC ("strf");

			if (AVI->track[j].a_fmt == 0x55 && AVI->track[j].a_vbr) {

				OUTLONG(30);                            /* # of bytes to follow */ // mplayer writes 28
				OUTSHRT(AVI->track[j].a_fmt);           /* Format */                  // 2
				OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */      // 2
				OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */           // 4
				//ThOe/tibit
				OUTLONG(1000*AVI->track[j].mp3rate/8);  /* maybe we should write an avg. */ // 4
				OUTSHRT(nBlockAlign);                   /* BlockAlign */              // 2
				OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */           // 2

				OUTSHRT(12);                           /* cbSize */                   // 2
				OUTSHRT(1);                            /* wID */                      // 2
				OUTLONG(2);                            /* fdwFlags */                 // 4
				OUTSHRT(nBlockAlign);                  /* nBlockSize */               // 2
				OUTSHRT(1);                            /* nFramesPerBlock */          // 2
				OUTSHRT(0);                            /* nCodecDelay */              // 2

			} else if (AVI->track[j].a_fmt == 0x55 && !AVI->track[j].a_vbr) {

				OUTLONG(30);                            /* # of bytes to follow */
				OUTSHRT(AVI->track[j].a_fmt);           /* Format */
				OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */
				OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */
				//ThOe/tibit
				OUTLONG(1000*AVI->track[j].mp3rate/8);
				OUTSHRT(sampsize/4);                    /* BlockAlign */
				OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */

				OUTSHRT(12);                           /* cbSize */
				OUTSHRT(1);                            /* wID */
				OUTLONG(2);                            /* fdwFlags */
				OUTSHRT(nBlockAlign);                  /* nBlockSize */
				OUTSHRT(1);                            /* nFramesPerBlock */
				OUTSHRT(0);                            /* nCodecDelay */

			} else {

				OUTLONG(18);                   /* # of bytes to follow */
				OUTSHRT(AVI->track[j].a_fmt);           /* Format */
				OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */
				OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */
				//ThOe/tibit
				OUTLONG(avgbsec);  /* Avg bytes/sec */
				OUTSHRT(sampsize/4);                    /* BlockAlign */
				OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */
				OUTSHRT(0);                           /* cbSize */

			}
		}
		if (AVI->is_opendml) {
			u32 k;

			if (!AVI->track[j].audio_superindex) {
				// not initialized -> no index
				continue;
			}

			OUT4CC(AVI->track[j].audio_superindex->fcc);    /* "indx" */
			OUTLONG(2+1+1+4+4+3*4 + AVI->track[j].audio_superindex->nEntriesInUse * (8+4+4));
			OUTSHRT(AVI->track[j].audio_superindex->wLongsPerEntry);
			OUTCHR(AVI->track[j].audio_superindex->bIndexSubType);
			OUTCHR(AVI->track[j].audio_superindex->bIndexType);
			OUTLONG(AVI->track[j].audio_superindex->nEntriesInUse);
			OUT4CC(AVI->track[j].audio_superindex->dwChunkId);
			OUTLONG(0);
			OUTLONG(0);
			OUTLONG(0);

			for (k = 0; k < AVI->track[j].audio_superindex->nEntriesInUse; k++) {
				u32 r = (u32) ((AVI->track[j].audio_superindex->aIndex[k].qwOffset >> 32) & 0xffffffff);
				u32 s = (u32) ((AVI->track[j].audio_superindex->aIndex[k].qwOffset) & 0xffffffff);

				/*
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] AUD[%d] NrEntries %d/%ld (%c%c%c%c) |0x%llX|%ld|%ld| \n",  j, k,
				        AVI->track[j].audio_superindex->nEntriesInUse,
				    AVI->track[j].audio_superindex->dwChunkId[0], AVI->track[j].audio_superindex->dwChunkId[1],
				    AVI->track[j].audio_superindex->dwChunkId[2], AVI->track[j].audio_superindex->dwChunkId[3],
				    AVI->track[j].audio_superindex->aIndex[k].qwOffset,
				    AVI->track[j].audio_superindex->aIndex[k].dwSize,
				    AVI->track[j].audio_superindex->aIndex[k].dwDuration
				  ));
				  */

				OUTLONG(s);
				OUTLONG(r);
				OUTLONG(AVI->track[j].audio_superindex->aIndex[k].dwSize);
				OUTLONG(AVI->track[j].audio_superindex->aIndex[k].dwDuration);
			}
		}
		/* Finish stream list, i.e. put number of bytes in the list to proper pos */
		long2str(AVI_header+strl_start-4,nhb-strl_start);
	}

	if (AVI->is_opendml) {
		OUT4CC("LIST");
		OUTLONG(16);
		OUT4CC("odml");
		OUT4CC("dmlh");
		OUTLONG(4);
		OUTLONG(AVI->total_frames);
	}

	/* Finish header list */

	long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);


	// add INFO list --- (0.6.0pre4)

#ifdef INFO_LIST
	OUT4CC ("LIST");

	info_start_pos = nhb;
	info_len = MAX_INFO_STRLEN + 12;
	OUTLONG(info_len); // rewritten later
	OUT4CC ("INFO");

	OUT4CC ("ISFT");
	//OUTLONG(MAX_INFO_STRLEN);
	memset(id_str, 0, MAX_INFO_STRLEN);
	if (gf_sys_is_test_mode()) {
		snprintf(id_str, MAX_INFO_STRLEN, "GPAC/avilib");
	} else {
		snprintf(id_str, MAX_INFO_STRLEN, "GPAC/avilib-%s", gf_gpac_version());
	}
	real_id_len = id_len = (u32) strlen(id_str)+1;
	if (id_len&1) id_len++;

	OUTLONG(real_id_len);

	memset(AVI_header+nhb, 0, id_len);
	memcpy(AVI_header+nhb, id_str, id_len);
	nhb += id_len;

	info_len = 0;

	// write correct len
	long2str(AVI_header+info_start_pos, info_len + id_len + 4+4+4);

	nhb += info_len;

//   OUT4CC ("ICMT");
//   OUTLONG(MAX_INFO_STRLEN);

//   calptr=time(NULL);
//   sprintf(id_str, "\t%s %s", ctime(&calptr), "");
//   memset(AVI_header+nhb, 0, MAX_INFO_STRLEN);
//   memcpy(AVI_header+nhb, id_str, 25);
//   nhb += MAX_INFO_STRLEN;
#endif

	// ----------------------------

	/* Calculate the needed amount of junk bytes, output junk */

	njunk = HEADERBYTES - nhb - 8 - 12;

	/* Safety first: if njunk <= 0, somebody has played with
	   HEADERBYTES without knowing what (s)he did.
	   This is a fatal error */

	if(njunk<=0)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] AVI_close_output_file: # of header bytes too small\n"));
		exit(1);
	}

	OUT4CC ("JUNK");
	OUTLONG(njunk);
	memset(AVI_header+nhb,0,njunk);

	nhb += njunk;

	/* Start the movi list */

	OUT4CC ("LIST");
	OUTLONG(movi_len); /* Length of list in bytes */
	OUT4CC ("movi");

	/* Output the header, truncate the file to the number of bytes
	   actually written, report an error if someting goes wrong */

	if ( (gf_fseek(AVI->fdes,0,SEEK_SET)==(u64)-1) ||
	        avi_write(AVI->fdes,(char *)AVI_header,HEADERBYTES)!=HEADERBYTES
//		|| ftruncate(AVI->fdes,AVI->pos)<0
	   )
	{
		AVI_errno = AVI_ERR_CLOSE;
		return -1;
	}


	// Fix up the empty additional RIFF and LIST chunks
	if (AVI->is_opendml) {
		u32 k;
		char f[4];
		u32 len;

		for (k=1; k<AVI->video_superindex->nEntriesInUse; k++) {
			// the len of the RIFF Chunk
			gf_fseek(AVI->fdes, AVI->video_superindex->stdindex[k]->qwBaseOffset+4, SEEK_SET);
			len = (u32) (AVI->video_superindex->stdindex[k+1]->qwBaseOffset - AVI->video_superindex->stdindex[k]->qwBaseOffset - 8);
			long2str((unsigned char *)f, len);
			avi_write(AVI->fdes, f, 4);

			// len of the LIST/movi chunk
			gf_fseek(AVI->fdes, 8, SEEK_CUR);
			len -= 12;
			long2str((unsigned char *)f, len);
			avi_write(AVI->fdes, f, 4);
		}
	}


	if(idxerror) return -1;

	return 0;
}

/*
   AVI_write_data:
   Add video or audio data to the file;

   Return values:
    0    No error;
   -1    Error, AVI_errno is set appropriatly;

*/

static int avi_write_data(avi_t *AVI, char *data, unsigned int length, int audio, int keyframe)
{
	int n = 0;

	unsigned char astr[5];

	// transcode core itself checks for the size -- unneeded and
	// does harm to xvid 2pass encodes where the first pass can get
	// _very_ large -- tibit.

#if 0
	/* Check for maximum file length */

	if ( (AVI->pos + 8 + length + 8 + (AVI->n_idx+1)*16) > AVI_MAX_LEN ) {
		AVI_errno = AVI_ERR_SIZELIM;
		return -1;
	}
#endif

	/* Add index entry */

	//set tag for current audio track
	sprintf((char *)astr, "0%1dwb", (int)(AVI->aptr+1));

	if(audio) {
		if (!AVI->is_opendml) n = avi_add_index_entry(AVI,astr,0x10,AVI->pos,length);
		n += avi_add_odml_index_entry(AVI,astr,0x10,AVI->pos,length);
	} else {
		if (!AVI->is_opendml) n = avi_add_index_entry(AVI,(unsigned char *)"00db",((keyframe)?0x10:0x0),AVI->pos,length);
		n += avi_add_odml_index_entry(AVI,(unsigned char *)"00db",((keyframe)?0x10:0x0),AVI->pos,length);
	}

	if(n) return -1;

	/* Output tag and data */

	if(audio)
		n = avi_add_chunk(AVI,(unsigned char *)astr, (unsigned char *)data, length);
	else
		n = avi_add_chunk(AVI,(unsigned char *)"00db", (unsigned char *)data, length);

	if (n) return -1;

	return 0;
}

GF_EXPORT
int AVI_write_frame(avi_t *AVI, u8 *data, int bytes, int keyframe)
{
	s64 pos;

	if(AVI->mode==AVI_MODE_READ) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}

	pos = AVI->pos;

	if(avi_write_data(AVI,data,bytes,0,keyframe)) return -1;

	AVI->last_pos = pos;
	AVI->last_len = bytes;
	AVI->video_frames++;
	return 0;
}

#if 0 //unused
int AVI_dup_frame(avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_READ) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}

	if(AVI->last_pos==0) return 0; /* No previous real frame */
	if(avi_add_index_entry(AVI,(unsigned char *)"00db",0x10,AVI->last_pos,AVI->last_len)) return -1;
	AVI->video_frames++;
	AVI->must_use_index = 1;
	return 0;
}
#endif

GF_EXPORT
int AVI_write_audio(avi_t *AVI, u8 *data, int bytes)
{
	if(AVI->mode==AVI_MODE_READ) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}

	if( avi_write_data(AVI,data,bytes,1,0) ) return -1;
	AVI->track[AVI->aptr].audio_bytes += bytes;
	AVI->track[AVI->aptr].audio_chunks++;
	return 0;
}

#if 0 //unused

int AVI_append_audio(avi_t *AVI, u8 *data, int bytes)
{

	// won't work for >2gb
	int i, length, pos;
	unsigned char c[4];

	if(AVI->mode==AVI_MODE_READ) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}

	// update last index entry:

	--AVI->n_idx;
	length = str2ulong(AVI->idx[AVI->n_idx]+12);
	pos    = str2ulong(AVI->idx[AVI->n_idx]+8);

	//update;
	long2str(AVI->idx[AVI->n_idx]+12,length+bytes);

	++AVI->n_idx;

	AVI->track[AVI->aptr].audio_bytes += bytes;

	//update chunk header
	gf_fseek(AVI->fdes, pos+4, SEEK_SET);
	long2str(c, length+bytes);
	avi_write(AVI->fdes, (char *)c, 4);

	gf_fseek(AVI->fdes, pos+8+length, SEEK_SET);

	i=PAD_EVEN(length + bytes);

	bytes = i - length;
	avi_write(AVI->fdes, data, bytes);
	AVI->pos = pos + 8 + i;

	return 0;
}

u64 AVI_bytes_remain(avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_READ) return 0;

	return ( AVI_MAX_LEN - (AVI->pos + 8 + 16*AVI->n_idx));
}

u64 AVI_bytes_written(avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_READ) return 0;

	return (AVI->pos + 8 + 16*AVI->n_idx);
}
#endif

int AVI_set_audio_track(avi_t *AVI, u32 track)
{

	if (track + 1 > AVI->anum) return(-1);

	//this info is not written to file anyway
	AVI->aptr=track;
	return 0;
}

int AVI_get_audio_track(avi_t *AVI)
{
	return(AVI->aptr);
}

#if 0 //unused
void AVI_set_audio_vbr(avi_t *AVI, int is_vbr)
{
	AVI->track[AVI->aptr].a_vbr = is_vbr;
}

int AVI_get_audio_vbr(avi_t *AVI)
{
	return(AVI->track[AVI->aptr].a_vbr);
}
#endif


/*******************************************************************
 *                                                                 *
 *    Utilities for reading video and audio from an AVI File       *
 *                                                                 *
 *******************************************************************/

GF_EXPORT
int AVI_close(avi_t *AVI)
{
	int ret;
	u32 j;

	/* If the file was open for writing, the header and index still have
	   to be written */

	if(AVI->mode == AVI_MODE_WRITE)
		ret = avi_close_output_file(AVI);
	else
		ret = 0;

	/* Even if there happened an error, we first clean up */

	gf_fclose(AVI->fdes);
	if(AVI->idx) gf_free(AVI->idx);
	if(AVI->video_index) gf_free(AVI->video_index);
	if(AVI->video_superindex) {
		if(AVI->video_superindex->aIndex) gf_free(AVI->video_superindex->aIndex);
		if (AVI->video_superindex->stdindex) {
			for (j=0; j < NR_IXNN_CHUNKS; j++) {
				if (AVI->video_superindex->stdindex[j]->aIndex)
					gf_free(AVI->video_superindex->stdindex[j]->aIndex);
				gf_free(AVI->video_superindex->stdindex[j]);
			}
			gf_free(AVI->video_superindex->stdindex);
		}
		gf_free(AVI->video_superindex);
	}

	for (j=0; j<AVI->anum; j++)
	{
		if(AVI->track[j].audio_index) gf_free(AVI->track[j].audio_index);
		if(AVI->track[j].audio_superindex) {
			avisuperindex_chunk *asi = AVI->track[j].audio_superindex;
			if (asi->aIndex) gf_free(asi->aIndex);

			if (asi->stdindex) {
				for (j=0; j < NR_IXNN_CHUNKS; j++) {
					if (asi->stdindex[j]->aIndex)
						gf_free(asi->stdindex[j]->aIndex);
					gf_free(asi->stdindex[j]);
				}
				gf_free(asi->stdindex);
			}
			gf_free(asi);
		}
	}

	if (AVI->bitmap_info_header)
		gf_free(AVI->bitmap_info_header);
	for (j = 0; j < AVI->anum; j++)
		if (AVI->wave_format_ex[j])
			gf_free(AVI->wave_format_ex[j]);

	gf_free(AVI);
	AVI=NULL;

	return ret;
}


#define ERR_EXIT(x) \
{ \
   AVI_close(AVI); \
   AVI_errno = x; \
   return 0; \
}


avi_t *AVI_open_input_file(char *filename, int getIndex)
{
	avi_t *AVI=NULL;

	/* Create avi_t structure */

	AVI = (avi_t *) gf_malloc(sizeof(avi_t));
	if(AVI==NULL)
	{
		AVI_errno = AVI_ERR_NO_MEM;
		return 0;
	}
	memset((void *)AVI,0,sizeof(avi_t));

	AVI->mode = AVI_MODE_READ; /* open for reading */

	/* Open the file */

	AVI->fdes = gf_fopen(filename,"rb");
	if(!AVI->fdes )
	{
		AVI_errno = AVI_ERR_OPEN;
		gf_free(AVI);
		return 0;
	}

	AVI_errno = 0;
	avi_parse_input_file(AVI, getIndex);

	if (AVI != NULL && !AVI_errno) {
		AVI->aptr=0; //reset
	}

	if (AVI_errno) return NULL;

	return AVI;
}

#if 0
avi_t *AVI_open_fd(FILE *fd, int getIndex)
{
	avi_t *AVI=NULL;

	/* Create avi_t structure */

	AVI = (avi_t *) gf_malloc(sizeof(avi_t));
	if(AVI==NULL)
	{
		AVI_errno = AVI_ERR_NO_MEM;
		return 0;
	}
	memset((void *)AVI,0,sizeof(avi_t));

	AVI->mode = AVI_MODE_READ; /* open for reading */

	// file alread open
	AVI->fdes = fd;

	AVI_errno = 0;
	avi_parse_input_file(AVI, getIndex);

	if (AVI != NULL && !AVI_errno) {
		AVI->aptr=0; //reset
	}

	if (AVI_errno)
		return AVI=NULL;
	else
		return AVI;
}
#endif

int avi_parse_input_file(avi_t *AVI, int getIndex)
{
	int i, rate, scale, idx_type;
	s64 n;
	unsigned char *hdrl_data;
	u64 header_offset=0;
	int hdrl_len=0;
	int nvi, nai[AVI_MAX_TRACKS], ioff;
	u64 tot[AVI_MAX_TRACKS];
	u32 j;
	int lasttag = 0;
	int vids_strh_seen = 0;
	int vids_strf_seen = 0;
	int auds_strh_seen = 0;
	//  int auds_strf_seen = 0;
	int num_stream = 0;
	char data[256];
	s64 oldpos=-1, newpos=-1;

	int aud_chunks = 0;
	/* Read first 12 bytes and check that this is an AVI file */

	if( avi_read(AVI->fdes,data,12) != 12 ) ERR_EXIT(AVI_ERR_READ)

		if( strnicmp(data  ,"RIFF",4) !=0 ||
		        strnicmp(data+8,"AVI ",4) !=0 ) ERR_EXIT(AVI_ERR_NO_AVI)

			/* Go through the AVI file and extract the header list,
			   the start position of the 'movi' list and an optionally
			   present idx1 tag */

			hdrl_data = 0;


	while(1)
	{
		if( avi_read(AVI->fdes,data,8) != 8 ) break; /* We assume it's EOF */
		newpos = gf_ftell(AVI->fdes);
		if(oldpos==newpos) {
			/* This is a broken AVI stream... */
			return -1;
		}
		oldpos=newpos;

		n = str2ulong((unsigned char *)data+4);
		n = PAD_EVEN(n);

		if(strnicmp(data,"LIST",4) == 0)
		{
			if( avi_read(AVI->fdes,data,4) != 4 ) ERR_EXIT(AVI_ERR_READ)
				n -= 4;
			if(strnicmp(data,"hdrl",4) == 0)
			{
				hdrl_len = (u32) n;
				hdrl_data = (unsigned char *) gf_malloc((u32)n);
				if(hdrl_data==0) ERR_EXIT(AVI_ERR_NO_MEM);

				// offset of header

				header_offset = gf_ftell(AVI->fdes);

				if( avi_read(AVI->fdes,(char *)hdrl_data, (u32) n) != n ) ERR_EXIT(AVI_ERR_READ)
				}
			else if(strnicmp(data,"movi",4) == 0)
			{
				AVI->movi_start = gf_ftell(AVI->fdes);
				if (gf_fseek(AVI->fdes,n,SEEK_CUR)==(u64)-1) break;
			}
			else if (gf_fseek(AVI->fdes,n,SEEK_CUR)==(u64)-1) break;
		}
		else if(strnicmp(data,"idx1",4) == 0)
		{
			/* n must be a multiple of 16, but the reading does not
			   break if this is not the case */

			AVI->n_idx = AVI->max_idx = (u32) (n/16);
			AVI->idx = (unsigned  char((*)[16]) ) gf_malloc((u32)n);
			if(AVI->idx==0) ERR_EXIT(AVI_ERR_NO_MEM)
				if(avi_read(AVI->fdes, (char *) AVI->idx, (u32) n) != n ) {
					gf_free( AVI->idx);
					AVI->idx=NULL;
					AVI->n_idx = 0;
				}
		}
		else
			gf_fseek(AVI->fdes,n,SEEK_CUR);
	}

	if(!hdrl_data      ) ERR_EXIT(AVI_ERR_NO_HDRL)
		if(!AVI->movi_start) ERR_EXIT(AVI_ERR_NO_MOVI)

			/* Interpret the header list */

			for(i=0; i<hdrl_len;)
			{
				/* List tags are completly ignored */

#ifdef DEBUG_ODML
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] TAG %c%c%c%c\n", (hdrl_data+i)[0], (hdrl_data+i)[1], (hdrl_data+i)[2], (hdrl_data+i)[3]));
#endif

				if(strnicmp((char *)hdrl_data+i,"LIST",4)==0) {
					i+= 12;
					continue;
				}

				n = str2ulong(hdrl_data+i+4);
				n = PAD_EVEN(n);


				/* Interpret the tag and its args */

				if(strnicmp((char *)hdrl_data+i,"strh",4)==0)
				{
					i += 8;
#ifdef DEBUG_ODML
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] TAG   %c%c%c%c\n", (hdrl_data+i)[0], (hdrl_data+i)[1], (hdrl_data+i)[2], (hdrl_data+i)[3]));
#endif
					if(strnicmp((char *)hdrl_data+i,"vids",4) == 0 && !vids_strh_seen)
					{
						memcpy(AVI->compressor,hdrl_data+i+4,4);
						AVI->compressor[4] = 0;

						// ThOe
						AVI->v_codech_off = header_offset + i+4;

						scale = str2ulong(hdrl_data+i+20);
						rate  = str2ulong(hdrl_data+i+24);
						if(scale!=0) AVI->fps = (double)rate/(double)scale;
						AVI->video_frames = str2ulong(hdrl_data+i+32);
						AVI->video_strn = num_stream;
						AVI->max_len = 0;
						vids_strh_seen = 1;
						lasttag = 1; /* vids */
						memcpy(&AVI->video_stream_header, hdrl_data + i,
						       sizeof(alAVISTREAMHEADER));
					}
					else if (strnicmp ((char *)hdrl_data+i,"auds",4) ==0 && ! auds_strh_seen)
					{

						//inc audio tracks
						AVI->aptr=AVI->anum;
						++AVI->anum;

						if(AVI->anum > AVI_MAX_TRACKS) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] error - only %d audio tracks supported\n", AVI_MAX_TRACKS));
							return(-1);
						}

						AVI->track[AVI->aptr].audio_bytes = str2ulong(hdrl_data+i+32)*avi_sampsize(AVI, 0);
						AVI->track[AVI->aptr].audio_strn = num_stream;

						// if samplesize==0 -> vbr
						AVI->track[AVI->aptr].a_vbr = !str2ulong(hdrl_data+i+44);

						AVI->track[AVI->aptr].padrate = str2ulong(hdrl_data+i+24);
						memcpy(&AVI->stream_headers[AVI->aptr], hdrl_data + i,
						       sizeof(alAVISTREAMHEADER));

						//	   auds_strh_seen = 1;
						lasttag = 2; /* auds */

						// ThOe
						AVI->track[AVI->aptr].a_codech_off = header_offset + i;

					}
					else if (strnicmp ((char*)hdrl_data+i,"iavs",4) ==0 && ! auds_strh_seen) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] AVILIB: error - DV AVI Type 1 no supported\n"));
						return (-1);
					}
					else
						lasttag = 0;
					num_stream++;
				}
				else if(strnicmp((char*)hdrl_data+i,"dmlh",4) == 0) {
					AVI->total_frames = str2ulong(hdrl_data+i+8);
#ifdef DEBUG_ODML
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] real number of frames %d\n", AVI->total_frames));
#endif
					i += 8;
				}
				else if(strnicmp((char *)hdrl_data+i,"strf",4)==0)
				{
					i += 8;
					if(lasttag == 1)
					{
						alBITMAPINFOHEADER bih;

						memcpy(&bih, hdrl_data + i, sizeof(alBITMAPINFOHEADER));
						AVI->bitmap_info_header = (alBITMAPINFOHEADER *)
						                          gf_malloc(str2ulong((unsigned char *)&bih.bi_size));
						if (AVI->bitmap_info_header != NULL)
							memcpy(AVI->bitmap_info_header, hdrl_data + i,
							       str2ulong((unsigned char *)&bih.bi_size));

						AVI->width  = str2ulong(hdrl_data+i+4);
						AVI->height = str2ulong(hdrl_data+i+8);
						vids_strf_seen = 1;
						//ThOe
						AVI->v_codecf_off = header_offset + i+16;

						memcpy(AVI->compressor2, hdrl_data+i+16, 4);
						AVI->compressor2[4] = 0;

					}
					else if(lasttag == 2)
					{
						alWAVEFORMATEX *wfe;
						char *nwfe;
						int wfes;

						if ((u32) (hdrl_len - i) < sizeof(alWAVEFORMATEX))
							wfes = hdrl_len - i;
						else
							wfes = sizeof(alWAVEFORMATEX);
						wfe = (alWAVEFORMATEX *)gf_malloc(sizeof(alWAVEFORMATEX));
						if (wfe != NULL) {
							memset(wfe, 0, sizeof(alWAVEFORMATEX));
							memcpy(wfe, hdrl_data + i, wfes);
							if (str2ushort((unsigned char *)&wfe->cb_size) != 0) {
								nwfe = (char *)
								       gf_realloc(wfe, sizeof(alWAVEFORMATEX) +
								                  str2ushort((unsigned char *)&wfe->cb_size));
								if (nwfe != 0) {
									s64 lpos = gf_ftell(AVI->fdes);
									gf_fseek(AVI->fdes, header_offset + i + sizeof(alWAVEFORMATEX),
									         SEEK_SET);
									wfe = (alWAVEFORMATEX *)nwfe;
									nwfe = &nwfe[sizeof(alWAVEFORMATEX)];
									avi_read(AVI->fdes, nwfe,
									         str2ushort((unsigned char *)&wfe->cb_size));
									gf_fseek(AVI->fdes, lpos, SEEK_SET);
								}
							}
							AVI->wave_format_ex[AVI->aptr] = wfe;
						}

						AVI->track[AVI->aptr].a_fmt   = str2ushort(hdrl_data+i  );

						//ThOe
						AVI->track[AVI->aptr].a_codecf_off = header_offset + i;

						AVI->track[AVI->aptr].a_chans = str2ushort(hdrl_data+i+2);
						AVI->track[AVI->aptr].a_rate  = str2ulong (hdrl_data+i+4);
						//ThOe: read mp3bitrate
						AVI->track[AVI->aptr].mp3rate = 8*str2ulong(hdrl_data+i+8)/1000;
						//:ThOe
						AVI->track[AVI->aptr].a_bits  = str2ushort(hdrl_data+i+14);
						//            auds_strf_seen = 1;
					}
				}
				else if(strnicmp((char*)hdrl_data+i,"indx",4) == 0) {
					char *a;

					if(lasttag == 1) // V I D E O
					{

						a = (char*)hdrl_data+i;

						AVI->video_superindex = (avisuperindex_chunk *) gf_malloc (sizeof (avisuperindex_chunk));
						memset(AVI->video_superindex, 0, sizeof (avisuperindex_chunk));
						memcpy (AVI->video_superindex->fcc, a, 4);
						a += 4;
						AVI->video_superindex->dwSize = str2ulong((unsigned char *)a);
						a += 4;
						AVI->video_superindex->wLongsPerEntry = str2ushort((unsigned char *)a);
						a += 2;
						AVI->video_superindex->bIndexSubType = *a;
						a += 1;
						AVI->video_superindex->bIndexType = *a;
						a += 1;
						AVI->video_superindex->nEntriesInUse = str2ulong((unsigned char *)a);
						a += 4;
						memcpy (AVI->video_superindex->dwChunkId, a, 4);
						a += 4;

						// 3 * reserved
						a += 4;
						a += 4;
						a += 4;

						if (AVI->video_superindex->bIndexSubType != 0) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] Invalid Header, bIndexSubType != 0\n"));
						}

						AVI->video_superindex->aIndex = (avisuperindex_entry*)
						                                gf_malloc (AVI->video_superindex->wLongsPerEntry * AVI->video_superindex->nEntriesInUse * sizeof (u32));

						// position of ix## chunks
						for (j=0; j<AVI->video_superindex->nEntriesInUse; ++j) {
							AVI->video_superindex->aIndex[j].qwOffset = str2ullong ((unsigned char*)a);
							a += 8;
							AVI->video_superindex->aIndex[j].dwSize = str2ulong ((unsigned char*)a);
							a += 4;
							AVI->video_superindex->aIndex[j].dwDuration = str2ulong ((unsigned char*)a);
							a += 4;

#ifdef DEBUG_ODML
							GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d] 0x%llx 0x%lx %lu\n", j,
							                                        (unsigned int long)AVI->video_superindex->aIndex[j].qwOffset,
							                                        (unsigned long)AVI->video_superindex->aIndex[j].dwSize,
							                                        (unsigned long)AVI->video_superindex->aIndex[j].dwDuration));
#endif
						}


#ifdef DEBUG_ODML
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] FOURCC \"%c%c%c%c\"\n", AVI->video_superindex->fcc[0], AVI->video_superindex->fcc[1],
						                                        AVI->video_superindex->fcc[2], AVI->video_superindex->fcc[3]));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] LEN \"%ld\"\n", (long)AVI->video_superindex->dwSize));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] wLongsPerEntry \"%d\"\n", AVI->video_superindex->wLongsPerEntry));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexSubType \"%d\"\n", AVI->video_superindex->bIndexSubType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexType \"%d\"\n", AVI->video_superindex->bIndexType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] nEntriesInUse \"%ld\"\n", (long)AVI->video_superindex->nEntriesInUse));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] dwChunkId \"%c%c%c%c\"\n", AVI->video_superindex->dwChunkId[0], AVI->video_superindex->dwChunkId[1],
						                                        AVI->video_superindex->dwChunkId[2], AVI->video_superindex->dwChunkId[3]));
#endif

						AVI->is_opendml = 1;

					}
					else if(lasttag == 2) // A U D I O
					{

						a = (char*) hdrl_data+i;

						AVI->track[AVI->aptr].audio_superindex = (avisuperindex_chunk *) gf_malloc (sizeof (avisuperindex_chunk));
						memcpy (AVI->track[AVI->aptr].audio_superindex->fcc, a, 4);
						a += 4;
						AVI->track[AVI->aptr].audio_superindex->dwSize = str2ulong((unsigned char*)a);
						a += 4;
						AVI->track[AVI->aptr].audio_superindex->wLongsPerEntry = str2ushort((unsigned char*)a);
						a += 2;
						AVI->track[AVI->aptr].audio_superindex->bIndexSubType = *a;
						a += 1;
						AVI->track[AVI->aptr].audio_superindex->bIndexType = *a;
						a += 1;
						AVI->track[AVI->aptr].audio_superindex->nEntriesInUse = str2ulong((unsigned char*)a);
						a += 4;
						memcpy (AVI->track[AVI->aptr].audio_superindex->dwChunkId, a, 4);
						a += 4;

						// 3 * reserved
						a += 4;
						a += 4;
						a += 4;

						if (AVI->track[AVI->aptr].audio_superindex->bIndexSubType != 0) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] Invalid Header, bIndexSubType != 0\n"));
						}

						AVI->track[AVI->aptr].audio_superindex->aIndex = (avisuperindex_entry*)
						        gf_malloc (AVI->track[AVI->aptr].audio_superindex->wLongsPerEntry *
						                   AVI->track[AVI->aptr].audio_superindex->nEntriesInUse * sizeof (u32));

						// position of ix## chunks
						for (j=0; j<AVI->track[AVI->aptr].audio_superindex->nEntriesInUse; ++j) {
							AVI->track[AVI->aptr].audio_superindex->aIndex[j].qwOffset = str2ullong ((unsigned char*)a);
							a += 8;
							AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwSize = str2ulong ((unsigned char*)a);
							a += 4;
							AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwDuration = str2ulong ((unsigned char*)a);
							a += 4;

#ifdef DEBUG_ODML
							GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d] 0x%llx 0x%lx %lu\n", j,
							                                        (unsigned int long)AVI->track[AVI->aptr].audio_superindex->aIndex[j].qwOffset,
							                                        (unsigned long)AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwSize,
							                                        (unsigned long)AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwDuration));
#endif
						}

						AVI->track[AVI->aptr].audio_superindex->stdindex = NULL;

#ifdef DEBUG_ODML
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] FOURCC \"%.4s\"\n", AVI->track[AVI->aptr].audio_superindex->fcc));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] LEN \"%ld\"\n", (long)AVI->track[AVI->aptr].audio_superindex->dwSize));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] wLongsPerEntry \"%d\"\n", AVI->track[AVI->aptr].audio_superindex->wLongsPerEntry));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexSubType \"%d\"\n", AVI->track[AVI->aptr].audio_superindex->bIndexSubType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexType \"%d\"\n", AVI->track[AVI->aptr].audio_superindex->bIndexType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] nEntriesInUse \"%ld\"\n", (long)AVI->track[AVI->aptr].audio_superindex->nEntriesInUse));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] dwChunkId \"%.4s\"\n", AVI->track[AVI->aptr].audio_superindex->dwChunkId[0]));
#endif

					}
					i += 8;
				}
				else if((strnicmp((char*)hdrl_data+i,"JUNK",4) == 0) ||
				        (strnicmp((char*)hdrl_data+i,"strn",4) == 0) ||
				        (strnicmp((char*)hdrl_data+i,"vprp",4) == 0)) {
					i += 8;
					// do not reset lasttag
				} else
				{
					i += 8;
					lasttag = 0;
				}
				//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] adding %ld bytes\n", (int int)n));

				i += (u32) n;
			}

	gf_free(hdrl_data);

	if(!vids_strh_seen || !vids_strf_seen) ERR_EXIT(AVI_ERR_NO_VIDS)

		AVI->video_tag[0] = AVI->video_strn/10 + '0';
	AVI->video_tag[1] = AVI->video_strn%10 + '0';
	AVI->video_tag[2] = 'd';
	AVI->video_tag[3] = 'b';

	/* Audio tag is set to "99wb" if no audio present */
	if(!AVI->track[0].a_chans) AVI->track[0].audio_strn = 99;

	{
		int i=0;
		for(j=0; j<AVI->anum+1; ++j) {
			if (j == AVI->video_strn) continue;
			AVI->track[i].audio_tag[0] = j/10 + '0';
			AVI->track[i].audio_tag[1] = j%10 + '0';
			AVI->track[i].audio_tag[2] = 'w';
			AVI->track[i].audio_tag[3] = 'b';
			++i;
		}
	}

	gf_fseek(AVI->fdes,AVI->movi_start,SEEK_SET);

	if(!getIndex) return(0);

	/* if the file has an idx1, check if this is relative
	   to the start of the file or to the start of the movi list */

	idx_type = 0;

	if(AVI->idx)
	{
		s64 pos, len;

		/* Search the first videoframe in the idx1 and look where
		   it is in the file */

		for(i=0; i<AVI->n_idx; i++)
			if( strnicmp((char *)AVI->idx[i],(char *)AVI->video_tag,3)==0 ) break;
		if(i>=AVI->n_idx) ERR_EXIT(AVI_ERR_NO_VIDS)

			pos = str2ulong(AVI->idx[i]+ 8);
		len = str2ulong(AVI->idx[i]+12);

		gf_fseek(AVI->fdes,pos,SEEK_SET);
		if(avi_read(AVI->fdes,data,8)!=8) ERR_EXIT(AVI_ERR_READ)
			if( strnicmp(data,(char *)AVI->idx[i],4)==0 && str2ulong((unsigned char *)data+4)==len )
			{
				idx_type = 1; /* Index from start of file */
			}
			else
			{
				gf_fseek(AVI->fdes,pos+AVI->movi_start-4,SEEK_SET);
				if(avi_read(AVI->fdes,data,8)!=8) ERR_EXIT(AVI_ERR_READ)
					if( strnicmp(data,(char *)AVI->idx[i],4)==0 && str2ulong((unsigned char *)data+4)==len )
					{
						idx_type = 2; /* Index from start of movi list */
					}
			}
		/* idx_type remains 0 if neither of the two tests above succeeds */
	}


	if(idx_type == 0 && !AVI->is_opendml && !AVI->total_frames)
	{
		/* we must search through the file to get the index */

		gf_fseek(AVI->fdes, AVI->movi_start, SEEK_SET);

		AVI->n_idx = 0;

		while(1)
		{
			if( avi_read(AVI->fdes,data,8) != 8 ) break;
			n = str2ulong((unsigned char *)data+4);

			/* The movi list may contain sub-lists, ignore them */

			if(strnicmp(data,"LIST",4)==0)
			{
				gf_fseek(AVI->fdes,4,SEEK_CUR);
				continue;
			}

			/* Check if we got a tag ##db, ##dc or ##wb */

			if( ( (data[2]=='d' || data[2]=='D') &&
			        (data[3]=='b' || data[3]=='B' || data[3]=='c' || data[3]=='C') )
			        || ( (data[2]=='w' || data[2]=='W') &&
			             (data[3]=='b' || data[3]=='B') ) )
			{
				u64 __pos = gf_ftell(AVI->fdes) - 8;
				avi_add_index_entry(AVI,(unsigned char *)data,0,__pos,n);
			}

			gf_fseek(AVI->fdes,PAD_EVEN(n),SEEK_CUR);
		}
		idx_type = 1;
	}

	// ************************
	// OPENDML
	// ************************

	// read extended index chunks
	if (AVI->is_opendml) {
		u64 offset = 0;
		int hdrl_len = 4+4+2+1+1+4+4+8+4;
		char *en, *chunk_start;
		int k = 0;
		u32 audtr = 0;
		u32 nrEntries = 0;

		AVI->video_index = NULL;

		nvi = 0;
		for(audtr=0; audtr<AVI->anum; ++audtr) {
			nai[audtr] = 0;
			tot[audtr] = 0;
		}

		// ************************
		// VIDEO
		// ************************

		for (j=0; j<AVI->video_superindex->nEntriesInUse; j++) {

			// read from file
			chunk_start = en = (char*) gf_malloc ((u32) (AVI->video_superindex->aIndex[j].dwSize+hdrl_len) );

			if (gf_fseek(AVI->fdes, AVI->video_superindex->aIndex[j].qwOffset, SEEK_SET) == (u64)-1) {
				gf_free(chunk_start);
				continue;
			}

			if (avi_read(AVI->fdes, en, (u32) (AVI->video_superindex->aIndex[j].dwSize+hdrl_len) ) <= 0) {
				gf_free(chunk_start);
				continue;
			}

			nrEntries = str2ulong((unsigned char*)en + 12);
#ifdef DEBUG_ODML
			//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d:0] Video nrEntries %ld\n", j, nrEntries));
#endif
			offset = str2ullong((unsigned char*)en + 20);

			// skip header
			en += hdrl_len;
			nvi += nrEntries;
			AVI->video_index = (video_index_entry *) gf_realloc (AVI->video_index, nvi * sizeof (video_index_entry));
			if (!AVI->video_index) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] out of mem (size = %ld)\n", nvi * sizeof (video_index_entry)));
				exit(1);
			}

			while (k < nvi) {

				AVI->video_index[k].pos = offset + str2ulong((unsigned char*)en);
				en += 4;
				AVI->video_index[k].len = str2ulong_len((unsigned char*)en);
				AVI->video_index[k].key = str2ulong_key((unsigned char*)en);
				en += 4;

				// completely empty chunk
				if (AVI->video_index[k].pos-offset == 0 && AVI->video_index[k].len == 0) {
					k--;
					nvi--;
				}

#ifdef DEBUG_ODML
				/*
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d] POS 0x%llX len=%d key=%s offset (%llx) (%ld)\n", k,
				  AVI->video_index[k].pos,
				  (int)AVI->video_index[k].len,
				  AVI->video_index[k].key?"yes":"no ", offset,
				  AVI->video_superindex->aIndex[j].dwSize));
				  */
#endif

				k++;
			}

			gf_free(chunk_start);
		}

		AVI->video_frames = nvi;
		// this should deal with broken 'rec ' odml files.
		if (AVI->video_frames == 0) {
			AVI->is_opendml=0;
			goto multiple_riff;
		}

		// ************************
		// AUDIO
		// ************************

		for(audtr=0; audtr<AVI->anum; ++audtr) {

			k = 0;
			if (!AVI->track[audtr].audio_superindex) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] (%s) cannot read audio index for track %d\n", __FILE__, audtr));
				continue;
			}
			for (j=0; j<AVI->track[audtr].audio_superindex->nEntriesInUse; j++) {

				// read from file
				chunk_start = en = (char*)gf_malloc ((u32) (AVI->track[audtr].audio_superindex->aIndex[j].dwSize+hdrl_len));

				if (gf_fseek(AVI->fdes, AVI->track[audtr].audio_superindex->aIndex[j].qwOffset, SEEK_SET) == (u64)-1) {
					gf_free(chunk_start);
					continue;
				}

				if (avi_read(AVI->fdes, en, (u32) (AVI->track[audtr].audio_superindex->aIndex[j].dwSize+hdrl_len)) <= 0) {
					gf_free(chunk_start);
					continue;
				}

				nrEntries = str2ulong((unsigned char*)en + 12);
				//if (nrEntries > 50) nrEntries = 2; // XXX
#ifdef DEBUG_ODML
				//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d:%d] Audio nrEntries %ld\n", j, audtr, nrEntries));
#endif
				offset = str2ullong((unsigned char*)en + 20);

				// skip header
				en += hdrl_len;
				nai[audtr] += nrEntries;
				AVI->track[audtr].audio_index = (audio_index_entry *) gf_realloc (AVI->track[audtr].audio_index, nai[audtr] * sizeof (audio_index_entry));

				while (k < nai[audtr]) {

					AVI->track[audtr].audio_index[k].pos = offset + str2ulong((unsigned char*)en);
					en += 4;
					AVI->track[audtr].audio_index[k].len = str2ulong_len((unsigned char*)en);
					en += 4;
					AVI->track[audtr].audio_index[k].tot = tot[audtr];
					tot[audtr] += AVI->track[audtr].audio_index[k].len;

#ifdef DEBUG_ODML
					/*
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d:%d] POS 0x%llX len=%d offset (%llx) (%ld)\n", k, audtr,
					  AVI->track[audtr].audio_index[k].pos,
					  (int)AVI->track[audtr].audio_index[k].len,
					  offset, AVI->track[audtr].audio_superindex->aIndex[j].dwSize));
					  */
#endif

					++k;
				}

				gf_free(chunk_start);
			}

			AVI->track[audtr].audio_chunks = nai[audtr];
			AVI->track[audtr].audio_bytes = tot[audtr];
		}
	} // is opendml

	else if (AVI->total_frames && !AVI->is_opendml && idx_type==0) {

		// *********************
		// MULTIPLE RIFF CHUNKS (and no index)
		// *********************

multiple_riff:

		gf_fseek(AVI->fdes, AVI->movi_start, SEEK_SET);

		AVI->n_idx = 0;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] Reconstructing index..."));

		// Number of frames; only one audio track supported
		nvi = AVI->video_frames = AVI->total_frames;
		nai[0] = AVI->track[0].audio_chunks = AVI->total_frames;
		for(j=1; j<AVI->anum; ++j) AVI->track[j].audio_chunks = 0;

		AVI->video_index = (video_index_entry *) gf_malloc(nvi*sizeof(video_index_entry));

		if(AVI->video_index==0) ERR_EXIT(AVI_ERR_NO_MEM);

		for(j=0; j<AVI->anum; ++j) {
			if(AVI->track[j].audio_chunks) {
				AVI->track[j].audio_index = (audio_index_entry *) gf_malloc((nai[j]+1)*sizeof(audio_index_entry));
				memset(AVI->track[j].audio_index, 0, (nai[j]+1)*(sizeof(audio_index_entry)));
				if(AVI->track[j].audio_index==0) ERR_EXIT(AVI_ERR_NO_MEM);
			}
		}

		nvi = 0;
		for(j=0; j<AVI->anum; ++j) {
			nai[j] = 0;
			tot[j] = 0;
		}

		aud_chunks = AVI->total_frames;

		while(1)
		{
			if (nvi >= AVI->total_frames) break;

			if( avi_read(AVI->fdes,data,8) != 8 ) break;
			n = str2ulong((unsigned char *)data+4);


			j=0;

			if (aud_chunks - nai[j] -1 <= 0) {
				aud_chunks += AVI->total_frames;
				AVI->track[j].audio_index = (audio_index_entry *)
				                            gf_realloc( AVI->track[j].audio_index, (aud_chunks+1)*sizeof(audio_index_entry));
				if (!AVI->track[j].audio_index) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] Internal error in avilib -- no mem\n"));
					AVI_errno = AVI_ERR_NO_MEM;
					return -1;
				}
			}

			/* Check if we got a tag ##db, ##dc or ##wb */

			// VIDEO
			if(
			    (data[0]=='0' || data[1]=='0') &&
			    (data[2]=='d' || data[2]=='D') &&
			    (data[3]=='b' || data[3]=='B' || data[3]=='c' || data[3]=='C') ) {

				AVI->video_index[nvi].key = 0x0;
				AVI->video_index[nvi].pos = gf_ftell(AVI->fdes);
				AVI->video_index[nvi].len = (u32) n;

				/*
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] Frame %ld pos %"LLD" len %"LLD" key %ld\n",
				    nvi, AVI->video_index[nvi].pos,  AVI->video_index[nvi].len, (long)AVI->video_index[nvi].key));
				    */
				nvi++;
				gf_fseek(AVI->fdes,PAD_EVEN(n),SEEK_CUR);
			}

			//AUDIO
			else if(
			    (data[0]=='0' || data[1]=='1') &&
			    (data[2]=='w' || data[2]=='W') &&
			    (data[3]=='b' || data[3]=='B') ) {


				AVI->track[j].audio_index[nai[j]].pos = gf_ftell(AVI->fdes);
				AVI->track[j].audio_index[nai[j]].len = (u32) n;
				AVI->track[j].audio_index[nai[j]].tot = tot[j];
				tot[j] += AVI->track[j].audio_index[nai[j]].len;
				nai[j]++;

				gf_fseek(AVI->fdes,PAD_EVEN(n),SEEK_CUR);
			}
			else {
				gf_fseek(AVI->fdes,-4,SEEK_CUR);
			}

		}
		if (nvi < AVI->total_frames) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[avilib] Uh? Some frames seems missing (%ld/%d)\n",
			        nvi,  AVI->total_frames));
		}


		AVI->video_frames = nvi;
		AVI->track[0].audio_chunks = nai[0];

		for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_bytes = tot[j];
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] done. nvi=%ld nai=%ld tot=%ld\n", nvi, nai[0], tot[0]));

	} // total_frames but no indx chunk (xawtv does this)

	else

	{
		// ******************
		// NO OPENDML
		// ******************

		/* Now generate the video index and audio index arrays */

		nvi = 0;
		for(j=0; j<AVI->anum; ++j) nai[j] = 0;

		for(i=0; i<AVI->n_idx; i++) {

			if(strnicmp((char *)AVI->idx[i],AVI->video_tag,3) == 0) nvi++;

			for(j=0; j<AVI->anum; ++j) if(strnicmp((char *)AVI->idx[i], AVI->track[j].audio_tag,4) == 0) nai[j]++;
		}

		AVI->video_frames = nvi;
		for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_chunks = nai[j];


		if(AVI->video_frames==0) ERR_EXIT(AVI_ERR_NO_VIDS);
		AVI->video_index = (video_index_entry *) gf_malloc(nvi*sizeof(video_index_entry));
		if(AVI->video_index==0) ERR_EXIT(AVI_ERR_NO_MEM);

		for(j=0; j<AVI->anum; ++j) {
			if(AVI->track[j].audio_chunks) {
				AVI->track[j].audio_index = (audio_index_entry *) gf_malloc((nai[j]+1)*sizeof(audio_index_entry));
				memset(AVI->track[j].audio_index, 0, (nai[j]+1)*(sizeof(audio_index_entry)));
				if(AVI->track[j].audio_index==0) ERR_EXIT(AVI_ERR_NO_MEM);
			}
		}

		nvi = 0;
		for(j=0; j<AVI->anum; ++j) {
			nai[j] = 0;
			tot[j] = 0;
		}

		ioff = idx_type == 1 ? 8 : (u32)AVI->movi_start+4;

		for(i=0; i<AVI->n_idx; i++) {

			//video
			if(strnicmp((char *)AVI->idx[i],AVI->video_tag,3) == 0) {
				AVI->video_index[nvi].key = str2ulong(AVI->idx[i]+ 4);
				AVI->video_index[nvi].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
				AVI->video_index[nvi].len = str2ulong(AVI->idx[i]+12);
				nvi++;
			}

			//audio
			for(j=0; j<AVI->anum; ++j) {

				if(strnicmp((char *)AVI->idx[i],AVI->track[j].audio_tag,4) == 0) {
					AVI->track[j].audio_index[nai[j]].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
					AVI->track[j].audio_index[nai[j]].len = str2ulong(AVI->idx[i]+12);
					AVI->track[j].audio_index[nai[j]].tot = tot[j];
					tot[j] += AVI->track[j].audio_index[nai[j]].len;
					nai[j]++;
				}
			}
		}


		for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_bytes = tot[j];

	} // is no opendml

	/* Reposition the file */

	gf_fseek(AVI->fdes,AVI->movi_start,SEEK_SET);
	AVI->video_pos = 0;

	return(0);
}

int AVI_video_frames(avi_t *AVI)
{
	return AVI->video_frames;
}
int  AVI_video_width(avi_t *AVI)
{
	return AVI->width;
}
int  AVI_video_height(avi_t *AVI)
{
	return AVI->height;
}
double AVI_frame_rate(avi_t *AVI)
{
	return AVI->fps;
}
char* AVI_video_compressor(avi_t *AVI)
{
	return AVI->compressor2;
}

#if 0
int AVI_max_video_chunk(avi_t *AVI)
{
	return AVI->max_len;
}
#endif

int AVI_audio_tracks(avi_t *AVI)
{
	return(AVI->anum);
}

int AVI_audio_channels(avi_t *AVI)
{
	return AVI->track[AVI->aptr].a_chans;
}

int AVI_audio_mp3rate(avi_t *AVI)
{
	return AVI->track[AVI->aptr].mp3rate;
}

#if 0 //unused
int AVI_audio_padrate(avi_t *AVI)
{
	return AVI->track[AVI->aptr].padrate;
}
#endif

int AVI_audio_bits(avi_t *AVI)
{
	return AVI->track[AVI->aptr].a_bits;
}

int AVI_audio_format(avi_t *AVI)
{
	return AVI->track[AVI->aptr].a_fmt;
}

int AVI_audio_rate(avi_t *AVI)
{
	return AVI->track[AVI->aptr].a_rate;
}


int AVI_frame_size(avi_t *AVI, int frame)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->video_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	if(frame < 0 || frame >= AVI->video_frames) return 0;
	return (u32) (AVI->video_index[frame].len);
}

int AVI_audio_size(avi_t *AVI, int frame)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->track[AVI->aptr].audio_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	if(frame < 0 || frame >= AVI->track[AVI->aptr].audio_chunks) return -1;
	return (u32) (AVI->track[AVI->aptr].audio_index[frame].len);
}

u64 AVI_get_video_position(avi_t *AVI, int frame)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return (u64) -1;
	}
	if(!AVI->video_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return (u64) -1;
	}

	if(frame < 0 || frame >= AVI->video_frames) return 0;
	return(AVI->video_index[frame].pos);
}


#if 0 //unused
int AVI_seek_start(avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}

	gf_fseek(AVI->fdes,AVI->movi_start,SEEK_SET);
	AVI->video_pos = 0;
	return 0;
}

int AVI_set_video_position(avi_t *AVI, int frame)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->video_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	if (frame < 0 ) frame = 0;
	AVI->video_pos = frame;
	return 0;
}

int AVI_set_audio_bitrate(avi_t *AVI, int bitrate)
{
	if(AVI->mode==AVI_MODE_READ) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}

	AVI->track[AVI->aptr].mp3rate = bitrate;
	return 0;
}
#endif

int AVI_read_frame(avi_t *AVI, u8 *vidbuf, int *keyframe)
{
	int n;

	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->video_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	if(AVI->video_pos < 0 || AVI->video_pos >= AVI->video_frames) return -1;
	n = (u32) AVI->video_index[AVI->video_pos].len;

	*keyframe = (AVI->video_index[AVI->video_pos].key==0x10) ? 1:0;

	if (vidbuf == NULL) {
		AVI->video_pos++;
		return n;
	}

	gf_fseek(AVI->fdes, AVI->video_index[AVI->video_pos].pos, SEEK_SET);

	if (avi_read(AVI->fdes,vidbuf,n) != (u32) n)
	{
		AVI_errno = AVI_ERR_READ;
		return -1;
	}

	AVI->video_pos++;

	return n;
}

#if 0 //unused
int AVI_get_audio_position_index(avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->track[AVI->aptr].audio_index) {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	return (AVI->track[AVI->aptr].audio_posc);
}

int AVI_set_audio_position_index(avi_t *AVI, int indexpos)
{
	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->track[AVI->aptr].audio_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}
	if(indexpos > AVI->track[AVI->aptr].audio_chunks)     {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	AVI->track[AVI->aptr].audio_posc = indexpos;
	AVI->track[AVI->aptr].audio_posb = 0;

	return 0;
}


int AVI_set_audio_position(avi_t *AVI, int byte)
{
	int n0, n1, n;

	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->track[AVI->aptr].audio_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	if(byte < 0) byte = 0;

	/* Binary search in the audio chunks */

	n0 = 0;
	n1 = AVI->track[AVI->aptr].audio_chunks;

	while(n0<n1-1)
	{
		n = (n0+n1)/2;
		if(AVI->track[AVI->aptr].audio_index[n].tot>(u32) byte)
			n1 = n;
		else
			n0 = n;
	}

	AVI->track[AVI->aptr].audio_posc = n0;
	AVI->track[AVI->aptr].audio_posb = (u32) (byte - AVI->track[AVI->aptr].audio_index[n0].tot);

	return 0;
}
#endif


int AVI_read_audio(avi_t *AVI, u8 *audbuf, int bytes, int *continuous)
{
	int nr, left, todo;
	s64 pos;

	if(AVI->mode==AVI_MODE_WRITE) {
		AVI_errno = AVI_ERR_NOT_PERM;
		return -1;
	}
	if(!AVI->track[AVI->aptr].audio_index)         {
		AVI_errno = AVI_ERR_NO_IDX;
		return -1;
	}

	nr = 0; /* total number of bytes read */

	if (bytes==0) {
		AVI->track[AVI->aptr].audio_posc++;
		AVI->track[AVI->aptr].audio_posb = 0;
	}

	*continuous = 1;
	while(bytes>0)
	{
		s64 ret;
		left = (int) (AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].len - AVI->track[AVI->aptr].audio_posb);
		if(left==0)
		{
			if(AVI->track[AVI->aptr].audio_posc>=AVI->track[AVI->aptr].audio_chunks-1) return nr;
			AVI->track[AVI->aptr].audio_posc++;
			AVI->track[AVI->aptr].audio_posb = 0;
			*continuous = 0;
			continue;
		}
		if(bytes<left)
			todo = bytes;
		else
			todo = left;
		pos = AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].pos + AVI->track[AVI->aptr].audio_posb;
		gf_fseek(AVI->fdes, pos, SEEK_SET);
		if ( (ret = avi_read(AVI->fdes,audbuf+nr,todo)) != todo)
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] XXX pos = %"LLD", ret = %"LLD", todo = %ld\n", pos, ret, todo));
			AVI_errno = AVI_ERR_READ;
			return -1;
		}
		bytes -= todo;
		nr    += todo;
		AVI->track[AVI->aptr].audio_posb += todo;
	}

	return nr;
}


#if 0 //unused
/* AVI_read_data: Special routine for reading the next audio or video chunk
                  without having an index of the file. */

int AVI_read_data(avi_t *AVI, char *vidbuf, int max_vidbuf,
                  char *audbuf, int max_audbuf,
                  int *len)
{

	/*
	 * Return codes:
	 *
	 *    1 = video data read
	 *    2 = audio data read
	 *    0 = reached EOF
	 *   -1 = video buffer too small
	 *   -2 = audio buffer too small
	 */

	s64 n;
	char data[8];

	if(AVI->mode==AVI_MODE_WRITE) return 0;

	while(1)
	{
		/* Read tag and length */

		if( avi_read(AVI->fdes,data,8) != 8 ) return 0;

		/* if we got a list tag, ignore it */

		if(strnicmp(data,"LIST",4) == 0)
		{
			gf_fseek(AVI->fdes,4,SEEK_CUR);
			continue;
		}

		n = PAD_EVEN(str2ulong((unsigned char *)data+4));

		if(strnicmp(data,AVI->video_tag,3) == 0)
		{
			*len = (u32) n;
			AVI->video_pos++;
			if(n>max_vidbuf)
			{
				gf_fseek(AVI->fdes,n,SEEK_CUR);
				return -1;
			}
			if(avi_read(AVI->fdes,vidbuf, (u32) n) != n ) return 0;
			return 1;
		}
		else if(strnicmp(data,AVI->track[AVI->aptr].audio_tag,4) == 0)
		{
			*len = (u32) n;
			if(n>max_audbuf)
			{
				gf_fseek(AVI->fdes,n,SEEK_CUR);
				return -2;
			}
			if(avi_read(AVI->fdes,audbuf, (u32) n) != n ) return 0;
			return 2;
			break;
		}
		else if(gf_fseek(AVI->fdes,n,SEEK_CUR) == (u64) -1)  return 0;
	}
}

u64 AVI_max_size(void)
{
	return((u64) AVI_MAX_LEN);
}
#endif


#endif /*GPAC_DISABLE_AVILIB*/
