#include "ts_muxer.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#ifdef AVIO_FLAG_WRITE
#include <libavutil/samplefmt.h>
#define url_fopen avio_open
#define url_fclose avio_close
#define URL_WRONLY	AVIO_FLAG_WRITE
#define av_write_header(__a)	avformat_write_header(__a, NULL)
#define dump_format av_dump_format

#define SAMPLE_FMT_S16 AV_SAMPLE_FMT_S16
#endif



#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_NB_FRAMES  ((int)(STREAM_DURATION * STREAM_FRAME_RATE))
#define STREAM_PIX_FMT PIX_FMT_YUV420P /* default pix_fmt */

#define PACKETS_BUFFER_LEN 1024

struct avr_ts_muxer {
	AVFormatContext *oc;
	AVStream *audio_st, *video_st;
	AVPacketList * videoPackets;
	AVPacketList * audioPackets;
	volatile Bool encode;
	GF_Mutex *videoMx, *audioMx;
	GF_Thread * tsEncodingThread;
	const char * destination;
};

static Bool has_packet_ready(GF_AbstractTSMuxer* ts, GF_Mutex * mx, AVPacketList ** pkts) {
	Bool ret;
	gf_mx_p(mx);
	ret = (*pkts) != NULL;
	gf_mx_v(mx);
	return ret;
}


/*!
 * Wait for a packet on a given queue
 * \param ts The muxer
 * \param mx The mutex to use
 * \param pkts The list of packets to watch
 * \param idx a pointer to the index in the list
 * \return NULL if encoding is ended, the first packet available otherwise
 */
static AVPacketList * wait_for_packet(GF_AbstractTSMuxer* ts, GF_Mutex * mx, AVPacketList ** pkts) {
	AVPacketList * p;
	gf_mx_p(mx);
	while (ts->encode && !(*pkts)) {
		gf_mx_v(mx);
		gf_mx_p(mx);
		gf_sleep(1);
	}
	if (!ts->encode) {
		gf_mx_v(mx);
		return NULL;
	}
	p = *pkts;
	*pkts = p->next;
	gf_mx_v(mx);
	return p;
}


static u32 ts_interleave_thread_run(void *param) {
	GF_AbstractTSMuxer * mux = (GF_AbstractTSMuxer *) param;
	AVStream * video_st = mux->video_st;
	AVStream * audio_st = mux->audio_st;
	u64 audio_pts, video_pts;
	u64 audioSize, videoSize, videoKbps, audioKbps;
	u32 pass;
	u32 now, start;
	/* open the output file, if needed */
	if (!(mux->oc->oformat->flags & AVFMT_NOFILE)) {
		if (url_fopen(&mux->oc->pb, mux->destination, URL_WRONLY) < 0) {
			fprintf(stderr, "Could not open '%s'\n", mux->destination);
			return 0;
		}
	}
	/* write the stream header, if any */
	av_write_header(mux->oc);
	audio_pts = video_pts = 0;
	// Buffering...
	gf_sleep(1000);
	now = start = gf_sys_clock();
	audioSize = videoSize = 0;
	audioKbps = videoKbps = 0;
	pass = 0;
	while ( mux->encode) {
		pass++;
		if (0== (pass%16)) {
			now = gf_sys_clock();
			if (now - start > 1000) {
				videoKbps = videoSize * 8000 / (now-start) / 1024;
				audioKbps = audioSize * 8000 / (now-start) / 1024;
				audioSize = videoSize = 0;
				start = now;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("\rPTS audio="LLU" ("LLU"kbps), video="LLU" ("LLU"kbps)", audio_pts, audioKbps, video_pts, videoKbps));
			}
		}
		/* write interleaved audio and video frames */
		if (!video_st ||
		        (audio_pts == AV_NOPTS_VALUE && has_packet_ready(mux, mux->audioMx, &mux->audioPackets)) ||
		        ((audio_st && audio_pts < video_pts && audio_pts!= AV_NOPTS_VALUE))) {
			AVPacketList * pl = wait_for_packet(mux, mux->audioMx, &mux->audioPackets);
			if (!pl)
				goto exit;
			audio_pts = pl->pkt.pts ;
			audioSize+=pl->pkt.size;
			if (pl->pkt.pts == AV_NOPTS_VALUE) {
				pl->pkt.pts = 0;
			}
			if (av_interleaved_write_frame(mux->oc, &(pl->pkt)) < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] : failed to write audio interleaved frame audio_pts="LLU", video_pts="LLU"\n", audio_pts, video_pts));
			}
			gf_free(pl);
		} else {
			AVPacketList * pl = wait_for_packet(mux, mux->videoMx, &mux->videoPackets);
			if (!pl)
				goto exit;
			video_pts = pl->pkt.pts;
			/* write the compressed frame in the media file */
			if (0 && audio_pts != AV_NOPTS_VALUE && audio_pts > video_pts && pl->next) {
				u32 skipped = 0;
				u64 first = video_pts;
				/* We may be too slow... */
				gf_mx_p(mux->videoMx);
				while (video_pts < audio_pts && pl->next) {
					AVPacketList * old = pl;
					// We skip frames...
					pl = pl->next;
					video_pts = pl->pkt.pts;
					skipped++;
					gf_free(old);
				}
				mux->videoPackets = pl->next;
				gf_mx_v(mux->videoMx);
				if (skipped > 0)
					GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Skipped %u video frames, frame was "LLU", but is now "LLU"\n", skipped, first, video_pts));
			}
			videoSize+=pl->pkt.size;
			video_pts = pl->pkt.pts; // * video_st->time_base.num / video_st->time_base.den;
			assert( video_pts);
			if (av_interleaved_write_frame(mux->oc, &(pl->pkt)) < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] : failed to write video interleaved frame audio_pts="LLU", video_pts="LLU"\n", audio_pts, video_pts));
			}
			gf_free(pl);
		}
		gf_sleep(1);
	}
exit:
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[AVRedirect] Ending TS thread...\n"));
	av_write_trailer(mux->oc);
	if (!(mux->oc->oformat->flags & AVFMT_NOFILE)) {
		/* close the output file */
		url_fclose(mux->oc->pb);
	}
	return 0;
}


#if ((LIBAVFORMAT_VERSION_MAJOR == 52) && (LIBAVFORMAT_VERSION_MINOR <= 47)) || (LIBAVFORMAT_VERSION_MAJOR < 52)
#define GUESS_FORMAT guess_stream_format
#else
#define GUESS_FORMAT av_guess_format
#endif

GF_AbstractTSMuxer * ts_amux_new(GF_AVRedirect * avr, u32 videoBitrateInBitsPerSec, u32 width, u32 height, u32 audioBitRateInBitsPerSec) {
	GF_AbstractTSMuxer * ts = (GF_AbstractTSMuxer*)gf_malloc( sizeof(GF_AbstractTSMuxer));
	memset( ts, 0, sizeof( GF_AbstractTSMuxer));
	ts->oc = avformat_alloc_context();
	ts->destination = avr->destination;
	av_register_all();
	ts->oc->oformat = GUESS_FORMAT(NULL, avr->destination, NULL);
	if (!ts->oc->oformat)
		ts->oc->oformat = GUESS_FORMAT("mpegts", NULL, NULL);
	assert( ts->oc->oformat);
#if REDIRECT_AV_AUDIO_ENABLED

#ifdef FF_API_AVFRAME_LAVC
	ts->audio_st = avformat_new_stream(ts->oc, avr->audioCodec);
#else
	ts->audio_st = av_new_stream(ts->oc, avr->audioCodec->id);
#endif
	{
		AVCodecContext * c = ts->audio_st->codec;
		c->codec_id = avr->audioCodec->id;
		c->codec_type = AVMEDIA_TYPE_AUDIO;
		/* put sample parameters */
		c->sample_fmt = SAMPLE_FMT_S16;
		c->bit_rate = audioBitRateInBitsPerSec;
		c->sample_rate = avr->audioSampleRate;
		c->channels = 2;
		c->time_base.num = 1;
		c->time_base.den = 1000;
		// some formats want stream headers to be separate
		if (ts->oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
#endif

#ifdef FF_API_AVFRAME_LAVC
	ts->video_st = avformat_new_stream(ts->oc, avr->videoCodec);
#else
	ts->video_st = av_new_stream(ts->oc, avr->videoCodec->id);
#endif
	{
		AVCodecContext * c = ts->video_st->codec;
		c->codec_id = avr->videoCodec->id;
		c->codec_type = AVMEDIA_TYPE_VIDEO;

		/* put sample parameters */
		c->bit_rate = videoBitrateInBitsPerSec;
		/* resolution must be a multiple of two */
		c->width = width;
		c->height = height;
		/* time base: this is the fundamental unit of time (in seconds) in terms
		   of which frame timestamps are represented. for fixed-fps content,
		   timebase should be 1/framerate and timestamp increments should be
		   identically 1. */
		c->time_base.den = STREAM_FRAME_RATE;
		c->time_base.num = 1;
		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;
		if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			   This does not happen with normal video, it just happens here as
			   the motion of the chroma plane does not match the luma plane. */
			c->mb_decision=2;
		}
		// some formats want stream headers to be separate
		if (ts->oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	}
	//av_set_pts_info(ts->audio_st, 33, 1, audioBitRateInBitsPerSec);

#ifndef AVIO_FLAG_WRITE
	/* set the output parameters (must be done even if no
	   parameters). */
	if (av_set_parameters(ts->oc, NULL) < 0) {
		fprintf(stderr, "Invalid output format parameters\n");
		return NULL;
	}
#endif

	dump_format(ts->oc, 0, avr->destination, 1);
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[AVRedirect] DUMPING to %s...\n", ts->destination));

#if (LIBAVCODEC_VERSION_MAJOR<55)
	if (avcodec_open(ts->video_st->codec, avr->videoCodec) < 0) {
#else
	if (avcodec_open2(ts->video_st->codec, avr->videoCodec, NULL) < 0) {
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] failed to open video codec\n"));
		return NULL;
	}
#if REDIRECT_AV_AUDIO_ENABLED
#if (LIBAVCODEC_VERSION_MAJOR<55)
	if (avcodec_open(ts->audio_st->codec, avr->audioCodec) < 0) {
#else
	if (avcodec_open2(ts->audio_st->codec, avr->audioCodec, NULL) < 0) {
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] failed to open audio codec\n"));
		return NULL;
	}
	ts->audioMx = gf_mx_new("TS_AudioMx");
#endif
	ts->videoMx = gf_mx_new("TS_VideoMx");
	ts->tsEncodingThread = gf_th_new("ts_interleave_thread_run");
	ts->encode = GF_TRUE;
	ts->audioPackets = NULL;
	ts->videoPackets = NULL;
	gf_th_run(ts->tsEncodingThread, ts_interleave_thread_run, ts);
	return ts;
}

void ts_amux_del(GF_AbstractTSMuxer * muxerToDelete) {
	if (!muxerToDelete)
		return;
	muxerToDelete->encode = GF_FALSE;
	gf_sleep(100);
	gf_th_stop(muxerToDelete->tsEncodingThread);
	muxerToDelete->tsEncodingThread = NULL;
#if REDIRECT_AV_AUDIO_ENABLED
	gf_mx_del(muxerToDelete->audioMx);
	muxerToDelete->audioMx = NULL;
#endif
	gf_mx_del(muxerToDelete->videoMx);
	muxerToDelete->videoMx = NULL;
	if (muxerToDelete->video_st) {
		avcodec_close(muxerToDelete->video_st->codec);
		muxerToDelete->video_st = NULL;
	}
#if REDIRECT_AV_AUDIO_ENABLED
	if (muxerToDelete->audio_st) {
		avcodec_close(muxerToDelete->audio_st->codec);
		muxerToDelete->audio_st = NULL;
	}
#endif
	/* write the trailer, if any.  the trailer must be written
	 * before you close the CodecContexts open when you wrote the
	 * header; otherwise write_trailer may try to use memory that
	 * was freed on av_codec_close() */
	if (muxerToDelete->oc) {
		u32 i;
		/* free the streams */
		for (i = 0; i < muxerToDelete->oc->nb_streams; i++) {
			av_freep(&muxerToDelete->oc->streams[i]->codec);
			av_freep(&muxerToDelete->oc->streams[i]);
		}

		/* free the stream */
		av_free(muxerToDelete->oc);
		muxerToDelete->oc = NULL;
	}
}

Bool ts_encode_audio_frame(GF_AbstractTSMuxer * ts, uint8_t * data, int encoded, u64 pts) {
	AVPacketList *pl;
	AVPacket * pkt;
	if (!ts->encode)
		return GF_TRUE;
	pl = (AVPacketList*)gf_malloc(sizeof(AVPacketList));
	pl->next = NULL;
	pkt = &(pl->pkt);
	av_init_packet(pkt);
	assert( ts->audio_st);
	assert( ts->audio_st->codec);
	pkt->flags = 0;
	if (ts->audio_st->codec->coded_frame) {
		if (ts->audio_st->codec->coded_frame->key_frame)
			pkt->flags = AV_PKT_FLAG_KEY;
		if (ts->audio_st->codec->coded_frame->pts != AV_NOPTS_VALUE) {
			pkt->pts = av_rescale_q(ts->audio_st->codec->coded_frame->pts, ts->audio_st->codec->time_base, ts->audio_st->time_base);
		} else {
			if (pts == AV_NOPTS_VALUE)
				pkt->pts = AV_NOPTS_VALUE;
			else {
				pkt->pts = av_rescale_q(pts, ts->audio_st->codec->time_base, ts->audio_st->time_base);
			}
		}
	} else {
		if (pts == AV_NOPTS_VALUE)
			pkt->pts = AV_NOPTS_VALUE;
		else
			pkt->pts = av_rescale_q(pts, ts->audio_st->codec->time_base, ts->audio_st->time_base);
	}
	pkt->stream_index= ts->audio_st->index;
	pkt->data = data;
	pkt->size = encoded;
	//fprintf(stderr, "AUDIO PTS="LLU" was: "LLU" (%p)\n", pkt->pts, pts, pl);
	gf_mx_p(ts->audioMx);
	if (!ts->audioPackets)
		ts->audioPackets = pl;
	else {
		AVPacketList * px = ts->audioPackets;
		while (px->next)
			px = px->next;
		px->next = pl;
	}
	gf_mx_v(ts->audioMx);
	return GF_FALSE;
}

Bool ts_encode_video_frame(GF_AbstractTSMuxer* ts, uint8_t* data, int encoded) {
	AVPacketList *pl;
	AVPacket * pkt;
	if (!ts->encode)
		return GF_TRUE;
	pl = (AVPacketList*)gf_malloc(sizeof(AVPacketList));
	pl->next = NULL;
	pkt = &(pl->pkt);

	av_init_packet(pkt);

	if (ts->video_st->codec->coded_frame->pts != AV_NOPTS_VALUE) {
		//pkt->pts= av_rescale_q(ts->video_st->codec->coded_frame->pts, ts->video_st->codec->time_base, ts->video_st->time_base);
		pkt->pts = ts->video_st->codec->coded_frame->pts * ts->video_st->time_base.den / ts->video_st->time_base.num / 1000;
		//pkt->pts = ts->video_st->codec->coded_frame->pts;
	}
	if (ts->video_st->codec->coded_frame->key_frame)
		pkt->flags |= AV_PKT_FLAG_KEY;
	pkt->stream_index= ts->video_st->index;
	pkt->data= data;
	pkt->size= encoded;
	//fprintf(stderr, "VIDEO PTS="LLU" was: "LLU" (%p)\n", pkt->pts, ts->video_st->codec->coded_frame->pts, pl);
	gf_mx_p(ts->videoMx);
	if (!ts->videoPackets)
		ts->videoPackets = pl;
	else {
		AVPacketList * px = ts->videoPackets;
		while (px->next)
			px = px->next;
		px->next = pl;
	}
	gf_mx_v(ts->videoMx);
	return GF_FALSE;
}

AVCodecContext * ts_get_video_codec_context(GF_AbstractTSMuxer * ts) {
	return !ts ? NULL : ts->video_st->codec;
}

AVCodecContext * ts_get_audio_codec_context(GF_AbstractTSMuxer * ts) {
	return !ts ? NULL : ts->audio_st->codec;
}
