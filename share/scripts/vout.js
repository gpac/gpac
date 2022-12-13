
/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / vout default ui
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

/*very simple controller for vout and vout+aout allowing stats display and playback control */

import * as evg from 'evg'
import { Sys as sys } from 'gpaccore'

_gpac_log_name="";

print("Type 'h' in window for command list");

let aout = null;
let vout = null;
let speed = 1;
let win_id = 0;
let drop=0;
let fullscreen = false;
let paused=0;
let duration=-1;
let codec_slow=false;

let audio_only = false;
let ol_buffer = null;
let ol_canvas = null;
let ol_width = 200;
let ol_height = 20;

let text = new evg.Text();
text.font = 'SERIF';
text.fontsize = 20;
text.baseline = GF_TEXT_BASELINE_HANGING;
text.align = GF_TEXT_ALIGN_LEFT;
text.lineSpacing = 20;

const OL_NONE = 0;
const OL_HELP = 1;
const OL_STATS = 2;
const OL_PLAY = 3;
const OL_AUTH = 4;
let overlay_type = OL_NONE;
let overlay_type_bck = OL_NONE;

let brush = new evg.SolidBrush();
brush.set_color('white');

let disp_size = null;
let task_reschedule = 500;
let rot = 0;
let flip = 0;

let do_coverage=0;
if (sys.get_opt('temp', 'vout_cov') == 'yes') do_coverage = 1;


if (parent_filter && parent_filter.type=='vout')
	vout = parent_filter;

function check_filters()
{
	if (aout && vout) return;
	let i;
	for (i=0; i< session.nb_filters; i++) {
		let f = session.get_filter(i);
		if (!aout && (f.type==="aout")) aout = f;
		else if (!vout && (f.type==="vout")) vout = f;
	}
	if (vout) {
		speed = vout.get_arg('speed');
		drop = vout.get_arg('drop');
		fullscreen = vout.get_arg('fullscreen');
		win_id = vout.get_arg('wid');
	}
}
check_filters();


function setup_overlay()
{
	let target_width=ol_width;
	let target_height=ol_height;

	disp_size = vout.get_arg('owsize');
	if (disp_size==null) return false;
	if (audio_only && !disp_size.x) {
		disp_size = {"x": 320, "y": 80};
	}

	task_reschedule = 500;
	switch (overlay_type) {
	case OL_HELP:
		target_width = Math.floor(disp_size.x/2);
		target_height = Math.floor( (shortcuts.length+2) * 30);
		break;
	case OL_PLAY:
		target_width = Math.floor(disp_size.x/2);
		target_height = Math.floor( 60);
		task_reschedule = 500;
		break;
	case OL_STATS:
		target_width = Math.floor(disp_size.x/2);
		target_height = Math.floor(disp_size.y/2);
		task_reschedule = 250;
		break;
	case OL_AUTH:
		target_width = Math.floor(4*disp_size.x/5);
		target_height = Math.floor(4*disp_size.y/5);
		task_reschedule = 100;
		break;
	default:
		if (!ol_width)
				ol_width = Math.floor(disp_size.x/3);
		if (!ol_height)
			ol_height = Math.floor(disp_size.y/4);
		break;
	}
	init_wnd=true;

	if (audio_only || !vout.nb_ipid) {
		target_width = ol_width = disp_size.x;
		target_height = ol_height = disp_size.y;
	}
	while (target_width%2) target_width--;
	while (target_height%2) target_height--;

	if ((ol_width!=target_width) || (ol_height!=target_height)) {
		ol_buffer = null;
		ol_width = target_width;
		ol_height = target_height;
	}
	if (ol_buffer) return true;
	ol_buffer = new ArrayBuffer(ol_width*ol_height*4);
	ol_canvas = new evg.Canvas(ol_width, ol_height, 'rgba', ol_buffer);
	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);
	return true;
}

session.post_task( () => {
	if (session.last_task) {
		return false;
	}
	check_filters();
	if (!vout) return 1000;
	if (vout.nb_ipid) {
		audio_only=false;
		return false;
	}
	if (aout) {
		if (!aout.nb_ipid || !session.connected) {
			return 100;
		}

		if (!audio_only) {
			overlay_type=OL_PLAY;
			audio_only=true;
			toggle_overlay();
		}
	}
	return false;
}, "check_vout_pids");

let last_x=0, last_y=0;

session.set_event_fun( (evt)=> {
	if (evt.type != GF_FEVT_USER) return 0;

	if ((evt.ui_type == GF_EVENT_CODEC_SLOW) || (evt.ui_type == GF_EVENT_CODEC_OK)) {
		codec_slow = (evt.ui_type == GF_EVENT_CODEC_SLOW) ? true : false;

		if (overlay_type==OL_AUTH) return 0;
		print('trigger overlay');
		ol_visible = false;
		overlay_type=OL_PLAY;
		toggle_overlay();
		update_play();
		return false;
	}

	try {
		if(evt.window != win_id) return true;
	} catch (e) {
		return true;
	}

	switch (evt.ui_type) {
	case GF_EVENT_MOUSEDOWN:
		last_x = evt.mouse_x;
		last_y = evt.mouse_y;
		break;

	case GF_EVENT_MOUSEUP:
		if ((last_x != evt.mouse_x) || (last_y != evt.mouse_y)) return 0;
		if (overlay_type==OL_AUTH) return 0;

		if (overlay_type==OL_PLAY) {
			if (prog_interact(evt)) return 0;
		}
		if (ol_visible) {
			toggle_overlay();
			return true;
		} else if (!audio_only) {
			overlay_type=OL_PLAY;
			toggle_overlay();
			return true;
		}
		break;

	case GF_EVENT_KEYDOWN:
		return process_keyboard(evt);
	case GF_EVENT_TEXTINPUT:
		if (overlay_type==OL_AUTH) {
			if (auth_state==0) {
				if ((evt.char == '\n') || (evt.char == '\r'))
					auth_state = 1;
				else if (evt.char == '\b')
					auth_name = auth_name.slice(0, auth_name.length - 1);
				else
					auth_name += ''+evt.char;
			} else if (auth_state==1) {
				if ((evt.char == '\n') || (evt.char == '\r')) {
					auth_state = 2;
					auth_requests[0].done(auth_name, auth_pass);
					auth_name = '';
					auth_pass = '';
					auth_requests.shift();
					if (auth_requests.length) {
						auth_state = 0;
					} else {
						auth_state = 0;
						toggle_overlay();

						text.font = 'SERIF';
						text.fontsize = 20;
						text.baseline = GF_TEXT_BASELINE_HANGING;
						text.align = GF_TEXT_ALIGN_LEFT;
						text.lineSpacing = 20;
						overlay_type = overlay_type_bck;
						if (overlay_type) toggle_overlay();
					}
				} else if (evt.char == '\b')
					auth_pass = auth_pass.slice(0, auth_pass.length - 1);
				else
					auth_pass += ''+evt.char;
			}
			auth_modified = true;
		}
		return false;

	case GF_EVENT_SIZE:
		if (do_coverage) {
			overlay_type=OL_PLAY;
			ol_visible=true;
			if (do_coverage==1) {
				do_coverage = 0;
				coverage_tests();
			}
		}
		if (ol_visible) {
			let old_type = overlay_type;
			vout.lock(true);
			toggle_overlay();
			overlay_type = old_type;
			toggle_overlay();
			vout.update('oldata', ol_buffer);
			vout.lock(false);
		}
		break;
	default:
	}
	return false;
}
);

function coverage_tests()
{
	print('coverage tests');
	if (aout) aout.update('speed', '1');
	let evt = new FilterEvent(GF_FEVT_QUALITY_SWITCH);
	evt.up = true;
	let src = vout.ipid_source(0);
	session.fire_event(evt, src);
	evt.up = false;
	session.fire_event(evt, src);
}

let interactive_scene=0;
function check_duration()
{
	if (duration!=-1) return (duration>=0) ? true : false;
	let dur=null;
	if (audio_only) {
		dur = aout.ipid_props(0, 'Duration');
		interactive_scene = aout.ipid_props(0, 'InteractiveScene');
	} else if (vout.nb_ipid) {
		dur = vout.ipid_props(0, 'Duration');
		interactive_scene = vout.ipid_props(0, 'InteractiveScene');
	}
	if (interactive_scene==1) {
		duration = 0;
		return;
	}
	if (dur==null) return;
	duration = dur.n;
	duration /= dur.d;
	return true;
}

function do_seek(val, mods, absolute)
{
	if (! check_duration())
		return;

	let seek_to=-1;
	if (!absolute) {
		if (mods & GF_KEY_MOD_CTRL) val *= 30; 
		else if (mods & GF_KEY_MOD_SHIFT) val *= 10;
		seek_to = duration * val / 100;
	}
	//in seconds
	else {
		//30smin
		if (mods & GF_KEY_MOD_CTRL) val *= 1800;
		//10min
		else if (mods & GF_KEY_MOD_SHIFT) val *= 600;
		//30s
		else val *= 30;
		seek_to = val;
	}


	let last_ts_f = vout.last_ts_drop;
	if (last_ts_f==null) return;


	let last_ts = last_ts_f.n;
	last_ts /= last_ts_f.d;
	if (audio_only)
		last_ts -= aout.get_arg('media_offset');
	else
		last_ts -= vout.get_arg('media_offset');
	last_ts += seek_to;	
	if (last_ts<0) last_ts = 0;
	else if (last_ts>duration) last_ts = duration-10;
	
	vout.update('start', ''+last_ts);
	if (aout) aout.update('start', '' + last_ts);
}

let init_wnd=false;
function update_help()
{
	let args = []; //['Shortcuts for vout', '  '];

	shortcuts.forEach( (key) => {
		args.push( '' + sys.keyname(key.code) + ': ' + key.desc);
	});
	if (audio_only)
		text.fontsize = 20;
	else
		text.fontsize = Math.floor(0.6*ol_height/args.length);
	text.set_text(args);

	//lock vout since we will modify data of the canvas
	vout.lock(true);

	if (init_wnd && audio_only) {
		let txtdim = text.measure();

		ol_width = Math.floor(txtdim.width*1.2);
		while (ol_width%2) ol_width--;
		ol_height = Math.floor(txtdim.height*1.2);
		while (ol_height%2) ol_height--;
		ol_buffer = new ArrayBuffer(ol_width*ol_height*4);
		ol_canvas = new evg.Canvas(ol_width, ol_height, 'rgba', ol_buffer);
		vout.update('olsize', ''+ol_width+'x'+ol_height);
		vout.update('olwnd', '0x0x'+ol_width+'x'+ol_height);
		init_wnd=false;
	}

	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);
	let mx = new evg.Matrix2D();
	mx.translate(-ol_width/2, ol_height/3);
	ol_canvas.matrix = mx;
	ol_canvas.path = text;
	ol_canvas.fill(brush);


	overlay_type=OL_NONE;
	vout.update('oldata', ol_buffer);
	vout.lock(false);
}

let progress_bar_path=null;
let prog_ox=0;
let prog_length=0;
let reverse=false;

function prog_interact(evt)
{
	let ret = audio_only ? true : false;
	let x = evt.mouse_x - disp_size.x/2;
	let y = disp_size.y - evt.mouse_y - ol_height/2;

	if (x<-ol_width/2) return ret;
	if (x>ol_width/2) return ret;
	if (y>ol_height/2) return ret;

	if (!check_duration())
		return true;

	if (!progress_bar_path.point_over(x, y)) {
		x -= prog_ox;
		x /= prog_length;
		x += 0.5;
		if (x>1) reverse = !reverse;
		return true;
	}

	x -= prog_ox;
	x /= prog_length;
	x += 0.5;
	x *= duration;
	if (audio_only) {
		aout.update('start', '' + x);
	} else {
		vout.update('start', '' + x);
		if (aout) aout.update('start', '' + x);
	}
	return true;
}

function update_play()
{
	if (init_wnd) {
		let pos = '0x' + Math.floor(ol_height/2 - disp_size.y/2) + 'x'+ol_width+'x'+ol_height;
		vout.update('olwnd', pos);
		init_wnd=false;
		progress_bar_path=null;
	}

	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);
	let length = ol_width - 20;
	let last_ts_f = vout.last_ts_drop;
	if (!last_ts_f && aout)
		last_ts_f = aout.last_ts_drop;
	if (!last_ts_f) {
		return;
	}

	let has_dur = check_duration();
	if (!has_dur) reverse = false;

	let h, m, s;
	let last_ts = last_ts_f.n;
	last_ts /= last_ts_f.d;
	if (audio_only)
		last_ts -= aout.get_arg('media_offset');
	else
		last_ts -= vout.get_arg('media_offset');

	if (reverse) last_ts = duration - last_ts;

	h = Math.floor(last_ts/3600);
	m = Math.floor(last_ts/60 - h*60);
	s = Math.floor(last_ts - h*3600) - m*60;
	let str = reverse ? '-' : ' ';

	if (duration>=3600) {
		if (h<10) str+='0';
		str+=h+':';
	}
	if (m<10) str+='0';
	str+=m+':';
	if (s<10) str+='0';
	str+=s;
	let fs = Math.floor(0.5*ol_height);
	if (fs > 60) fs=60;
	text.fontsize = fs; 
	text.set_text(str);
	let mx = new evg.Matrix2D();
	if (has_dur)
		mx.translate(ol_width/2 - str.length * text.fontsize/2, -10);
	else
		mx.translate(- str.length * text.fontsize/3, -10);
	ol_canvas.matrix = mx;
	ol_canvas.path = text;
	ol_canvas.fill(brush);
	length -= str.length * text.fontsize/2;
	

	if (has_dur) {
		if (!progress_bar_path) {
			prog_length = length;
			prog_ox = 10 + (length - ol_width) / 2;
			progress_bar_path = new evg.Path();
			progress_bar_path.rectangle(prog_ox, 0, prog_length, 20, true);
		}
		ol_canvas.matrix = null;
		ol_canvas.path = progress_bar_path;
		brush.set_color('grey');
		ol_canvas.fill(brush);

		let	progress_path = new evg.Path();
		let progress = last_ts / duration;
		brush.set_color('white');
		progress_path.rectangle(prog_ox - prog_length/2, 9, prog_length*progress, 18, false);
		ol_canvas.path = progress_path;
		ol_canvas.fill(brush);
	}

	if (paused || (speed != 1) || codec_slow) {
		text.fontsize = 16;
		if (codec_slow)
			text.set_text('Codec Too Slow !');
		else if (paused)
			text.set_text((paused==2) ? 'Step mode' : 'Paused');
		else
			text.set_text('Speed: ' + speed);
		let mx = new evg.Matrix2D();
		mx.translate(0, ol_height/4);
		ol_canvas.matrix = mx;
		ol_canvas.path = text;
		brush.set_color(codec_slow ? 'red' : 'cyan');
		ol_canvas.fill(brush);
		brush.set_color('white');
	}
	//we could lock vout to avoid any tearing ...
	vout.update('oldata', ol_buffer);
}

function build_graph(f, str, str_list, fdone)
{
	let i, j, num_tiles=0;
	if (!f.nb_opid) {
		str += '->' + f.name;
		str_list.push(str);
		str = '';
		fdone.push(f);
		return;
	}
	if (fdone.indexOf(f)>=0) {
		str += '->' + f.name;
		str_list.push(str);
		str = '';
		return;
	}

	let len=0;
	if (!str.length) str = f.name;
	else str += '->' + f.name;
	text.set_text(str);
	len = text.measure().width;

	for (i=0; i<f.nb_opid; i++) {
		let sinks = f.opid_sinks(i);
		let sub_len=0;
		let sub_str;

		let p = f.opid_props(i, 'CodecID');
		if (p == 'hvt1') {
			if (num_tiles) continue;
			num_tiles++;
			for (j=i+1; j<f.nb_opid; j++) {
				let p = f.opid_props(j, 'CodecID');
				if (p == 'hvt1')
					num_tiles++;
			}
		}

		if (i) {
			str=' ';
			let k=0;
			while (1) { 
				text.set_text(str);
				let slen = text.measure().width;
				if (slen >= len) break;
				str += ' ';
			}
		}
		sub_str = str;
		if (f.nb_opid>1) {
			if (num_tiles>1) {
				sub_str += '#tilePIDs[' + num_tiles + ']';
			} else {
				sub_str += '#' + f.opid_props(i, 'name');
			}
		}

		if (!sinks.length) {
			str_list.push(sub_str + ': Not connected');
			continue;
		}

		for (j=0; j<sinks.length; j++) {
			let sf = sinks[j];

			if (!j) {
				text.set_text(sub_str);
				sub_len = text.measure().width;
			} else {
				sub_str=' ';
				let k=0;
				while (1) { 
					text.set_text(sub_str);
					let slen = text.measure().width;
					if (slen >= sub_len) break;
					sub_str += ' ';
				}
			}
			build_graph(sinks[j], sub_str, str_list, fdone);
		}
	}
	fdone.push(f);
}

let stats_translate_y=0;
let stats_graph = [];

function update_stats()
{
	let stats = [];

	let sys_info='CPU: '+sys.process_cpu_usage + ' Mem: ' + Math.floor(sys.process_memory/1000000) + ' MB';
	if (session.http_bitrate) {
		let r = session.http_bitrate;
		sys_info += ' HTTP: '; 
		if (r>1000000) sys_info += '' + Math.floor(r/100000) / 10 + ' mbps';
		else if (r>1000) sys_info += '' + Math.floor(r/100) / 10 + ' kbps';
		else sys_info += '' + r + ' bps';
	}
	stats.push(sys_info);
//	stats.push('  ');

	let i;
	for (i=0; i< session.nb_filters; i++) {
	 	let f = session.get_filter(i);
	 	//only output filters (no out, single in) 
	 	if ((f.nb_ipid!=1) || f.nb_opid) continue;

		let str = f.streamtype;
		let src = f.ipid_source(0);
		let names = src.name.split(':');

		str += ' (' + names[0];
		let decrate = src.pck_done;
		if (!decrate) decrate = src.pck_sent + src.pck_ifce_sent;
		decrate /= src.time/1000000;
		decrate = Math.floor(decrate);
		str += ') ' + decrate + ' f/s';
		let p = f.ipid_props(0, 'Width');
		if (p) {
			str += ' ' + p;
			p = f.ipid_props(0, 'Height');
			str += 'x' + p;
			p = f.ipid_props(0, 'FPS');
			if (p) {
				let fps = Math.floor(p.n/p.d);
				if (fps * p.d == p.n)
					str += '@' + fps;
				else
					str += '@' + p.n + '/'+p.d;
			}
			p = f.ipid_props(0, 'SAR');
			if (p) str += ' SAR ' + p.n + '/' + p.d;			
			p = f.ipid_props(0, 'PixelFormat');
			if (p) str += ' pf ' + p;
		} else {
			p = f.ipid_props(0, 'SampleRate');
			if (p) {
				str += ' ' + p;
				p = f.ipid_props(0, 'Channels');
				if (p) str += ' ' + p;
				p = f.ipid_props(0, 'ChannelLayout');
				if (p) str += ' ' + p;
				p = f.ipid_props(0, 'AudioFormat');
				if (p) str += ' fmt ' + p;
			} 
		}

		let buffer = f.ipid_props(0, 'buffer')/1000;
		let rebuf = 0;
		if (audio_only) {
			rebuf = aout.get_arg('rebuffer');			
		}
		else {
			rebuf = vout.get_arg('rebuffer');
		}
		if (rebuf>0) {
			let play_buf = audio_only ? aout.get_arg('buffer') : vout.get_arg('buffer');
			let pc = Math.floor(100 * buffer / play_buf);
			str += ' - rebuffering ' + pc + ' %';
		} else {
			str += ' - buffer ' + Math.floor(f.ipid_props(0, 'buffer')/1000) + ' ms';
		}
		stats.push(str);
	}

//	stats.push(' ');

	//recompute graph only when initializing the window
	if (init_wnd)
		stats_graph = [];

	if (! stats_graph.length) {
		let fdone = [];
		//browse all sources, and build graph
		for (i=0; i< session.nb_filters; i++) {
		 	let f = session.get_filter(i);
		 	//source filter: no input and outputs
		 	if (f.nb_ipid || !f.nb_opid) continue;

		 	build_graph(f, '', stats_graph, fdone);
		}
		let not_done = [];
		for (i=0; i< session.nb_filters; i++) {
		 	let f = session.get_filter(i);
		 	if (f.alias) continue;

		 	if (fdone.indexOf(f)<0)
		 		not_done.push(f);
		 }
		 if (not_done.length) {
			stats.push(' ');
		 	let str = "Filters not connected:";
		 	not_done.forEach(f => { str += ' '+f.name;});
			stats.push(str);
		 }
	 }

	 //push graph to stats string array
	 stats_graph.forEach(str => { stats.push(str);});


	text.fontsize = 20; 
	text.set_text(stats);

	if (init_wnd) {
		let txtdim = text.measure();

		ol_width = Math.floor(txtdim.width*1.2);
		while (ol_width%2) ol_width--;
		ol_height = Math.floor(txtdim.height*1.4);
		while (ol_height%2) ol_height--;
		//we resize / realloc the overlay buffer, lock vout and update it
		vout.lock(true);
		ol_buffer = new ArrayBuffer(ol_width*ol_height*4);
		ol_canvas = new evg.Canvas(ol_width, ol_height, 'rgba', ol_buffer);
		vout.update('olsize', ''+ol_width+'x'+ol_height);
		stats_translate_y = txtdim.height/2;
		vout.update('oldata', ol_buffer);
		vout.lock(false);

		let pos;
		if (audio_only)
			pos = '0x0x'+ol_width+'x'+ol_height;
		else
			pos = ''+ Math.floor(ol_width/2 - disp_size.x/2) + 'x' + Math.floor(ol_height/2 - disp_size.y/2) + 'x'+ol_width+'x'+ol_height;
		vout.update('olwnd', pos);
		init_wnd=false;
	}

	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);

	let mx = new evg.Matrix2D();
	mx.translate(-ol_width/2, stats_translate_y);
	ol_canvas.matrix = mx;
	ol_canvas.path = text;
	ol_canvas.fill(brush);
	//we could lock vout to avoid any tearing ...
	vout.update('oldata', ol_buffer);
}

let auth_name="";
let auth_pass="";
let auth_state=0;
let auth_modified = false;

let auth_requests = [];

function update_auth()
{
	if (init_wnd) {
		let pos = '0x0x'+ol_width+'x'+ol_height;
		vout.update('olwnd', pos);
		init_wnd=false;
		progress_bar_path=null;
		auth_modified = true;

		auth_name = auth_requests[0].user;
		auth_pass = auth_requests[0].pass;
	}
	if (!auth_modified) return false;
	auth_modified = false;

	ol_canvas.clearf(1, 1, 1, 1);
	let str = ['Authentication for ' + auth_requests[0].site];
	str.push(auth_requests[0].secure ? "Secured Connection" : "NOT SECURED Connection");
	if (auth_state) {
		str.push('Enter password');
		let p = '';
		let i = auth_pass.length;
		while (i) {
			p+='*';
			i--;
		}
		str.push(p);
	} else {
		str.push('Enter user name');
		str.push(auth_name);
	}
	let fs = Math.floor(0.1*ol_height);
	if (fs > 60) fs=60;
	text.fontsize = fs;
	text.lineSpacing = 1.5*fs;
	text.align = GF_TEXT_ALIGN_CENTER;
	text.baseline = GF_TEXT_BASELINE_TOP;
	text.set_text(str);

	let txtdim = text.measure();
	let mx = new evg.Matrix2D();
	mx.translate(- txtdim.width/2, -0);

	ol_canvas.matrix = mx;
	ol_canvas.path = text;
	brush.set_color('black');
	ol_canvas.fill(brush);
	brush.set_color('white');


	//we could lock vout to avoid any tearing ...
	vout.update('oldata', ol_buffer);
}

function update_overlay()
{
	if (overlay_type==OL_NONE) return;

	//help screen
	if (overlay_type==OL_HELP) {
		update_help();
		return;
	}
	//play screen
	if (overlay_type==OL_PLAY) {
		update_play();
	}
	//stats screen
	if (overlay_type==OL_STATS) {
		update_stats();
	}
	//auth screen
	if (overlay_type==OL_AUTH) {
		update_auth();
	}
}

let ol_visible=false;
let oltask_scheduled=false;

function toggle_overlay()
{
	ol_visible = !ol_visible;
	if (!ol_visible) {
		//we don't lock vout because the overlay buffer is still valid
		vout.update('oldata', null);
		overlay_type=OL_NONE;
		return;
	}
	//we will potentially destroy the previous overlay bffer, lock vout
	vout.lock(true);
	if (!setup_overlay()) {
		vout.lock(false);
		return;
	}

	vout.update('olwnd', '0x0x'+ol_width+'x'+ol_height);
	vout.update('olsize', ''+ol_width+'x'+ol_height);
	vout.update('oldata', ol_buffer);
	vout.lock(false);
	if (!oltask_scheduled) {
		try {
			session.post_task( () => {
				oltask_scheduled=false;
				if (audio_only && aout.ipid_props(0, 'eos') ) {
					//we don't lock vout because the overlay buffer is still valid
					vout.update('oldata', null);
					return false;
				}
				if (session.last_task) {
					return false;
				}
				update_overlay();
				if (!ol_visible) return false;
				if (overlay_type==OL_NONE) return false;
				oltask_scheduled=true;
				return task_reschedule;
			}, "overlay_update");
			oltask_scheduled = true;
		} catch (e) {}
	}
}

let shortcuts = [
	{ "code": GF_KEY_B, "desc": "speeds up by 2"},
	{ "code": GF_KEY_V, "desc": "speeds down by 2"},
	{ "code": GF_KEY_N, "desc": "normal play speed"},
	{ "code": GF_KEY_SPACE, "desc": "pause/resume"},
	{ "code": GF_KEY_S, "desc": "step one frame"},
	{ "code": GF_KEY_RIGHT, "desc": "seek forward by 1%, 10% if alt down, 30% if ctrl down"},
	{ "code": GF_KEY_LEFT, "desc": "seek backward "},
	{ "code": GF_KEY_UP, "desc": "seek forward by 30s, 10m if alt down, 30m if ctrl down"},
	{ "code": GF_KEY_DOWN, "desc": "seek backward"},
	{ "code": GF_KEY_F, "desc": "fullscreen mode"},
	{ "code": GF_KEY_I, "desc": "show info and statistics"},
	{ "code": GF_KEY_P, "desc": "show playback info"},
	{ "code": GF_KEY_M, "desc": "flip video"},
	{ "code": GF_KEY_R, "desc": "rotate video by 90 degree"},
];

function set_speed(speed)
{
	if (audio_only) aout.update('speed', ''+speed);
	else {
		vout.update('speed', ''+speed);
		if (aout) aout.update('speed', ''+speed);

		if (speed>2) vout.update('drop', '1');
		else if (speed<=1) vout.update('drop', ''+drop);

	}
}

function process_keyboard(evt)
{
	if (interactive_scene==1) return false;

	if (overlay_type==OL_AUTH) {
		return true;
	}
	if(evt.window != win_id) return true;

	switch (evt.keycode) {
	case GF_KEY_B:
		if (evt.keymods & GF_KEY_MOD_SHIFT) speed *= 1.2;
		else speed *= 2;
		set_speed(speed);
		return true;
	case GF_KEY_V:
		if (evt.keymods & GF_KEY_MOD_SHIFT) speed /= 1.2;
		else speed /= 2;
		set_speed(speed);
		return true;
	case GF_KEY_N:
		speed = 1;
		set_speed(speed);
		return true;
	case GF_KEY_SPACE:
		paused = !paused;
		set_speed(paused ? 0 : speed);
		return true;
	case GF_KEY_S:
		paused = 2;
		vout.update('step', '1');
		if (aout) aout.update('speed', '0');
		check_duration();
		return true;
	case GF_KEY_RIGHT:
		if (interactive_scene) return false;
		do_seek(1, evt.keymods, false);
		return true;
	case GF_KEY_LEFT:
		if (interactive_scene) return false;
		do_seek(-1, evt.keymods, false);
		return true;
	case GF_KEY_UP:
		if (interactive_scene) return false;
		do_seek(1, evt.keymods, true);
		return true;
	case GF_KEY_DOWN:
		if (interactive_scene) return false;
		do_seek(-1, evt.keymods, true);
		return true;

	case GF_KEY_F:
		if (audio_only) return;
		fullscreen = !fullscreen;
		vout.update('fullscreen', ''+fullscreen);
		return true;

	case GF_KEY_H:
		if (overlay_type==OL_AUTH) break;
		//hide player
		if (audio_only && (overlay_type==OL_PLAY)) {
			toggle_overlay();
		}
		overlay_type=OL_HELP;
		toggle_overlay();
		//show player player
		if (!ol_visible && audio_only) {
			overlay_type=OL_PLAY;
			toggle_overlay();
		}
		return true;
	case GF_KEY_I:
		if (overlay_type==OL_AUTH) break;
		//hide player
		if (audio_only && (overlay_type==OL_PLAY)) {
			toggle_overlay();
		}
		overlay_type=OL_STATS;
		toggle_overlay();
		//show player player
		if (!ol_visible && audio_only) {
			overlay_type=OL_PLAY;
			toggle_overlay();
		}
		return true;
	case GF_KEY_P:
		if (overlay_type==OL_AUTH) break;
		//do not untoggle for audio only
		if (audio_only) return;
		overlay_type=OL_PLAY;
		if (!ol_visible) init_wnd=true;
		toggle_overlay();
		return true;
	case GF_KEY_R:
		if (audio_only) return;
		rot = vout.get_arg('vrot');
		rot++;
		if (rot==4) rot = 0;
		if (!rot) vout.update('vrot', '0');
		else if (rot==1) vout.update('vrot', '90');
		else if (rot==2) vout.update('vrot', '180');
		else vout.update('vrot', '270');
		return true;
	case GF_KEY_M:
		if (audio_only) return;
		flip = vout.get_arg('vflip');
		flip++;
		if (flip==4) flip = 0;
		if (!flip) vout.update('vflip', 'no');
		else if (flip==1) vout.update('vflip', 'v');
		else if (flip==2) vout.update('vflip', 'h');
		else vout.update('vflip', 'vh');
		return;
	default:
		break;
	}
	return false;
}

session.set_auth_fun( (site, user, pass, secure, auth_cbk) => {
	overlay_type_bck = overlay_type;
	overlay_type = OL_AUTH;
	auth_cbk.site = site;
	auth_cbk.user = user ? user : "";
	auth_cbk.pass = pass ? pass : "";
	auth_cbk.secure = secure;
	auth_requests.push(auth_cbk);

	if (overlay_type_bck) toggle_overlay();
	toggle_overlay();
});

