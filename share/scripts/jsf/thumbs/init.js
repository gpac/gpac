import * as evg from 'evg'
import { Sys as sys } from 'gpaccore'
import { File as File } from 'gpaccore'

//metadata
filter.set_name("thumbs");
filter.set_desc("Thumbnail collection generator");
filter.set_version("1.0");
filter.set_author("GPAC team");
filter.set_help(`This filter generates screenshots from a video stream.

The input video is down-sampled by the [-scale]() factor. The output size is configured based on the number of images per line and per column in the [-grid](). 
Once configured, the output size is no longer modified.

The [-snap]() option indicates to use one video frame every given seconds. If value is 0, all input frames are used.

If the number of rows is 0, it will be computed based on the source duration and desired [-snap]() time, and will default to 10 if it cannot be resolved.

To output one image per input frame, use \`:grid=1x1\`.

If a single image per output frame is used, the default value for [-snap]() is 0 and for [-scale]() is 1.
Otherwise, the default value for [-snap]() is 1 second and for [-scale]() is 10.

A single line of text can be inserted over each frame. Predefined keywords can be used in input text, identified as \`$KEYWORD$\`:
- ts: replaced by packet timestamp
- timescale: replaced by PID timescale
- time: replaced by packet time as HH:MM:SS.ms
- cpu: replaced by current CPU usage of process
- mem: replaced by current memory usage of process
- version: replaced by GPAC version
- fversion: replaced by GPAC full version
- mae: replaced by Mean Absolute Error with previous frame
- mse: replaced by Mean Square Error with previous frame
- P4CC, PropName: replaced by corresponding PID property

EX gpac -i src reframer:saps=1 thumbs:snap=30:grid=6x30 -o dump/$num$.png
This will generate images from key-frames only, inserting one image every 30 seconds. Using key-frame filtering is much faster but may give unexpected results if there are not enough key-frames in the source.

EX gpac -i src thumbs:snap=0:grid=5x5 -o dump/$num$.png
This will generate one image containing 25 frames every second at 25 fps.


If a single image per output frame is used and the scaling factor is 1, the input packet is reused as input with text and graphics overlaid.

EX gpac -i src thumbs:grid=1x1:txt='Frame $time$' -o dump/$num$.png
This will inject text over each frame and keep timing and other packet properties.

A json output can be specified in input [-list]() to let applications retrieve frame position in output image from its timing.

# Scene change detection

The filter can compute the absolute and/or square error metrics between consecutive images and drop image if the computed metric is less than the given threshold.
If both [-mae]() and [-mse]() thresholds are 0, scene detection is not performed (default).
If both [-mae]() and [-mse]() thresholds are not 0, the frame is added if it passes both thresholds.

For both metrics, a value of 0 means all pixels are the same, a value of 100 means all pixels have 100% intensity difference (e.g. black versus white).

The scene detection is performed after the [-snap]() filtering and uses:
- the previous frame in the stream, whether it was added or not, if [-scref]() is not set,
- the last added frame otherwise.

Typical thresholds for scene cut detection are 14 to 20 for [-mae]() and 5 to 7 for [-mse]().

Since this is a costly process, it is recommended to use it combined with key-frames selection:

EX gpac -i src reframer:saps=1 thumbs:mae=15 -o dump/$num$.png

The [-maxsnap]() option can be used to force insertion after the given time if no scene cut is found.

`);

filter.set_arg({ name: "grid", desc: "number of images per lines and columns", type: GF_PROP_VEC2I, def: "6x0"} );
filter.set_arg({ name: "scale", desc: "scale factor for input size", type: GF_PROP_DOUBLE, def: "-1"} );
filter.set_arg({ name: "mae", desc: "scene diff threshold using Mean Absolute Error", type: GF_PROP_UINT, def: "0", hint:"advanced", minmax_enum: "0,100"} );
filter.set_arg({ name: "mse", desc: "scene diff threshold using Mean Square Error", type: GF_PROP_UINT, def: "0", hint:"advanced", minmax_enum: "0,100"} );
filter.set_arg({ name: "lw", desc: "line width between images in pixels", type: GF_PROP_DOUBLE, def: "0.0", hint:"advanced"} );
filter.set_arg({ name: "lc", desc: "line color", type: GF_PROP_STRING, def: "white", hint:"advanced"} );
filter.set_arg({ name: "clear", desc: "clear color", type: GF_PROP_STRING, def: "white", hint:"advanced"} );
filter.set_arg({ name: "snap", desc: "duration between images, 0 for all images", type: GF_PROP_DOUBLE, def: "-1"} );
filter.set_arg({ name: "maxsnap", desc: "maximum duration between two thumbnails when scene change detection is enabled", type: GF_PROP_DOUBLE, def: "-1"} );
filter.set_arg({ name: "pfmt", desc: "output pixel format", type: GF_PROP_PIXFMT, def: "rgb", hint:"expert"} );
filter.set_arg({ name: "txt", desc: "text to insert per thumbnail", type: GF_PROP_STRING, def: "", hint:"advanced"} );
filter.set_arg({ name: "tc", desc: "text color", type: GF_PROP_STRING, def: "white", hint:"expert"} );
filter.set_arg({ name: "tb", desc: "text shadow", type: GF_PROP_STRING, def: "black", hint:"expert"} );
filter.set_arg({ name: "font", desc: "font to use", type: GF_PROP_STRING, def: "SANS", hint:"expert"} );
filter.set_arg({ name: "fs", desc: "font size to use in percent of scaled height", type: GF_PROP_DOUBLE, def: "10", hint:"expert"} );
filter.set_arg({ name: "tv", desc: "text vertical position in percent of scaled height", type: GF_PROP_DOUBLE, def: "0", hint:"expert"} );
filter.set_arg({ name: "thread", desc: "number of threads for software rasterizer, -1 for all available cores", type: GF_PROP_SINT, def: "-1", hint:"expert"} );
filter.set_arg({ name: "blt", desc: "use blit instead of software rasterizer", type: GF_PROP_BOOL, def: "true", hint:"expert"} );
filter.set_arg({ name: "scref", desc: "use last inserted image as reference for scene change detection", type: GF_PROP_BOOL, def: "false", hint:"expert"} );
filter.set_arg({ name: "dropfirst", desc: "drop first image", type: GF_PROP_BOOL, def: "false", hint:"expert"} );
filter.set_arg({ name: "list", desc: "export json list of frame times and positions to given file", type: GF_PROP_STRING, def: null, hint:"expert"} );
filter.set_arg({ name: "lxy", desc: "add explicit x and y in json export", type: GF_PROP_BOOL, def: "false", hint:"expert"} );


filter.initialize = function() {

	if ((this.grid.x==1) && (this.grid.y==1)) {
		if (this.scale==-1) this.scale = 1;
		if (this.snap==-1) this.snap = 0;
	} else {
		if (this.scale==-1) this.scale = 10;
		if (this.snap==-1) this.snap = 1;	
	}

	if (this.scale<=0) {
		print(GF_LOG_ERROR, 'Invalid scale factor, must be greater than 0');
		return GF_BAD_PARAM;
	}
	if (this.grid.x<=0) {
		print(GF_LOG_ERROR, 'Invalid tile grid, must be at least 1x1');
		return GF_BAD_PARAM;
	}
	this.set_cap({id: "StreamType", value: "Video", inout: true} );
	this.set_cap({id: "CodecID", value: "raw", inout: true} );

	let test_canvas=null;
	try {
		test_canvas = new evg.Canvas(48, 48, filter.pfmt);
	} catch (e) {
		print(GF_LOG_WARNING, 'Pixel format ' + filter.pfmt + ' not supported by rasterizer, defaulting to rgb');
		filter.pfmt = 'rgb';
	}

	if (this.txt.length) use_text = true;

	let mode=0;
	if (this.mae > 0) mode = (this.mse>0) ? 3 : 1;
	else if (this.mse>0) mode = 2;

	if (this.txt && (this.txt.indexOf('mae')>=0)) {
		if (!mode) mode=1;
		else if (mode==2) mode = 3;
	}
	if (this.txt && (this.txt.indexOf('mse')>=0)) {
		if (!mode) mode=2;
		else if (mode==1) mode = 3;
	}
	if (mode==3) sd_mode="mae,mse";
	else if (mode==2) sd_mode="mse";
	else if (mode==1) sd_mode="mae";

}

let use_text=false;
let nb_tiles=0;
let max_tiles;
let opck=null;
let canvas=null;
let opid=null;
let ipid=null;
let timescale=0;
let nb_oframe=0;
let last_dur=0;
let init_ts=0;
let path=null;
let outline=null;
let tot_tiles=0;
let swidth=0, sheight=0, owidth, oheight, osize;
let can_blit;

let mx = new evg.Matrix2D();
let line_brush = new evg.SolidBrush();
let reuse_src=false;
let reuse_pfmt=false;
let tx_matrix=null;
let sd_mode=null;
let delay=0;
let out_list = null;


filter.configure_pid = function(pid) 
{
	if (!opid)
		opid = this.new_pid();
	ipid = pid;

	//only configure once
	if (swidth || sheight) {
		if (pid.timescale != timescale) {
			init_ts = 0;
			timescale = pid.timescale;
			if (reuse_src) opid.copy_props(ipid);
		}
		return GF_OK;
	}
	swidth = pid.get_prop('Width');
	sheight = pid.get_prop('Height');
	if (!swidth || !sheight) return GF_OK;

	timescale = pid.timescale;
	swidth = Math.floor(swidth/this.scale);
	if (swidth%2) swidth++;
	sheight = Math.floor(sheight/this.scale);
	if (sheight%2) sheight++;

	pid.mirror = pid.get_prop('Mirror');
	pid.rotate = pid.get_prop('Rotate');
	delay = pid.get_prop('Delay');

	if (this.grid.y<=0) {
		if (this.snap>0) {
			let dur = pid.get_prop('Duration');
			if (dur && dur.n && dur.d) {
				let nb_img = Math.ceil(dur.n / dur.d / this.snap / this.grid.x);
				if (nb_img) {
					print('Using ' + nb_img + ' rows');
					this.grid.y = nb_img;
				}
			}
		}
		if (!this.grid.y) {
			if (this.snap>0) {
				print(GF_LOG_WARNING, 'Cannot determine video duration, using 10 as default output rows');
			}
			this.grid.y = 10;
		}
	}

	can_blit = true;
  if ((pid.rotate==1) || (pid.rotate==3)) {
    let tmp = swidth;
    swidth = sheight;
    sheight = tmp;
  }
  if (pid.rotate || pid.mirror) can_blit=false;

	owidth = swidth * this.grid.x;
	oheight = sheight * this.grid.y;
	max_tiles = this.grid.x * this.grid.y;

	if ((max_tiles==1) && (filter.scale==1) && can_blit) {
		reuse_src=true;
		opid.copy_props(ipid);
		swidth = pid.get_prop('Width');
		sheight = pid.get_prop('Height');
		reuse_pfmt = pid.get_prop('PixelFormat');
	} else {
		opid.reset_props();
		opid.set_prop('Width', owidth);
		opid.set_prop('Height', oheight);
		opid.set_prop('StreamType', 'Video');
		opid.set_prop('CodecID', 'raw');
		opid.set_prop('PixelFormat', this.pfmt);
		opid.set_prop('Timescale', timescale);
	}

	if (this.list) {
		out_list = {};
		out_list.timescale = timescale;
		out_list.grid_width = this.grid.x;
		out_list.grid_height = this.grid.y;
		out_list.thumb_width = swidth;
		out_list.thumb_height = sheight;
		out_list.images = [];
	}

	osize = sys.pixfmt_size(this.pfmt, owidth, oheight);
	canvas = null;
	path = new evg.Path();
	path.rectangle(0, 0, swidth, sheight, true);

	if (this.lw) {
		outline = path.outline({width: this.lw, align: GF_PATH_LINE_CENTER, join: GF_LINE_JOIN_BEVEL});
	}
  build_tx_matrix();

}


function flush_frame()
{
	tot_tiles+=nb_tiles;
	nb_tiles = 0;
	opck.send();
	nb_oframe++;
	canvas=null;
	opck=null;
}

let prev_tx = null;
let prev_pck = null;
let frame_mae = 0;
let frame_mse = 0;
let last_thumb_ts=0;

filter.process = function()
{
	let ipck = ipid.get_packet();
	if (!ipck) {
		if (ipid.eos) {
			if (nb_tiles) {
				flush_frame();
				print(reuse_src ? GF_LOG_DEBUG : GF_LOG_INFO, 'generated ' + nb_oframe + ' images for ' + tot_tiles + ' tiles');
			}
			if (prev_pck) {
				prev_pck.unref();
				prev_pck=null;
			}
			opid.eos = true;
			if (out_list.images.length && this.list) {
				let out = new File(this.list, "w");
				out.puts(JSON.stringify(out_list));
				out.close();
				out_list.images.length=[];
			}
			return GF_EOS;
		}
		return GF_OK;
	}

	if (this.snap>0) {
		if (!init_ts) {
			last_thumb_ts = init_ts = ipck.cts+1;
			if (this.dropfirst) {
				ipid.drop_packet();
				return GF_OK;	  		
			}
		} else {
			let time = ipck.cts - init_ts+1;
			if (time < this.snap * timescale) {
				if (sd_mode && !this.scref && 0) {
					prev_pck = ipck.ref();
					prev_tx = new evg.Texture(ipck);
				}
				ipid.drop_packet();
				return GF_OK;
			}
			init_ts = ipck.cts+1;
		}
	} 

	let tx = new evg.Texture(ipck);
	if (sd_mode) {
		let forward = false;
		if (!prev_pck) {
			forward = true;
			frame_mae = 0;
			frame_mse = 0;
		} else {
			let diff_score = tx.diff_score(prev_tx, sd_mode);
			let mae = diff_score[0].mae;
			let mse = diff_score[0].mse;

			if (filter.mae>0) {
				if (filter.mse>0) {
					if ((mae >= filter.mae) && (mse >= filter.mse))
						forward = true;
				} else {
					if (mae >= filter.mae)
						forward = true;
				}
			}
			else if (filter.mse>0) {
				if (mse >= filter.mse)
					forward = true;
			}
			//mae and mse disabled but we run them for stats
			else {
				forward = true;
			}

			if (!forward && (this.maxsnap>0) && last_thumb_ts) {
				let diff = ipck.cts - last_thumb_ts - 1;
				if (diff >= this.maxsnap*timescale) {
					forward = true;
				}
			}
			if (!forward) {
				if (!this.scref) {
					if (prev_pck) prev_pck.unref();
					prev_pck = ipck.ref();
					prev_tx = tx;
				}
				ipid.drop_packet();
				return GF_OK;
			}
			if (prev_pck) prev_pck.unref();
			frame_mae = mae;
			frame_mse = mse;
			last_thumb_ts = ipck.cts+1;
		}
		prev_pck = ipck.ref();
		prev_tx = tx;
	}

	if (reuse_src) {
		//create a clone of the input data, requesting inplace processing 
		opck = opid.new_packet(ipck, false, false);

		try {
			/*create a canvas for our output data if first time, otherwise reassign internal canvas data*/
			if (!canvas) {
				canvas = new evg.Canvas(swidth, sheight, reuse_pfmt, opck.data);
			} else {		
				canvas.reassign(opck.data);
			}
		} catch (e) {
			print(GF_LOG_ERROR, 'Failed to create canvas from packet: ' + e);
			canvas = null;
			return GF_SERVICE_ERROR;
		}
		print(GF_LOG_DEBUG, 'Created create canvas from input packet');
	}

	/*create a canvas for our output data if first time, otherwise reassign internal canvas data*/
	if (!canvas) {
		opck = opid.new_packet(osize);
		opck.cts = ipck.cts;
		try {
			canvas = new evg.Canvas(owidth, oheight, filter.pfmt, opck.data);
		} catch (e) {
			print(GF_LOG_ERROR, 'Failed to create canvas from packet: ' + e + ' - try changing output pixel format');
			canvas = null;
			return GF_SERVICE_ERROR;	
		}
		if (filter.thread) {
			canvas.enable_threading(filter.thread);
		}
		canvas.clear(this.clear);
	}

	mx.identity = true;
	let off_x = nb_tiles % this.grid.x;
	let off_y = (nb_tiles-off_x) / this.grid.x;
	off_x *= swidth;
	off_y *= sheight;
	mx.translate(-owidth/2 + off_x + swidth/2, oheight/2 - off_y - sheight/2);

	if (!reuse_src) {
		let do_soft=true;
		if (filter.blt && can_blit) {
			try {
				let dst_rc = {x: off_x, y: off_y, w: swidth, h: sheight};
				canvas.blit(tx, dst_rc, null);
				do_soft=false;
			} catch (e) {
				print(GF_LOG_WARNING, 'blit failed: ' + e);
			}
		}
		if (do_soft) {
			tx.mx = tx_matrix;
			canvas.path = path;
			canvas.matrix = mx;
			canvas.fill(tx);
		}
	}

	if (outline) {  	
		line_brush.set_color(this.lc);
		canvas.path = outline;
		canvas.matrix = mx;
		canvas.fill(line_brush);
	}

	if (use_text) {
		let txt = new evg.Text();
		txt.font = filter.font;
		txt.fontsize = filter.fs * sheight/100;
		let diff = (sheight - 2*txt.fontsize) * filter.tv / 100;
		mx.translate(0, -sheight/2 + txt.fontsize/2 + diff);

		txt.baseline = GF_TEXT_BASELINE_ALPHABETIC;
		txt.align = GF_TEXT_ALIGN_LEFT;
		txt.maxWidth = swidth;

		set_text(txt, ipck);

		let tsize = txt.measure();
		mx.translate(-tsize.width/2, 0);

		canvas.path = txt;
		canvas.matrix = mx;

		line_brush.set_color(filter.tb);
		canvas.fill(line_brush);
		line_brush.set_color(filter.tc);
		mx.translate(-1.25, 1.25);
		canvas.matrix = mx;
		canvas.fill(line_brush);
	}

	if (this.list) {
		let it = { frame: nb_tiles+1, page: nb_oframe+1, ts: (ipck.cts+delay)};
		if (this.lxy) {
			it.x = off_x;
			it.y = off_y;
		}
		out_list.images.push(it);
	}

	/*send packet*/
	ipid.drop_packet();

	nb_tiles++;
	print(reuse_src ? GF_LOG_DEBUG : GF_LOG_INFO, 'processing tile '+nb_tiles+'/'+max_tiles+ ' (' + nb_oframe + ' images sent)     \r');

	if (nb_tiles==max_tiles) {
		flush_frame();
	}
	return GF_OK;
}


function build_tx_matrix()
{
	tx_matrix = null;

	if (!ipid.rotate && !ipid.mirror) return;

	let mirrored=0;
	let rotated=0;
	let scale_tx_sw = 1;
	let scale_tx_sh = 1;
	let trans_tx_x = 0;
	let trans_tx_y = 0;
	let mxflip = new evg.Matrix2D();
	if (ipid.mirror) {
		mirrored=ipid.mirror;
		if (mirrored==1) mxflip.scale(-1, 1);
		else if (mirrored==2) mxflip.scale(1, -1);
		else if (mirrored==3) {
			mxflip.scale(-1, 1);
			mxflip.scale(1, -1);
		}
	}

	if (ipid.rotate) {
		rotated=ipid.rotate;
		if (rotated==3) mxflip.rotate(0, 0, -Math.PI/2);
		else if (rotated==2) mxflip.rotate(0, 0, Math.PI);
		else if (rotated==1) mxflip.rotate(0, 0, Math.PI/2);
	}

	if ((rotated==1) || (rotated==3)) {
		scale_tx_sw = swidth / sheight;
		scale_tx_sh = sheight / swidth;
	}
	let txmx = new evg.Matrix2D();
	txmx.scale(scale_tx_sw, scale_tx_sh);
	txmx.add(mxflip);

	if (rotated==0) {
		if (mirrored==2) txmx.translate(0, 1);
		else if (mirrored==3) txmx.translate(1, 1);
		else if (mirrored==1) txmx.translate(1, 0);
		else txmx.translate(0, 0);
	}
	else if (rotated==1) {
		if (mirrored==2) txmx.translate(0, 0);
		else if (mirrored==3) txmx.translate(0, 1);
		else if (mirrored==1) txmx.translate(1, 1);
		else txmx.translate(1, 0);
	}
	else if (rotated==2) {
		if (mirrored==2) txmx.translate(1, 0);
		else if (mirrored==3) txmx.translate(0, 0);
		else if (mirrored==1) txmx.translate(0, 1);
		else txmx.translate(1, 1);
	}
	else { // (rotated==3) 
		if (mirrored==2) txmx.translate(1, 1);
		else if (mirrored==3) txmx.translate(1, 0);
		else if (mirrored==1) txmx.translate(0, 0);
		else txmx.translate(0, 1);
	}
	tx_matrix = txmx;
}	


function set_text(txt, pck)
{
	let line = filter.txt;

	if (line.indexOf('$') < 0) {
		txt.set_text(line);
		return;
	}
	let first_is_sep = (line.charAt(0)=='$') ? true : false; 
	let last_is_sep = (line.charAt(line.length-1)=='$') ? true : false; 
	let split_text = line.split('$');
	let len = split_text.length;
	let new_line='';

	split_text.forEach( (item, index) => {
		if (!index && !first_is_sep) {
			new_line += item;
			return;
		}
		if (!last_is_sep && (index==len-1)) {
			new_line += item;
			return;
		}
		let timeout=0;
		let kword = item.toLowerCase();
		if (kword=='ts') {
			item = '' + pck.cts;
		}
		else if (kword=='timescale') {
			item = '' + pid.timescale;
		}
		else if (kword=='time') {
			let time = pck.cts * 1000 / timescale;
			let time_s = Math.floor(time/1000);
			let h = Math.floor(time_s/3600);
			let m = Math.floor(time_s/60 - h*60);
			let s = Math.floor((time_s - h*3600 - m*60));
			item = ((h<10) ? '0' : '') + h + ((m<10) ? ':0' : ':') + m + ((s<10) ? ':0' : ':') + s + '.';
			time =  Math.floor(time - time_s*1000);
			if (time < 10) item += '00';
			else if (time < 100) item += '0';
			item += time;
		}
		else if (kword=='cpu') {
			item = '' + sys.process_cpu_usage;
		}
		else if (kword=='mem') {
			item = '' + Math.floor(sys.process_memory/1000000) + ' MB';
		}
		else if (kword=='version') {
			item = sys.version;
		}
		else if (kword=='fversion') {
			item = sys.version_full;
		}
		else if (kword=='mae') {
			item = '' + Math.floor(frame_mae*1000)/1000;
		}
		else if (kword=='mse') {
			item = '' + Math.floor(frame_mse*1000)/1000;
		}
		else {
			try {
				let p = ipid.get_prop(item);
				if (!p) p = ipid.get_info(item);
				if (typeof p != 'undefined') {
					if (p==null) item = 'n/a';
					else item = '' + p;
				}
			} catch (e) {}
		}
		new_line += item;
	});

	txt.set_text(new_line);      
}

