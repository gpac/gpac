/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020
 *					All rights reserved
 *
 *  This file is part of GPAC / AVGenerator filter
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

import * as evg from 'evg'
import { Sys as sys } from 'gpaccore'

filter.pids = [];

filter.set_name("avgen");
filter.set_desc("AV Counter Generator");
filter.set_version("1.0");
filter.set_author("GPAC Team");
filter.set_help(
"This filter generates AV streams representing a counter. Streams can be enabled or disabled using [-type]().\n"
+"The filter is software-based and does not use GPU.\n"
+"\n"
+"When [-adjust]() is set, the first video frame is adjusted such that a full circle happens at each exact second according to the system UTC clock.\n"
+"By default, video UTC and date are computed at each frame generation from current clock and not from frame number.\n"
+"This will result in broken timing when playing at speeds other than 1.0.\n"
+"This can be changed using [-lock]().\n"
+"\n"
+"Audio beep is generated every second, with octave (2xfreq) of even beep used every 10 seconds.\n"
+"When video is generated, beep is synchronized to video at each exact second.\n"
+"\n"
+"If NTP injection is used, each video packet (but not audio ones) has a `SenderNTP` property set; if video is not used, each audio packet has a `SenderNTP` property set.\n"
+"\n"
+"# Multistream generation\n"
+"More than one output size can be specified. This will result in multiple sources being generated, one per size.\n"
+"A size can be specified more than once, resulting in packet references when [-copy]() is not set, or full copies otherwise.\n"
+"Target encoding bitrates can be assigned to each output using [-rates](). This can be useful when generating dash:\n"
+"EX gpac avgen:sizes=1280x720,1920x1080:rates=2M,5M c=aac:FID=1 c=264:FID=2:clone -o live.mpd:SID=1,2\n"
+"\n"
+"# Multiview generation\n"
+"In multiview mode, only the animated counter will move in depth backward and forward, as indicated by the [-disparity]() value.\n"
+"When [-pack]() is set, a packed stereo couple is generated for each video packet.\n"
+"Otherwise, when [-views]() is greater than 2, each view is generated on a dedicated output PID with the property `ViewIdx` set in [1, views].\n"
+"Multi-view output forces usage of [-copy]() mode.\n"
+"\n"
+"# PID Naming\n"
+"The audio PID is assigned the name `audio` and ID `1`.\n"
+"If a single video PID is produced, it is assigned the name `video` and ID `2`.\n"
+"If multiple video PIDs are produced, they are assigned the names `videoN` and ID `N+1`, N in [1, sizes].\n"
+"If multiple [-views]() are generated, they are assigned the names `videoN_vK` and ID `N*views+K-1`, N in [1, sizes], K in [1, views].\n"
);

filter.set_arg({ name: "type", desc: "output selection\n- a: audio only\n- v: video only\n- av: audio and video", type: GF_PROP_UINT, def: "av", minmax_enum: "a|v|av"} );
filter.set_arg({ name: "freq", desc: "frequency of beep", type: GF_PROP_UINT, def: "440"} );
filter.set_arg({ name: "freq2", desc: "frequency of odd beep", type: GF_PROP_UINT, def: "659"} );
filter.set_arg({ name: "sr", desc: "output samplerate", type: GF_PROP_UINT, def: "44100"} );
filter.set_arg({ name: "flen", desc: "output frame length in samples", type: GF_PROP_UINT, def: "1024"} );
filter.set_arg({ name: "ch", desc: "number of channels", type: GF_PROP_UINT, def: "1"} );
filter.set_arg({ name: "alter", desc: "beep alternatively on each channel", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "blen", desc: "length of beep in milliseconds", type: GF_PROP_UINT, def: "50"} );
filter.set_arg({ name: "fps", desc: "video frame rate", type: GF_PROP_FRACTION, def: "25"} );
filter.set_arg({ name: "sizes", desc: "video size in pixels", type: GF_PROP_VEC2I_LIST, def: "1280x720"} );
filter.set_arg({ name: "pfmt", desc: "output pixel format", type: GF_PROP_PIXFMT, def: "yuv"} );
filter.set_arg({ name: "lock", desc: "lock timing to video generation", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "dyn", desc: "move bottom banner", type: GF_PROP_BOOL, def: "true"} );
filter.set_arg({ name: "ntp", desc: "send NTP along with packets", type: GF_PROP_BOOL, def: "true"} );
filter.set_arg({ name: "copy", desc: "copy the framebuffer into each video packet instead of using packet references", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "dur", desc: "run for the given time in second", type: GF_PROP_FRACTION, def: "0/0"} );
filter.set_arg({ name: "adjust", desc: "adjust start time to synchronize counter and UTC", type: GF_PROP_BOOL, def: "true"} );
filter.set_arg({ name: "pack", desc: "packing mode for stereo views\n - no: no packing\n - ss: side by side packing, forces [-views]() to 2\n - tb: top-bottom packing, forces [-views]() to 2", type: GF_PROP_UINT, def: "no", minmax_enum: "no|ss|tb"} );
filter.set_arg({ name: "disparity", desc: "disparity in pixels between left-most and right-most views", type: GF_PROP_UINT, def: "20"} );
filter.set_arg({ name: "views", desc: "number of views", type: GF_PROP_UINT, def: "1"} );
filter.set_arg({ name: "rates", desc: "number of target bitrates to assign, one per size", type: GF_PROP_STRING_LIST} );

let audio_osize=0;
let audio_cts=0;
let audio_pid = null;
let audio_pos=0;
let nb_secs=0;
let audio_odd=0;
let audio_10s=1;
let audio_beep_len=0;
let audio_playing=false;
let audio_in_beep=false;
let audio_beep_ch=0;

let videos = [];
let video_cts=0;
let video_frame=0;

let brush = new evg.SolidBrush();
let video_playing=false;
let start_date = 0;
let banner = 'many thanks to QuickJS, FreeType, OpenSSL, SDL, FFmpeg, OpenHEVC, libjpeg, libpng, faad2, libmad, a52dec, xvid, OGG ...';
let frame_offset = 0;
let nb_frame_init = 0;
let utc_init = 0;
let ntp_init = 0;

/*create a text*/
let text = new evg.Text();
text.font = 'SANS';
text.fontsize = 20;
text.baseline = GF_TEXT_BASELINE_HANGING;
text.align=GF_TEXT_ALIGN_CENTER;
text.lineSpacing=0;

filter.frame_pending = 0;

filter.initialize = function() {

	if (filter.type != 1) {
		this.set_cap({id: "StreamType", value: "Audio", output: true} );	
	}
	if (filter.type != 0) {
		this.set_cap({id: "StreamType", value: "Video", output: true} );
	}
	this.set_cap({id: "CodecID", value: "raw", output: true} );

	//setup audio
	if (filter.type != 1) {
		audio_pid = this.new_pid();
		audio_pid.set_prop('StreamType', 'Audio');
		audio_pid.set_prop('CodecID', 'raw');
		audio_pid.set_prop('SampleRate', filter.sr);
		audio_pid.set_prop('NumChannels', filter.ch);
		audio_pid.set_prop('Timescale', filter.sr);
		audio_pid.set_prop('AudioFormat', 'flt');
		audio_pid.set_prop('Cached', true);
		audio_pid.name = "audio";
		audio_pid.set_prop('ID', 1);
		if (!filter.freq)
			filter.freq = 440;

		if (!filter.freq2)
			filter.freq2 = filter.freq;

		audio_osize = 4 * filter.flen * filter.ch;
		audio_beep_len = filter.sr * filter.blen / 1000;
	}

	if (!filter.sizes || !filter.sizes.length)
		filter.type = 0;

	//setup video
	if (filter.type != 0) {
		let assign_rates = false;
		//single view output if packed modes
		if (filter.pack) filter.views = 1;
		else if (!filter.views) filter.views = 1;

		if (filter.rates && filter.rates.length) {
			if (filter.rates.length == filter.sizes.length) {
				assign_rates = true;
			} else {
				print(3, "Number of rates differ from number of video sizes, not assigning rates");
			}
		}

		for (let vid=0; vid<filter.sizes.length; vid++) {
			//setup one source
			let vsrc = {};
			vsrc.canvas=null;
			vsrc.video_buffer=null;
			vsrc.video_tcard_height=0;
			vsrc.video_pids = [];
			vsrc.forward_srcs = [];
			vsrc.init_banner_done = false;
			vsrc.w = filter.sizes[vid].x;
			vsrc.h = filter.sizes[vid].y;
			vsrc.is_forward = false;

			for (let prev_vid=0; prev_vid<videos.length; prev_vid++) {
				let a_vid = videos[prev_vid];
				if ((a_vid.w == vsrc.w) && (a_vid.h == vsrc.h)) {
					a_vid.forward_srcs.push(vsrc);
					vsrc.is_forward = true;
				}
			}
			videos.push(vsrc);
			for (let view=0; view<filter.views; view++) {
				let vpid = this.new_pid();
				vpid.set_prop('StreamType', 'Video');
				vpid.set_prop('CodecID', 'raw');
				vpid.set_prop('FPS', filter.fps);
				vpid.set_prop('Timescale', filter.fps.n);
				vpid.set_prop('PixelFormat', filter.pfmt);
				vpid.set_prop('Width', vsrc.w);
				vpid.set_prop('Stride', vsrc.w);
				vpid.set_prop('Height', vsrc.h);
				vpid.set_prop('Cached', true);

				if (assign_rates) {
					vpid.set_prop('TargetRate', filter.rates[vid]);
				}

				let name = "video";
				if (filter.sizes.length>1)
					name += "" + (vid+1)*filter.views;

				if (filter.views>1) {
					vpid.name = name+"_v" + (view+1);
					vpid.set_prop('ViewIdx', (view+1));
					filter.copy = true;
				} else {
					vpid.name = name;
				}
				vpid.set_prop('ID', 1 + (vid+1)*filter.views + view);
				vsrc.video_pids.push(vpid);
			}

			if (vsrc.is_forward) continue;

			let osize = sys.pixfmt_size(filter.pfmt, vsrc.w, vsrc.h);
			vsrc.video_buffer = new ArrayBuffer(osize);
			vsrc.canvas = new evg.Canvas(vsrc.w, vsrc.h, filter.pfmt, vsrc.video_buffer);
			vsrc.canvas.centered = true;
			//clear to black and write our static content
			vsrc.canvas.clearf('black');
			vsrc.banner_fontsize = vsrc.h / 36;
			vsrc.txt_fontsize = 20;

			let tx_card = new evg.Texture("testcard.png", true);
			let tx_gpac = new evg.Texture("$GSHARE/res/gpac_highres.png", false);
			tx_card.repeat_s = true;
			tx_card.repeat_t = true;
			tx_gpac.repeat_s = true;
			tx_gpac.repeat_t = true;

			put_image(vsrc, tx_card, true, true);
			put_image(vsrc, tx_gpac, false, true);
			if (filter.pack) {
				put_image(vsrc, tx_card, true, false);
				put_image(vsrc, tx_gpac, false, false);
			}
			text.align=GF_TEXT_ALIGN_LEFT;
		}
	}
}

function put_image(vsrc, tx, is_testcard, is_first)
{

	let disp_w = vsrc.w;
	let disp_h = vsrc.h;
	let t_x = 0;
	let t_y = 0;
	if (filter.pack==1) {
		if (is_first) {
			t_x = -disp_w/2;
		} else {
			t_x = disp_w/2;
		}
	}
	else if (filter.pack==2) {
		if (is_first) {
			t_y = disp_h/2;
		} else {
			t_y = -disp_h/2;
		}
	}

	let path = new evg.Path();
	let scale;
	if (is_testcard) {
		scale = disp_w/2 / tx.width;
	} else {		
		scale = disp_w/6 / tx.width;
	}
	let rw = scale * tx.width;
	let rh = scale * tx.height;

	let mmx = new evg.Matrix2D();
	if (filter.pack==1)
		mmx.scale(0.5, 1);
	else if (filter.pack==2)
		mmx.scale(1, 0.5);
	vsrc.canvas.matrix = mmx;
	mmx.scale(scale, scale);
	let ox, oy;
	//testcard in top-left quarter of image
	if (is_testcard) {
		ox = t_x - disp_w/2;
		oy = t_y + disp_h/2;
		vsrc.video_tcard_height = rh;
		if (filter.pack==2)
			vsrc.video_tcard_height /= 2;
	}
	//logo & text in top-right quarter
	else {
		ox = t_x + disp_w/2 - rw;
		oy = t_y + disp_h/2;
	}
	path.rectangle(ox, oy, rw, rh);
	mmx.identity = true;
	if (filter.pack==1)
		mmx.scale(0.5, 1);
	else if (filter.pack==2)
		mmx.scale(1, 0.5);
	mmx.scale(scale, scale);

	if (filter.pack==2)
		mmx.translate(ox, oy/2);
	else
		mmx.translate(ox, oy);

	vsrc.canvas.path = path;
	//put a gradient behind gpac transparent logo
	if (!is_testcard) {
		if (filter.pack) {
			brush.set_color('white');
			vsrc.canvas.fill(brush);
		} else {
			let ch = 0.8;
			let grad = new evg.RadialGradient();
			grad.set_points(ox+rw/2, oy-rh/2, ox+0.46*rw, oy-0.44*rh, rw/2, rh/2);
			grad.set_stopf(0.0, ch, ch, ch, 0.0);
			grad.set_stopf(0.5, ch, ch, ch, 1.0);
			grad.set_stopf(0.65, 1.0, ch, ch, 1.0);
			grad.set_stopf(0.8, ch, ch, ch, 1.0);
			grad.set_stopf(1.0, 0.0, 0.0, 0.0, 0.0);
			grad.mode = GF_GRADIENT_MODE_SPREAD;
			vsrc.canvas.fill(grad);
		}
	}
	tx.mx = mmx;
	tx.auto_mx=false;
	vsrc.canvas.fill(tx);

	if (is_testcard) return;

	/*create a text*/
	text.fontsize = rh/9;
	text.align=GF_TEXT_ALIGN_CENTER;
	let vprop = '' + vsrc.w + 'x' + vsrc.h + '@';
	let fps = filter.fps.n / filter.fps.d;
	let ifps = Math.floor(fps);
	if (ifps==fps) fps = ifps;
	else fps = Math.floor(100*fps) / 100;

	vprop += '' + fps + ' FPS';
	text.set_text(['GPAC AV Generator', 'v'+sys.version_full, ' ',  'UTC Locked: ' + (filter.lock ? 'yes' : 'no'), ' ', vprop]);

	mmx.identity = true;
	mmx.translate(t_x+10, oy-rh/5);
	if (filter.pack==1)
		mmx.scale(0.5, 1);
	else if (filter.pack==2)
		mmx.scale(1, 0.5);
	vsrc.canvas.matrix = mmx;
	vsrc.canvas.path = text;
	brush.set_color('white');
	vsrc.canvas.fill(brush);

	vsrc.txt_fontsize = rh/7;
}

filter.process_event = function(pid, evt)
{
	if (evt.type == GF_FEVT_STOP) {
		if (pid === audio_pid) audio_playing = false;
		else video_playing = false;
	} 
	else if (evt.type == GF_FEVT_PLAY) {
		if (pid === audio_pid) audio_playing = true;
		else video_playing = true;
		filter.reschedule();
	} 
}

filter.process = function()
{
	if (!audio_playing && !video_playing) return GF_EOS;

	//start by processing video, adjusting start time
	if (video_playing) 
		process_video();

	if (audio_playing)
		process_audio();
	return GF_OK;
}

function process_audio()
{
	if (!audio_pid || audio_pid.would_block)
		return;

	let pck = audio_pid.new_packet(audio_osize);

	let farray = new Float32Array(pck.data);
	for (let i=0; i<filter.flen; i++) {
		let idx;
		let samp = 0;
		let cur_pos = audio_pos - nb_secs*filter.sr; 
		if (cur_pos < 0) {}
		else if (cur_pos > audio_beep_len) {
			if (audio_in_beep) {
				audio_beep_ch ++;
				if (audio_beep_ch==filter.ch) audio_beep_ch = 0;
			}
			audio_in_beep = false;
		}
		else {
			audio_in_beep = true;
			if (audio_odd) {
				samp = Math.sin( audio_pos * 2 * Math.PI * filter.freq2 / filter.sr);
			} else if (audio_10s) {
				samp = Math.sin( audio_pos * 2 * Math.PI * 2 * filter.freq / filter.sr);
			} else {
				samp = Math.sin( audio_pos * 2 * Math.PI * filter.freq / filter.sr);
			}
			//triangle amplitude
			let scaler = 0.5 - cur_pos / audio_beep_len;
			if (scaler>=0) scaler = (0.5 - scaler) * 2;
			else scaler = (scaler+0.5) * 2;
			samp *= scaler;
		}
		idx = filter.ch * i;
		for (let k=0; k<filter.ch; k++) {
			if (filter.alter && (audio_beep_ch!=k)) {
				farray[idx + k] = 0;
			} else {
				farray[idx + k] = samp;
			}
		}
		audio_pos++;
		if (cur_pos >= filter.sr) {
			nb_secs++;
			audio_odd = (nb_secs % 2);
			audio_10s = ! (nb_secs % 10);
		}
	}

	/*set packet properties and send it*/
	pck.cts = audio_cts;
	pck.dur = filter.flen;
	pck.sap = GF_FILTER_SAP_1;
	
	//when prop value is set to true for 'SenderNTP', automatically set 
	if (!videos.length && filter.ntp)
		pck.set_prop('SenderNTP', true);
	pck.send();
	audio_cts += filter.flen;

	if (filter.dur.d) {
		if (audio_cts * filter.dur.d >= filter.sr * filter.dur.n) {
			audio_playing = false;
			audio_pid.eos = true;
		}
	}
}

function process_video()
{
	if (!filter.copy) {
		if (filter.frame_pending)
			return;
	}

	let date = new Date();
	let utc, ntp;
	//remember start date for lock, and compute initial offset in cycles so that we reach tull cycle at each second
	if (!start_date) {
		start_date = date.getTime(); 
		let ms_init = date.getMilliseconds();
		if (filter.adjust) {
			frame_offset = filter.fps.n * ms_init / 1000;

			//in audio samples
			audio_pos = Math.floor(ms_init * filter.sr / 1000);
	
			//move to nb frames
			nb_frame_init = Math.floor(frame_offset / filter.fps.d);
			frame_offset = filter.fps.d * nb_frame_init;
		}
		utc_init = sys.get_utc();
		ntp_init = sys.get_ntp();
		utc = utc_init;
		ntp = ntp_init;
	}
	else if (filter.lock) {
		let elapsed_us = (1000000 * video_cts) / filter.fps.n;
		let elapsed_ms = Math.floor(elapsed_us / 1000);
		date.setTime(start_date + elapsed_ms);
		utc = utc_init + elapsed_ms;
		ntp = sys.ntp_shift(ntp_init, elapsed_us);
	} else {
		utc = sys.get_utc();
		ntp = sys.get_ntp();
	}

	let time = (video_cts + frame_offset) / filter.fps.n;
	let sec = Math.floor(time);
	let col_idx = sec % 2;
	let cycle_time = time;
	while (cycle_time>=1) cycle_time -= 1;


	let ms = time - sec;
	let h = Math.floor(sec / 3600);
	let m = Math.floor(sec / 60 - h*60);
	let s = Math.floor(sec - h*3600 - m * 60);
	//adjusted nb frames, reset cycle time
	let nbf = Math.floor( ((video_cts + frame_offset) - sec * filter.fps.n) / filter.fps.d );
	if (!nbf) cycle_time = 0;

	let col = (!cycle_time && !(sec % 10)) ? 'slateblue' : 'black';

	for (let vid=0; vid<videos.length; vid++) {
		let vsrc = videos[vid];
		if (vsrc.is_forward) continue;

		for (let i=0; i<filter.views; i++) {
			let vpid = vsrc.video_pids[i];
			if (filter.copy && vpid.would_block)
				continue;

			let nb_pack_views = filter.pack ? 2 : 1;
			for (let j=0; j<nb_pack_views; j++) {
				draw_view(vsrc, i+j, col, col_idx, cycle_time, h, m, s, nbf, video_frame, date, utc, ntp);
			}

			if (!vsrc.init_banner_done) {
				vsrc.init_banner_done = true;
			}
		
			let forward_idx = 0;
			let a_src = vsrc;
			while (a_src) {
				let pck = null;
				if (filter.copy) {
					pck = vpid.new_packet(vsrc.video_buffer);
					filter.frame_pending = 0;
				} else {
					pck = vpid.new_packet(vsrc.video_buffer, true,  () => { filter.frame_pending--; } );
					filter.frame_pending ++;
				}
				/*set packet properties and send it*/
				pck.cts = video_cts;
				pck.dur = filter.fps.d;
				pck.sap = GF_FILTER_SAP_1;
				if (filter.ntp)
					pck.set_prop('SenderNTP', ntp);
				pck.send();
				a_src = null;
				if (forward_idx < vsrc.forward_srcs.length) {
					a_src = vsrc.forward_srcs[forward_idx];
					forward_idx++;
					vpid = a_src.video_pids[i];
				} else {
					break;
				}
			}
		}
	}

	video_cts += filter.fps.d;
	video_frame++;

	if (filter.dur.d && (video_cts * filter.dur.d >= filter.fps.n * filter.dur.n)) {
		print("done playing, cts " + video_cts);
		video_playing = false;
		for (let vid=0; vid<videos.length; vid++) {
			let vsrc = videos[vid];
			for (let i=0; i<filter.views; i++) {
				vsrc.video_pids[i].eos = true;
			}
		}
	}
}

function draw_view(vsrc, view_idx, col, col_idx, cycle_time, h, m, s, nbf, video_frame, date, utc, ntp)
{
	let disp_w = vsrc.w;
	let disp_h = vsrc.h;
	let t_x = 0;
	let t_y = 0;
	let r = disp_h/4;
	if (filter.pack==1) {
		disp_w /= 2;
		if (!view_idx) {
			t_x = -disp_w/2;
		} else {
			t_x = disp_w/2;
		}
	}
	else if (filter.pack==2) {
		disp_h /= 2;
		if (!view_idx) {
			t_y = disp_h/2;
		} else {
			t_y = -disp_h/2;
		}
	}


	let clear_crop = filter.dyn ? 0 : 3*vsrc.banner_fontsize;
	if (view_idx && !filter.pack)
		clear_crop = 3*vsrc.banner_fontsize;

	let remain_y = disp_h - vsrc.video_tcard_height;
	//clear bottom-left
	if (!view_idx || filter.pack)
		vsrc.canvas.clearf({x: t_x -disp_w/2-2,y: t_y - remain_y/2, w: disp_w/2+4, h: remain_y - clear_crop}, 'black');
	//clear circle
	vsrc.canvas.clearf({x: t_x + 1, y: t_y + disp_h/5, w:disp_w/2, h: 7*disp_h/10 - clear_crop}, col);

	let cx = t_x + disp_w/4;
	let cy = t_y - disp_h/4 + disp_h/5;


	let disparity = 0;
	if (filter.pack || (filter.views>1)) {
		let num_views = filter.pack ? 2 : filter.views;
		//disparity of first view
		let d = (cycle_time-0.5) * filter.disparity;
		if (col_idx) {
			d *= -1;
		}
		let dstep = -2*d / (num_views-1);
		disparity = d + view_idx * dstep;
	}

	let mx = new evg.Matrix2D();
	if (filter.pack==1)
		mx.scale(0.5, 1);
	else if (filter.pack==2)
		mx.scale(1, 0.5);
	mx.translate(cx + disparity, cy);
	vsrc.canvas.matrix = mx;

	let path = new evg.Path();

	if (col_idx) {
		brush.set_color('white');
	} else {		
		brush.set_color('grey');
	}
	path.ellipse(0, 0, 2*r, 2*r);
	vsrc.canvas.path = path;
	vsrc.canvas.fill(brush);
	
	if (cycle_time) {
		path.reset();
		let start = Math.PI/2 - cycle_time * 2 * Math.PI;
		path.arc(r/2, start, Math.PI/2, 2);
	} else {
		col_idx = !col_idx;		
	}

	if (col_idx) {
		brush.set_color('grey');
	} else {		
		brush.set_color('white');
	}
	vsrc.canvas.path = path;
	vsrc.canvas.fill(brush);

	text.align=GF_TEXT_ALIGN_LEFT;
	text.fontsize = vsrc.txt_fontsize;

	let t = 'Time: ';
	if (h<10) t = t+'0'+h;
	else t = t+''+h;
	
	if (m<10) t = t+':0'+m;
	else t = t + ':' + m;

	if (s<10) t = t+':0'+s;
	else t = t+':'+s;

	if (nbf<10) t = t+'/0'+nbf;
	else t = t+'/'+nbf;

	text.set_text(['Frame: '+(video_frame+nb_frame_init), t]);

	if (filter.pack || (filter.views>1)) {
		mx.identity = true;
		mx.translate(-r/2, -1.2*r);
		if (filter.pack==1)
			mx.scale(0.5, 1);
		else if (filter.pack==2)
			mx.scale(1, 0.5);
		mx.translate(cx, cy);
	} else {
		mx.translate(-r, -1.2*r);
	}


	vsrc.canvas.matrix = mx;
	vsrc.canvas.path = text;
	brush.set_color('white');
	vsrc.canvas.fill(brush);

	//non packed, Nth view: no changes
	if (view_idx && !filter.pack)
		return;

	text.set_text([' Date: ' + date.toUTCString(), ' Local: ' + date], ' UTC (ms): ' + utc, ' NTP (s.f):  ' + ntp.n + '.' + ntp.d.toString(16) );

	mx.identity = true;
	if (filter.pack==1)
		mx.scale(0.5, 1);
	else if (filter.pack==2)
		mx.scale(1, 0.5);
	mx.translate(t_x -disp_w/2, t_y - disp_h/4);
	vsrc.canvas.matrix = mx;
	vsrc.canvas.path = text;
	brush.set_color('white');
	vsrc.canvas.fill(brush);

	if (!filter.dyn && vsrc.init_banner_done) 
		return;

	if (filter.pack==1)
		vsrc.canvas.clipper = {x:t_x - disp_w/2,y:0, w: disp_w, h: filter.h };

	let pos_x = (video_frame+nb_frame_init) % (16 * filter.fps.n);
	pos_x /= 10*filter.fps.n;
	pos_x = - (pos_x + 0.25) * disp_w;
	if (!filter.dyn) {
		pos_x = pos_x - 2*disp_w/5;
		text.align=GF_TEXT_ALIGN_CENTER;
	}
	text.fontsize = vsrc.banner_fontsize;
	text.bold = true;
	text.italic = true;
	mx.identity = true;
	if (filter.pack==1)
		mx.scale(0.5, 1);
	else if (filter.pack==2)
		mx.scale(1, 0.5);

	if (!filter.dyn) {
		text.set_text([sys.copyright, banner]);
		mx.translate(0, text.fontsize/2);
	} else {
		text.set_text([sys.copyright + ' - ' + banner]);
	}
	mx.translate(t_x + pos_x, t_y -disp_h/2 + text.fontsize);

	vsrc.canvas.matrix = mx;
	vsrc.canvas.path = text;
	brush.set_color('red');
	vsrc.canvas.fill(brush);
	brush.set_color('white');
	mx.translate(-1, 1);
	vsrc.canvas.matrix = mx;
	vsrc.canvas.fill(brush);
	text.bold = false;
	text.italic = false;
	text.fontsize = vsrc.txt_fontsize;

	vsrc.canvas.clipper = null;
}

