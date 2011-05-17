/*
 * RTSP definitions
 * Copyright (c) 2002 Fabrice Bellard
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
#ifndef AVFORMAT_RTSP_H
#define AVFORMAT_RTSP_H

#include <stdint.h>
#include "avformat.h"
#include "rtspcodes.h"
#include "rtpdec.h"
#include "network.h"
#include "httpauth.h"

/**
 * Network layer over which RTP/etc packet data will be transported.
 */
enum RTSPLowerTransport {
    RTSP_LOWER_TRANSPORT_UDP = 0,           /**< UDP/unicast */
    RTSP_LOWER_TRANSPORT_TCP = 1,           /**< TCP; interleaved in RTSP */
    RTSP_LOWER_TRANSPORT_UDP_MULTICAST = 2, /**< UDP/multicast */
    RTSP_LOWER_TRANSPORT_NB
};

/**
 * Packet profile of the data that we will be receiving. Real servers
 * commonly send RDT (although they can sometimes send RTP as well),
 * whereas most others will send RTP.
 */
enum RTSPTransport {
    RTSP_TRANSPORT_RTP, /**< Standards-compliant RTP */
    RTSP_TRANSPORT_RDT, /**< Realmedia Data Transport */
    RTSP_TRANSPORT_NB
};

#define RTSP_DEFAULT_PORT   554
#define RTSP_MAX_TRANSPORTS 8
#define RTSP_TCP_MAX_PACKET_SIZE 1472
#define RTSP_DEFAULT_NB_AUDIO_CHANNELS 2
#define RTSP_DEFAULT_AUDIO_SAMPLERATE 44100
#define RTSP_RTP_PORT_MIN 5000
#define RTSP_RTP_PORT_MAX 10000

/**
 * This describes a single item in the "Transport:" line of one stream as
 * negotiated by the SETUP RTSP command. Multiple transports are comma-
 * separated ("Transport: x-read-rdt/tcp;interleaved=0-1,rtp/avp/udp;
 * client_port=1000-1001;server_port=1800-1801") and described in separate
 * RTSPTransportFields.
 */
typedef struct RTSPTransportField {
    /** interleave ids, if TCP transport; each TCP/RTSP data packet starts
     * with a '$', stream length and stream ID. If the stream ID is within
     * the range of this interleaved_min-max, then the packet belongs to
     * this stream. */
    int interleaved_min, interleaved_max;

    /** UDP multicast port range; the ports to which we should connect to
     * receive multicast UDP data. */
    int port_min, port_max;

    /** UDP client ports; these should be the local ports of the UDP RTP
     * (and RTCP) sockets over which we receive RTP/RTCP data. */
    int client_port_min, client_port_max;

    /** UDP unicast server port range; the ports to which we should connect
     * to receive unicast UDP RTP/RTCP data. */
    int server_port_min, server_port_max;

    /** time-to-live value (required for multicast); the amount of HOPs that
     * packets will be allowed to make before being discarded. */
    int ttl;

    uint32_t destination; /**< destination IP address */

    /** data/packet transport protocol; e.g. RTP or RDT */
    enum RTSPTransport transport;

    /** network layer transport protocol; e.g. TCP or UDP uni-/multicast */
    enum RTSPLowerTransport lower_transport;
} RTSPTransportField;

/**
 * This describes the server response to each RTSP command.
 */
typedef struct RTSPMessageHeader {
    /** length of the data following this header */
    int content_length;

    enum RTSPStatusCode status_code; /**< response code from server */

    /** number of items in the 'transports' variable below */
    int nb_transports;

    /** Time range of the streams that the server will stream. In
     * AV_TIME_BASE unit, AV_NOPTS_VALUE if not used */
    int64_t range_start, range_end;

    /** describes the complete "Transport:" line of the server in response
     * to a SETUP RTSP command by the client */
    RTSPTransportField transports[RTSP_MAX_TRANSPORTS];

    int seq;                         /**< sequence number */

    /** the "Session:" field. This value is initially set by the server and
     * should be re-transmitted by the client in every RTSP command. */
    char session_id[512];

    /** the "Location:" field. This value is used to handle redirection.
     */
    char location[4096];

    /** the "RealChallenge1:" field from the server */
    char real_challenge[64];

    /** the "Server: field, which can be used to identify some special-case
     * servers that are not 100% standards-compliant. We use this to identify
     * Windows Media Server, which has a value "WMServer/v.e.r.sion", where
     * version is a sequence of digits (e.g. 9.0.0.3372). Helix/Real servers
     * use something like "Helix [..] Server Version v.e.r.sion (platform)
     * (RealServer compatible)" or "RealServer Version v.e.r.sion (platform)",
     * where platform is the output of $uname -msr | sed 's/ /-/g'. */
    char server[64];

    /** The "timeout" comes as part of the server response to the "SETUP"
     * command, in the "Session: <xyz>[;timeout=<value>]" line. It is the
     * time, in seconds, that the server will go without traffic over the
     * RTSP/TCP connection before it closes the connection. To prevent
     * this, sent dummy requests (e.g. OPTIONS) with intervals smaller
     * than this value. */
    int timeout;

    /** The "Notice" or "X-Notice" field value. See
     * http://tools.ietf.org/html/draft-stiemerling-rtsp-announce-00
     * for a complete list of supported values. */
    int notice;
} RTSPMessageHeader;

/**
 * Client state, i.e. whether we are currently receiving data (PLAYING) or
 * setup-but-not-receiving (PAUSED). State can be changed in applications
 * by calling av_read_play/pause().
 */
enum RTSPClientState {
    RTSP_STATE_IDLE,    /**< not initialized */
    RTSP_STATE_STREAMING, /**< initialized and sending/receiving data */
    RTSP_STATE_PAUSED,  /**< initialized, but not receiving data */
    RTSP_STATE_SEEKING, /**< initialized, requesting a seek */
};

/**
 * Identifies particular servers that require special handling, such as
 * standards-incompliant "Transport:" lines in the SETUP request.
 */
enum RTSPServerType {
    RTSP_SERVER_RTP,  /**< Standards-compliant RTP-server */
    RTSP_SERVER_REAL, /**< Realmedia-style server */
    RTSP_SERVER_WMS,  /**< Windows Media server */
    RTSP_SERVER_NB
};

/**
 * Private data for the RTSP demuxer.
 *
 * @todo Use ByteIOContext instead of URLContext
 */
typedef struct RTSPState {
    URLContext *rtsp_hd; /* RTSP TCP connexion handle */

    /** number of items in the 'rtsp_streams' variable */
    int nb_rtsp_streams;

    struct RTSPStream **rtsp_streams; /**< streams in this session */

    /** indicator of whether we are currently receiving data from the
     * server. Basically this isn't more than a simple cache of the
     * last PLAY/PAUSE command sent to the server, to make sure we don't
     * send 2x the same unexpectedly or commands in the wrong state. */
    enum RTSPClientState state;

    /** the seek value requested when calling av_seek_frame(). This value
     * is subsequently used as part of the "Range" parameter when emitting
     * the RTSP PLAY command. If we are currently playing, this command is
     * called instantly. If we are currently paused, this command is called
     * whenever we resume playback. Either way, the value is only used once,
     * see rtsp_read_play() and rtsp_read_seek(). */
    int64_t seek_timestamp;

    /* XXX: currently we use unbuffered input */
    //    ByteIOContext rtsp_gb;

    int seq;                          /**< RTSP command sequence number */

    /** copy of RTSPMessageHeader->session_id, i.e. the server-provided session
     * identifier that the client should re-transmit in each RTSP command */
    char session_id[512];

    /** copy of RTSPMessageHeader->timeout, i.e. the time (in seconds) that
     * the server will go without traffic on the RTSP/TCP line before it
     * closes the connection. */
    int timeout;

    /** timestamp of the last RTSP command that we sent to the RTSP server.
     * This is used to calculate when to send dummy commands to keep the
     * connection alive, in conjunction with timeout. */
    int64_t last_cmd_time;

    /** the negotiated data/packet transport protocol; e.g. RTP or RDT */
    enum RTSPTransport transport;

    /** the negotiated network layer transport protocol; e.g. TCP or UDP
     * uni-/multicast */
    enum RTSPLowerTransport lower_transport;

    /** brand of server that we're talking to; e.g. WMS, REAL or other.
     * Detected based on the value of RTSPMessageHeader->server or the presence
     * of RTSPMessageHeader->real_challenge */
    enum RTSPServerType server_type;

    /** plaintext authorization line (username:password) */
    char auth[128];

    /** authentication state */
    HTTPAuthState auth_state;

    /** The last reply of the server to a RTSP command */
    char last_reply[2048]; /* XXX: allocate ? */

    /** RTSPStream->transport_priv of the last stream that we read a
     * packet from */
    void *cur_transport_priv;

    /** The following are used for Real stream selection */
    //@{
    /** whether we need to send a "SET_PARAMETER Subscribe:" command */
    int need_subscription;

    /** stream setup during the last frame read. This is used to detect if
     * we need to subscribe or unsubscribe to any new streams. */
    enum AVDiscard real_setup_cache[MAX_STREAMS];

    /** the last value of the "SET_PARAMETER Subscribe:" RTSP command.
     * this is used to send the same "Unsubscribe:" if stream setup changed,
     * before sending a new "Subscribe:" command. */
    char last_subscription[1024];
    //@}

    /** The following are used for RTP/ASF streams */
    //@{
    /** ASF demuxer context for the embedded ASF stream from WMS servers */
    AVFormatContext *asf_ctx;

    /** cache for position of the asf demuxer, since we load a new
     * data packet in the bytecontext for each incoming RTSP packet. */
    uint64_t asf_pb_pos;
    //@}

    /** some MS RTSP streams contain a URL in the SDP that we need to use
     * for all subsequent RTSP requests, rather than the input URI; in
     * other cases, this is a copy of AVFormatContext->filename. */
    char control_uri[1024];

    /** The synchronized start time of the output streams. */
    int64_t start_time;
} RTSPState;

/**
 * Describes a single stream, as identified by a single m= line block in the
 * SDP content. In the case of RDT, one RTSPStream can represent multiple
 * AVStreams. In this case, each AVStream in this set has similar content
 * (but different codec/bitrate).
 */
typedef struct RTSPStream {
    URLContext *rtp_handle;   /**< RTP stream handle (if UDP) */
    void *transport_priv; /**< RTP/RDT parse context if input, RTP AVFormatContext if output */

    /** corresponding stream index, if any. -1 if none (MPEG2TS case) */
    int stream_index;

    /** interleave IDs; copies of RTSPTransportField->interleaved_min/max
     * for the selected transport. Only used for TCP. */
    int interleaved_min, interleaved_max;

    char control_url[1024];   /**< url for this stream (from SDP) */

    /** The following are used only in SDP, not RTSP */
    //@{
    int sdp_port;             /**< port (from SDP content) */
    struct in_addr sdp_ip;    /**< IP address (from SDP content) */
    int sdp_ttl;              /**< IP Time-To-Live (from SDP content) */
    int sdp_payload_type;     /**< payload type */
    //@}

    /** rtp payload parsing infos from SDP (i.e. mapping between private
     * payload IDs and media-types (string), so that we can derive what
     * type of payload we're dealing with (and how to parse it). */
    RTPPayloadData rtp_payload_data;

    /** The following are used for dynamic protocols (rtp_*.c/rdt.c) */
    //@{
    /** handler structure */
    RTPDynamicProtocolHandler *dynamic_handler;

    /** private data associated with the dynamic protocol */
    PayloadContext *dynamic_protocol_context;
    //@}
} RTSPStream;

void ff_rtsp_parse_line(RTSPMessageHeader *reply, const char *buf,
                        HTTPAuthState *auth_state);

#if LIBAVFORMAT_VERSION_INT < (53 << 16)
extern int rtsp_default_protocols;
#endif
extern int rtsp_rtp_port_min;
extern int rtsp_rtp_port_max;

/**
 * Send a command to the RTSP server without waiting for the reply.
 *
 * @param s RTSP (de)muxer context
 * @param method the method for the request
 * @param url the target url for the request
 * @param headers extra header lines to include in the request
 * @param send_content if non-null, the data to send as request body content
 * @param send_content_length the length of the send_content data, or 0 if
 *                            send_content is null
 */
void ff_rtsp_send_cmd_with_content_async(AVFormatContext *s,
                                         const char *method, const char *url,
                                         const char *headers,
                                         const unsigned char *send_content,
                                         int send_content_length);
/**
 * Send a command to the RTSP server without waiting for the reply.
 *
 * @see rtsp_send_cmd_with_content_async
 */
void ff_rtsp_send_cmd_async(AVFormatContext *s, const char *method,
                            const char *url, const char *headers);

/**
 * Send a command to the RTSP server and wait for the reply.
 *
 * @param s RTSP (de)muxer context
 * @param method the method for the request
 * @param url the target url for the request
 * @param headers extra header lines to include in the request
 * @param reply pointer where the RTSP message header will be stored
 * @param content_ptr pointer where the RTSP message body, if any, will
 *                    be stored (length is in reply)
 * @param send_content if non-null, the data to send as request body content
 * @param send_content_length the length of the send_content data, or 0 if
 *                            send_content is null
 */
void ff_rtsp_send_cmd_with_content(AVFormatContext *s,
                                   const char *method, const char *url,
                                   const char *headers,
                                   RTSPMessageHeader *reply,
                                   unsigned char **content_ptr,
                                   const unsigned char *send_content,
                                   int send_content_length);

/**
 * Send a command to the RTSP server and wait for the reply.
 *
 * @see rtsp_send_cmd_with_content
 */
void ff_rtsp_send_cmd(AVFormatContext *s, const char *method,
                      const char *url, const char *headers,
                      RTSPMessageHeader *reply, unsigned char **content_ptr);

/**
 * Read a RTSP message from the server, or prepare to read data
 * packets if we're reading data interleaved over the TCP/RTSP
 * connection as well.
 *
 * @param s RTSP (de)muxer context
 * @param reply pointer where the RTSP message header will be stored
 * @param content_ptr pointer where the RTSP message body, if any, will
 *                    be stored (length is in reply)
 * @param return_on_interleaved_data whether the function may return if we
 *                   encounter a data marker ('$'), which precedes data
 *                   packets over interleaved TCP/RTSP connections. If this
 *                   is set, this function will return 1 after encountering
 *                   a '$'. If it is not set, the function will skip any
 *                   data packets (if they are encountered), until a reply
 *                   has been fully parsed. If no more data is available
 *                   without parsing a reply, it will return an error.
 *
 * @return 1 if a data packets is ready to be received, -1 on error,
 *          and 0 on success.
 */
int ff_rtsp_read_reply(AVFormatContext *s, RTSPMessageHeader *reply,
                       unsigned char **content_ptr,
                       int return_on_interleaved_data);

/**
 * Skip a RTP/TCP interleaved packet.
 */
void ff_rtsp_skip_packet(AVFormatContext *s);

/**
 * Connect to the RTSP server and set up the individual media streams.
 * This can be used for both muxers and demuxers.
 *
 * @param s RTSP (de)muxer context
 *
 * @return 0 on success, < 0 on error. Cleans up all allocations done
 *          within the function on error.
 */
int ff_rtsp_connect(AVFormatContext *s);

/**
 * Close and free all streams within the RTSP (de)muxer
 *
 * @param s RTSP (de)muxer context
 */
void ff_rtsp_close_streams(AVFormatContext *s);

#endif /* AVFORMAT_RTSP_H */
