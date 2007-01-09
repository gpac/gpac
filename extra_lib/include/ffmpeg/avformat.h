/*
 * copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVFORMAT_H
#define AVFORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIBAVFORMAT_VERSION_INT ((51<<16)+(6<<8)+0)
#define LIBAVFORMAT_VERSION     51.6.0
#define LIBAVFORMAT_BUILD       LIBAVFORMAT_VERSION_INT

#define LIBAVFORMAT_IDENT       "Lavf" AV_STRINGIFY(LIBAVFORMAT_VERSION)

#include <time.h>
#include <stdio.h>  /* FILE */
#include "avcodec.h"

#include "avio.h"

/* packet functions */

#ifndef MAXINT64
#define MAXINT64 int64_t_C(0x7fffffffffffffff)
#endif

#ifndef MININT64
#define MININT64 int64_t_C(0x8000000000000000)
#endif

typedef struct AVPacket {
    int64_t pts;                            ///< presentation time stamp in time_base units
    int64_t dts;                            ///< decompression time stamp in time_base units
    uint8_t *data;
    int   size;
    int   stream_index;
    int   flags;
    int   duration;                         ///< presentation duration in time_base units (0 if not available)
    void  (*destruct)(struct AVPacket *);
    void  *priv;
    int64_t pos;                            ///< byte position in stream, -1 if unknown
} AVPacket;
#define PKT_FLAG_KEY   0x0001

void av_destruct_packet_nofree(AVPacket *pkt);
void av_destruct_packet(AVPacket *pkt);

/* initialize optional fields of a packet */
static inline void av_init_packet(AVPacket *pkt)
{
    pkt->pts   = AV_NOPTS_VALUE;
    pkt->dts   = AV_NOPTS_VALUE;
    pkt->pos   = -1;
    pkt->duration = 0;
    pkt->flags = 0;
    pkt->stream_index = 0;
    pkt->destruct= av_destruct_packet_nofree;
}

int av_new_packet(AVPacket *pkt, int size);
int av_get_packet(ByteIOContext *s, AVPacket *pkt, int size);
int av_dup_packet(AVPacket *pkt);

/**
 * Free a packet
 *
 * @param pkt packet to free
 */
static inline void av_free_packet(AVPacket *pkt)
{
    if (pkt && pkt->destruct) {
        pkt->destruct(pkt);
    }
}

/*************************************************/
/* fractional numbers for exact pts handling */

/* the exact value of the fractional number is: 'val + num / den'. num
   is assumed to be such as 0 <= num < den */
typedef struct AVFrac {
    int64_t val, num, den;
} AVFrac attribute_deprecated;

/*************************************************/
/* input/output formats */

struct AVFormatContext;

/* this structure contains the data a format has to probe a file */
typedef struct AVProbeData {
    const char *filename;
    unsigned char *buf;
    int buf_size;
} AVProbeData;

#define AVPROBE_SCORE_MAX 100               ///< max score, half of that is used for file extension based detection

typedef struct AVFormatParameters {
    AVRational time_base;
    int sample_rate;
    int channels;
    int width;
    int height;
    enum PixelFormat pix_fmt;
    int channel; /* used to select dv channel */
    const char *device; /* video, audio or DV device */
    const char *standard; /* tv standard, NTSC, PAL, SECAM */
    int mpeg2ts_raw:1;  /* force raw MPEG2 transport stream output, if possible */
    int mpeg2ts_compute_pcr:1; /* compute exact PCR for each transport
                                  stream packet (only meaningful if
                                  mpeg2ts_raw is TRUE */
    int initial_pause:1;       /* do not begin to play the stream
                                  immediately (RTSP only) */
    int prealloced_context:1;
    enum CodecID video_codec_id;
    enum CodecID audio_codec_id;
} AVFormatParameters;

#define AVFMT_NOFILE        0x0001 /* no file should be opened */
#define AVFMT_NEEDNUMBER    0x0002 /* needs '%d' in filename */
#define AVFMT_SHOW_IDS      0x0008 /* show format stream IDs numbers */
#define AVFMT_RAWPICTURE    0x0020 /* format wants AVPicture structure for
                                      raw picture data */
#define AVFMT_GLOBALHEADER  0x0040 /* format wants global header */
#define AVFMT_NOTIMESTAMPS  0x0080 /* format doesnt need / has any timestamps */

typedef struct AVOutputFormat {
    const char *name;
    const char *long_name;
    const char *mime_type;
    const char *extensions; /* comma separated extensions */
    /* size of private data so that it can be allocated in the wrapper */
    int priv_data_size;
    /* output support */
    enum CodecID audio_codec; /* default audio codec */
    enum CodecID video_codec; /* default video codec */
    int (*write_header)(struct AVFormatContext *);
    int (*write_packet)(struct AVFormatContext *, AVPacket *pkt);
    int (*write_trailer)(struct AVFormatContext *);
    /* can use flags: AVFMT_NOFILE, AVFMT_NEEDNUMBER, AVFMT_GLOBALHEADER */
    int flags;
    /* currently only used to set pixel format if not YUV420P */
    int (*set_parameters)(struct AVFormatContext *, AVFormatParameters *);
    int (*interleave_packet)(struct AVFormatContext *, AVPacket *out, AVPacket *in, int flush);
    /* private fields */
    struct AVOutputFormat *next;
} AVOutputFormat;

typedef struct AVInputFormat {
    const char *name;
    const char *long_name;
    /* size of private data so that it can be allocated in the wrapper */
    int priv_data_size;
    /* tell if a given file has a chance of being parsing by this format */
    int (*read_probe)(AVProbeData *);
    /* read the format header and initialize the AVFormatContext
       structure. Return 0 if OK. 'ap' if non NULL contains
       additionnal paramters. Only used in raw format right
       now. 'av_new_stream' should be called to create new streams.  */
    int (*read_header)(struct AVFormatContext *,
                       AVFormatParameters *ap);
    /* read one packet and put it in 'pkt'. pts and flags are also
       set. 'av_new_stream' can be called only if the flag
       AVFMTCTX_NOHEADER is used. */
    int (*read_packet)(struct AVFormatContext *, AVPacket *pkt);
    /* close the stream. The AVFormatContext and AVStreams are not
       freed by this function */
    int (*read_close)(struct AVFormatContext *);
    /**
     * seek to a given timestamp relative to the frames in
     * stream component stream_index
     * @param stream_index must not be -1
     * @param flags selects which direction should be preferred if no exact
     *              match is available
     */
    int (*read_seek)(struct AVFormatContext *,
                     int stream_index, int64_t timestamp, int flags);
    /**
     * gets the next timestamp in AV_TIME_BASE units.
     */
    int64_t (*read_timestamp)(struct AVFormatContext *s, int stream_index,
                              int64_t *pos, int64_t pos_limit);
    /* can use flags: AVFMT_NOFILE, AVFMT_NEEDNUMBER */
    int flags;
    /* if extensions are defined, then no probe is done. You should
       usually not use extension format guessing because it is not
       reliable enough */
    const char *extensions;
    /* general purpose read only value that the format can use */
    int value;

    /* start/resume playing - only meaningful if using a network based format
       (RTSP) */
    int (*read_play)(struct AVFormatContext *);

    /* pause playing - only meaningful if using a network based format
       (RTSP) */
    int (*read_pause)(struct AVFormatContext *);

    /* private fields */
    struct AVInputFormat *next;
} AVInputFormat;

typedef struct AVIndexEntry {
    int64_t pos;
    int64_t timestamp;
#define AVINDEX_KEYFRAME 0x0001
    int flags:2;
    int size:30; //yeah trying to keep the size of this small to reduce memory requirements (its 24 vs 32 byte due to possible 8byte align)
    int min_distance;         /* min distance between this and the previous keyframe, used to avoid unneeded searching */
} AVIndexEntry;

typedef struct AVStream {
    int index;    /* stream index in AVFormatContext */
    int id;       /* format specific stream id */
    AVCodecContext *codec; /* codec context */
    /**
     * real base frame rate of the stream.
     * this is the lowest framerate with which all timestamps can be
     * represented accurately (its the least common multiple of all
     * framerates in the stream), Note, this value is just a guess!
     * for example if the timebase is 1/90000 and all frames have either
     * approximately 3600 or 1800 timer ticks then r_frame_rate will be 50/1
     */
    AVRational r_frame_rate;
    void *priv_data;
    /* internal data used in av_find_stream_info() */
    int64_t codec_info_duration;
    int codec_info_nb_frames;
    /* encoding: PTS generation when outputing stream */
    AVFrac pts;

    /**
     * this is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. for fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identically 1.
     */
    AVRational time_base;
    int pts_wrap_bits; /* number of bits in pts (used for wrapping control) */
    /* ffmpeg.c private use */
    int stream_copy; /* if TRUE, just copy stream */
    enum AVDiscard discard; ///< selects which packets can be discarded at will and dont need to be demuxed
    //FIXME move stuff to a flags field?
    /* quality, as it has been removed from AVCodecContext and put in AVVideoFrame
     * MN:dunno if thats the right place, for it */
    float quality;
    /* decoding: position of the first frame of the component, in
       AV_TIME_BASE fractional seconds. */
    int64_t start_time;
    /* decoding: duration of the stream, in AV_TIME_BASE fractional
       seconds. */
    int64_t duration;

    char language[4]; /* ISO 639 3-letter language code (empty string if undefined) */

    /* av_read_frame() support */
    int need_parsing;                  ///< 1->full parsing needed, 2->only parse headers dont repack
    struct AVCodecParserContext *parser;

    int64_t cur_dts;
    int last_IP_duration;
    int64_t last_IP_pts;
    /* av_seek_frame() support */
    AVIndexEntry *index_entries; /* only used if the format does not
                                    support seeking natively */
    int nb_index_entries;
    unsigned int index_entries_allocated_size;

    int64_t nb_frames;                 ///< number of frames in this stream if known or 0

#define MAX_REORDER_DELAY 4
    int64_t pts_buffer[MAX_REORDER_DELAY+1];
} AVStream;

#define AVFMTCTX_NOHEADER      0x0001 /* signal that no header is present
                                         (streams are added dynamically) */

#define MAX_STREAMS 20

/* format I/O context */
typedef struct AVFormatContext {
    const AVClass *av_class; /* set by av_alloc_format_context */
    /* can only be iformat or oformat, not both at the same time */
    struct AVInputFormat *iformat;
    struct AVOutputFormat *oformat;
    void *priv_data;
    ByteIOContext pb;
    int nb_streams;
    AVStream *streams[MAX_STREAMS];
    char filename[1024]; /* input or output filename */
    /* stream info */
    int64_t timestamp;
    char title[512];
    char author[512];
    char copyright[512];
    char comment[512];
    char album[512];
    int year;  /* ID3 year, 0 if none */
    int track; /* track number, 0 if none */
    char genre[32]; /* ID3 genre */

    int ctx_flags; /* format specific flags, see AVFMTCTX_xx */
    /* private data for pts handling (do not modify directly) */
    /* This buffer is only needed when packets were already buffered but
       not decoded, for example to get the codec parameters in mpeg
       streams */
    struct AVPacketList *packet_buffer;

    /* decoding: position of the first frame of the component, in
       AV_TIME_BASE fractional seconds. NEVER set this value directly:
       it is deduced from the AVStream values.  */
    int64_t start_time;
    /* decoding: duration of the stream, in AV_TIME_BASE fractional
       seconds. NEVER set this value directly: it is deduced from the
       AVStream values.  */
    int64_t duration;
    /* decoding: total file size. 0 if unknown */
    int64_t file_size;
    /* decoding: total stream bitrate in bit/s, 0 if not
       available. Never set it directly if the file_size and the
       duration are known as ffmpeg can compute it automatically. */
    int bit_rate;

    /* av_read_frame() support */
    AVStream *cur_st;
    const uint8_t *cur_ptr;
    int cur_len;
    AVPacket cur_pkt;

    /* av_seek_frame() support */
    int64_t data_offset; /* offset of the first packet */
    int index_built;

    int mux_rate;
    int packet_size;
    int preload;
    int max_delay;

#define AVFMT_NOOUTPUTLOOP -1
#define AVFMT_INFINITEOUTPUTLOOP 0
    /* number of times to loop output in formats that support it */
    int loop_output;

    int flags;
#define AVFMT_FLAG_GENPTS       0x0001 ///< generate pts if missing even if it requires parsing future frames
#define AVFMT_FLAG_IGNIDX       0x0002 ///< ignore index

    int loop_input;
    /* decoding: size of data to probe; encoding unused */
    unsigned int probesize;
} AVFormatContext;

typedef struct AVPacketList {
    AVPacket pkt;
    struct AVPacketList *next;
} AVPacketList;

extern AVInputFormat *first_iformat;
extern AVOutputFormat *first_oformat;

enum CodecID av_guess_image2_codec(const char *filename);

/* XXX: use automatic init with either ELF sections or C file parser */
/* modules */

/* utils.c */
void av_register_input_format(AVInputFormat *format);
void av_register_output_format(AVOutputFormat *format);
AVOutputFormat *guess_stream_format(const char *short_name,
                                    const char *filename, const char *mime_type);
AVOutputFormat *guess_format(const char *short_name,
                             const char *filename, const char *mime_type);
enum CodecID av_guess_codec(AVOutputFormat *fmt, const char *short_name,
                            const char *filename, const char *mime_type, enum CodecType type);

void av_hex_dump(FILE *f, uint8_t *buf, int size);
void av_pkt_dump(FILE *f, AVPacket *pkt, int dump_payload);

void av_register_all(void);

/* media file input */
AVInputFormat *av_find_input_format(const char *short_name);
AVInputFormat *av_probe_input_format(AVProbeData *pd, int is_opened);
int av_open_input_stream(AVFormatContext **ic_ptr,
                         ByteIOContext *pb, const char *filename,
                         AVInputFormat *fmt, AVFormatParameters *ap);
int av_open_input_file(AVFormatContext **ic_ptr, const char *filename,
                       AVInputFormat *fmt,
                       int buf_size,
                       AVFormatParameters *ap);
/* no av_open for output, so applications will need this: */
AVFormatContext *av_alloc_format_context(void);

#define AVERROR_UNKNOWN     (-1)  /* unknown error */
#define AVERROR_IO          (-2)  /* i/o error */
#define AVERROR_NUMEXPECTED (-3)  /* number syntax expected in filename */
#define AVERROR_INVALIDDATA (-4)  /* invalid data found */
#define AVERROR_NOMEM       (-5)  /* not enough memory */
#define AVERROR_NOFMT       (-6)  /* unknown format */
#define AVERROR_NOTSUPP     (-7)  /* operation not supported */

int av_find_stream_info(AVFormatContext *ic);
int av_read_packet(AVFormatContext *s, AVPacket *pkt);
int av_read_frame(AVFormatContext *s, AVPacket *pkt);
int av_seek_frame(AVFormatContext *s, int stream_index, int64_t timestamp, int flags);
int av_read_play(AVFormatContext *s);
int av_read_pause(AVFormatContext *s);
void av_close_input_file(AVFormatContext *s);
AVStream *av_new_stream(AVFormatContext *s, int id);
void av_set_pts_info(AVStream *s, int pts_wrap_bits,
                     int pts_num, int pts_den);

#define AVSEEK_FLAG_BACKWARD 1 ///< seek backward
#define AVSEEK_FLAG_BYTE     2 ///< seeking based on position in bytes
#define AVSEEK_FLAG_ANY      4 ///< seek to any frame, even non keyframes

int av_find_default_stream_index(AVFormatContext *s);
int av_index_search_timestamp(AVStream *st, int64_t timestamp, int flags);
int av_add_index_entry(AVStream *st,
                       int64_t pos, int64_t timestamp, int size, int distance, int flags);
int av_seek_frame_binary(AVFormatContext *s, int stream_index, int64_t target_ts, int flags);
void av_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp);
int64_t av_gen_search(AVFormatContext *s, int stream_index, int64_t target_ts, int64_t pos_min, int64_t pos_max, int64_t pos_limit, int64_t ts_min, int64_t ts_max, int flags, int64_t *ts_ret, int64_t (*read_timestamp)(struct AVFormatContext *, int , int64_t *, int64_t ));

/* media file output */
int av_set_parameters(AVFormatContext *s, AVFormatParameters *ap);
int av_write_header(AVFormatContext *s);
int av_write_frame(AVFormatContext *s, AVPacket *pkt);
int av_interleaved_write_frame(AVFormatContext *s, AVPacket *pkt);
int av_interleave_packet_per_dts(AVFormatContext *s, AVPacket *out, AVPacket *pkt, int flush);

int av_write_trailer(AVFormatContext *s);

void dump_format(AVFormatContext *ic,
                 int index,
                 const char *url,
                 int is_output);
int parse_image_size(int *width_ptr, int *height_ptr, const char *str);
int parse_frame_rate(int *frame_rate, int *frame_rate_base, const char *arg);
int64_t parse_date(const char *datestr, int duration);

int64_t av_gettime(void);

/* ffm specific for ffserver */
#define FFM_PACKET_SIZE 4096
offset_t ffm_read_write_index(int fd);
void ffm_write_write_index(int fd, offset_t pos);
void ffm_set_write_index(AVFormatContext *s, offset_t pos, offset_t file_size);

int find_info_tag(char *arg, int arg_size, const char *tag1, const char *info);

int av_get_frame_filename(char *buf, int buf_size,
                          const char *path, int number);
int av_filename_number_test(const char *filename);

/* grab specific */
int video_grab_init(void);
int audio_init(void);

/* DV1394 */
int dv1394_init(void);
int dc1394_init(void);

#ifdef HAVE_AV_CONFIG_H

#include "os_support.h"

int strstart(const char *str, const char *val, const char **ptr);
int stristart(const char *str, const char *val, const char **ptr);
void pstrcpy(char *buf, int buf_size, const char *str);
char *pstrcat(char *buf, int buf_size, const char *s);

void __dynarray_add(unsigned long **tab_ptr, int *nb_ptr, unsigned long elem);

#ifdef __GNUC__
#define dynarray_add(tab, nb_ptr, elem)\
do {\
    typeof(tab) _tab = (tab);\
    typeof(elem) _elem = (elem);\
    (void)sizeof(**_tab == _elem); /* check that types are compatible */\
    __dynarray_add((unsigned long **)_tab, nb_ptr, (unsigned long)_elem);\
} while(0)
#else
#define dynarray_add(tab, nb_ptr, elem)\
do {\
    __dynarray_add((unsigned long **)(tab), nb_ptr, (unsigned long)(elem));\
} while(0)
#endif

time_t mktimegm(struct tm *tm);
struct tm *brktimegm(time_t secs, struct tm *tm);
const char *small_strptime(const char *p, const char *fmt,
                           struct tm *dt);

struct in_addr;
int resolve_host(struct in_addr *sin_addr, const char *hostname);

void url_split(char *proto, int proto_size,
               char *authorization, int authorization_size,
               char *hostname, int hostname_size,
               int *port_ptr,
               char *path, int path_size,
               const char *url);

int match_ext(const char *filename, const char *extensions);

#endif /* HAVE_AV_CONFIG_H */

#ifdef __cplusplus
}
#endif

#endif /* AVFORMAT_H */

