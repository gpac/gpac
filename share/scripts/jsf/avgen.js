import * as evg from 'evg'
import { Sys as sys } from 'gpaccore'

filter.pids = [];

filter.set_name("avgen");
filter.set_desc("AV Counter Generator");
filter.set_version("1.0");
filter.set_author("GPAC Team");
filter.set_help(
"This filter generates AV streams representing a counter. Streams can be enabled or disabled using [-type]().\n"
+"\n"
+"First video frame is adjusted such that a full circle happens at each exact second according to the system UTC clock.\n"
+"By default, video UTC and date are computed at each frame generation from current clock and not from frame number.\n"
+"This will result in broken timing when playing at speeds other than 1.0.\n"
+"This can be changed using [-lock]().\n"
+"\n"
+"Audio beep is generated every second, with octave (2xfreq) of even beep used every 10 seconds.\n"
+"When video is generated, beep is synchronized to video at each exact second.\n"
+"\n"
+"Video packets always have a `SenderNTP` property injected.\n"
+"If video is not used, audio packets always have a `SenderNTP` property injected.\n"
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
filter.set_arg({ name: "w", desc: "video width", type: GF_PROP_UINT, def: "1280"} );
filter.set_arg({ name: "h", desc: "video height", type: GF_PROP_UINT, def: "720"} );
filter.set_arg({ name: "lock", desc: "lock timing to video generation", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "dyn", desc: "move bottom banner", type: GF_PROP_BOOL, def: "true"} );
filter.set_arg({ name: "ntp", desc: "send NTP along with packets", type: GF_PROP_BOOL, def: "true"} );

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

let video_pid = null;
let canvas=null;
let video_buffer=null;
let video_cts=0;
let video_osize=0;
let video_frame=0;
let testcard_tx = null;
let video_tcard_bottom=0;
let video_tcard_height=0;
let brush = new evg.SolidBrush();
let video_playing=false;
let start_date = 0;
let banner = 'many thanks to QuickJS, FreeType, OpenSSL, SDL, FFmpeg, OpenHEVC, libjpeg, libpng, faad2, libmad, a52dec, xvid, OGG ...';
let init_banner_done = false;
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

let txt_fontsize = 0;
let banner_fontsize = 0;

filter.frame_pending = false;

filter.initialize = function() {

	if (filter.type != 1) {
		this.set_cap({id: "StreamType", value: "Audio", output: true} );	
	}
	if (filter.type != 0) {
		this.set_cap({id: "StreamType", value: "Video", output: true} );
	}
	this.set_cap({id: "CodecID", value: "raw", ouput: true} );

	//setup audio
	if (filter.type != 1) {
		audio_pid = this.new_pid();
		audio_pid.set_prop('StreamType', 'Audio');
		audio_pid.set_prop('CodecID', 'raw');
		audio_pid.set_prop('SampleRate', filter.sr);
		audio_pid.set_prop('NumChannels', filter.ch);
		audio_pid.set_prop('Timescale', filter.sr);
		audio_pid.set_prop('AudioFormat', 'flt');
		audio_pid.set_prop('BufferLength', 0);
		audio_pid.set_prop('ReBuffer', 0);
		audio_pid.name = "audio";
		if (!filter.freq)
			filter.freq = 440;

		if (!filter.freq2)
			filter.freq2 = filter.freq;

		audio_osize = 4 * filter.flen * filter.ch;
		audio_beep_len = filter.sr * filter.blen / 1000;
	}

	//setup audio
	if (filter.type != 0) {
		video_pid = this.new_pid();
		video_pid.set_prop('StreamType', 'Video');
		video_pid.set_prop('CodecID', 'raw');
		video_pid.set_prop('FPS', filter.fps);
		video_pid.set_prop('Timescale', filter.fps.n);
		video_pid.set_prop('PixelFormat', 'yuv');
		video_pid.set_prop('Width', filter.w);
		video_pid.set_prop('Stride', filter.w);
		video_pid.set_prop('Height', filter.h);
		video_pid.set_prop('BufferLength', 0);
		video_pid.set_prop('ReBuffer', 0);

		video_pid.name = "video";

		video_osize = filter.w * filter.h * 3 / 2;
		video_buffer = new ArrayBuffer(video_osize);
		canvas = new evg.Canvas(filter.w, filter.h, 'yuv', video_buffer);
		canvas.centered = true;
		//clear to black and write our static content
		canvas.clearf('black');
		put_image(canvas, true);
		put_image(canvas, false);
		banner_fontsize = filter.h / 36;
	}
}

function put_image(canvas, is_testcard)
{
	let tx = null;
	if (is_testcard) {
		tx = new evg.Texture("testcard.png", true);
	} else {		
		tx = new evg.Texture("$GSHARE/res/gpac_highres.png", false);
	}
	tx.repeat_s = true;
	tx.repeat_t = true;

	let path = new evg.Path();
	let scale;
	if (is_testcard) {
		scale = filter.w/2 / tx.width;
	} else {		
		scale = filter.w/6 / tx.width;
	}
	let rw = scale * tx.width;
	let rh = scale * tx.height;
	let mmx = new evg.Matrix2D();
	mmx.scale(scale, scale);
	let ox, oy;
	//testcard in top-left quarter of image
	if (is_testcard) {
		ox = -filter.w/2;
		oy = filter.h/2;
		video_tcard_bottom = oy - rh;
		video_tcard_height = rh;
	}
	//logo & text in top-right quarter
	else {
		ox = filter.w/2 - rw;
		oy = filter.h/2;
	}
	path.rectangle(ox, oy, rw, rh);
	mmx.translate(ox, oy);
	canvas.path = path;
	//put a gradient behind gpac transparent logo
	if (!is_testcard) {
		let ch = 0.8;
		let grad = new evg.RadialGradient();
		grad.set_points(ox+rw/2, oy-rh/2, ox+0.46*rw, oy-0.44*rh, rw/2, rh/2);
		grad.set_stopf(0.0, ch, ch, ch, 0.0);
		grad.set_stopf(0.5, ch, ch, ch, 1.0);
		grad.set_stopf(0.65, 1.0, ch, ch, 1.0);
		grad.set_stopf(0.8, ch, ch, ch, 1.0);
		grad.set_stopf(1.0, 0.0, 0.0, 0.0, 0.0);
		grad.mode = GF_GRADIENT_MODE_STREAD;
		canvas.fill(grad);
	}
	tx.mx = mmx;
	canvas.fill(tx);

	if (is_testcard) return;

	/*create a text*/
	text.fontsize = rh/9;
	let vprop = '' + filter.w + 'x' + filter.h + '@';
	let fps = filter.fps.n / filter.fps.d;
	let ifps = Math.floor(fps);
	if (ifps==fps) fps = ifps;
	else fps = Math.floor(100*fps) / 100;

	vprop += '' + fps + ' FPS';
	text.set_text(['GPAC AV Generator', 'v'+sys.version_full, ' ',  'UTC Locked: ' + (filter.lock ? 'yes' : 'no'), ' ', vprop]);

	mmx = new evg.Matrix2D();
	mmx.translate(10, oy-rh/5);
	canvas.matrix = mmx;
	canvas.path = text;
	brush.set_color('white');
	canvas.fill(brush);

	text.align=GF_TEXT_ALIGN_LEFT;
	txt_fontsize = text.fontsize = rh/7;
}

filter.process_event = function(pid, evt)
{
	if (evt.type == GF_FEVT_STOP) {
		if (pid === audio_pid) audio_playing = false;
		else if (pid === video_pid) video_playing = false;
	} 
	else if (evt.type == GF_FEVT_PLAY) {
		if (pid === audio_pid) audio_playing = true;
		else if (pid === video_pid) video_playing = true;
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
	pck.sap = GF_FILTER_SAP_1;
	
	//when prop value is set to true for 'SenderNTP', automatically set 
	if (!video_pid && filter.ntp)
		pck.set_prop('SenderNTP', true);
	pck.send();
	audio_cts += filter.flen;
}

function process_video()
{
	if (!video_pid || filter.frame_pending)
		return;

	let date = new Date();
	let utc, ntp;
	//remember start date for lock, and compute initial offset in cycles so that we reach tull cycle at each second
	if (!start_date) {
		start_date = date.getTime(); 
		let ms_init = date.getMilliseconds();
		frame_offset = filter.fps.n * ms_init / 1000;

		//in audio samples
		audio_pos = Math.floor(ms_init * filter.sr / 1000);

		//move to nb frames
		nb_frame_init = Math.floor(frame_offset / filter.fps.d);
		frame_offset = filter.fps.d * nb_frame_init;
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

	let clear_crop = filter.dyn ? 0 : 3*banner_fontsize;
	canvas.clearf({x:-filter.w/2-2,y:video_tcard_bottom,w:filter.w/2+4,h: filter.h - video_tcard_height - clear_crop}, 'black');
	let col = (!cycle_time && !(sec % 10)) ? 'slateblue' : 'black';
	canvas.clearf({x:1,y:filter.h/5, w:filter.w/2, h: 7*filter.h/10 - clear_crop}, col);

	let cx = filter.w/4;
	let cy = -filter.h/4 + filter.h/5;
	let r = filter.h/4;

	let mx = new evg.Matrix2D();
	mx.translate(cx, cy);
	canvas.matrix = mx;

	let path = new evg.Path();

	if (col_idx) {
		brush.set_color('white');
	} else {		
		brush.set_color('grey');
	}
	path.ellipse(0, 0, 2*r, 2*r);
	canvas.path = path;
	canvas.fill(brush);
	
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
	canvas.path = path;
	canvas.fill(brush);

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

	mx.translate(-r, -1.2*r);
	canvas.matrix = mx;
	canvas.path = text;
	brush.set_color('white');
	canvas.fill(brush);


	text.set_text([' Date: ' + date.toUTCString(), ' Local: ' + date], ' UTC (ms): ' + utc, ' NTP (s.f):  ' + ntp.n + '.' + ntp.d.toString(16) );

	mx.identity = true;
	mx.translate(-filter.w/2, -filter.h/4);
	canvas.matrix = mx;
	canvas.path = text;
	brush.set_color('white');
	canvas.fill(brush);

	if (filter.dyn || !init_banner_done) {
		let pos_x = (video_frame+nb_frame_init) % (16 * filter.fps.n);
		pos_x /= 10*filter.fps.n;
		pos_x = - (pos_x + 0.25) * filter.w;
		if (!filter.dyn) {
			pos_x = -2*filter.w/5;
			text.align=GF_TEXT_ALIGN_CENTER;
		}
		text.fontsize = banner_fontsize;
		text.bold = true;
		text.italic = true;
		mx.identity = true;
		if (!filter.dyn) {
			text.set_text([sys.copyright, banner]);
			mx.translate(0, text.fontsize/2);
		} else {
			text.set_text([sys.copyright + ' - ' + banner]);
		}
		mx.translate(pos_x, -filter.h/2 + text.fontsize);
		canvas.matrix = mx;
		canvas.path = text;
		brush.set_color('red');
		canvas.fill(brush);
		brush.set_color('white');
		mx.translate(-1, 1);
		canvas.matrix = mx;
		canvas.fill(brush);
		text.bold = false;
		text.italic = false;
		text.fontsize = txt_fontsize;
		if (!init_banner_done) {
			text.align=GF_TEXT_ALIGN_LEFT;
			init_banner_done = true;
		}
	}

	let pck = video_pid.new_packet(video_buffer, true,  () => { filter.frame_pending=false; } );
	filter.frame_pending = true;
	/*set packet properties and send it*/
	pck.cts = video_cts;
	pck.sap = GF_FILTER_SAP_1;
	if (filter.ntp)
		pck.set_prop('SenderNTP', ntp);
	pck.send();
	video_cts += filter.fps.d;
	video_frame++;
}



