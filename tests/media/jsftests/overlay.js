import * as evg from 'evg'

//metadata
filter.set_name("Graphics Burning Test !");
filter.set_desc("JS-based vector graphics generator");
filter.set_version("0.1beta");
filter.set_author("GPAC team");
filter.set_help("This filter provides testing of gpac's vector graphics bindings");

let path = new evg.Path();
path.ellipse(0, 0, 400, 200);
path.zero_fill=true;

let brush = new evg.SolidBrush();
brush.set_color('cyan');

let text = new evg.Text();
text.font = 'Times';
text.align=GF_TEXT_ALIGN_RIGHT;
text.lineSpacing=0;
text.text = ['GPAC Overlay demo', 'Bliting on YUV !' ];

let centered=true;

let mx = new evg.Matrix2D();
//mx.translate(0, 200);

let mx1 = new evg.Matrix2D();
mx1.translate(0, -200);

//let tx = new evg.Texture("/Volumes/DATA/src.png", true);
let tx = new evg.Texture("../auxiliary_files/logo.png", true);
tx.repeat_s = true;
tx.repeat_t = true;

//let tx_e = tx.conv( {w:3, h:3, norm:12, k: [2, 1, 2, 1, 0, 1, 2, 1, 2] } );
//let tx_e = tx.conv( {w:3, h:3, norm:16, k: [1, 2, 1, 2, 4, 2, 1, 2, 1] } );
//let tx_e = tx.conv( {w:3, h:3, norm:0, k: [1, 0, -1, 0, 0, 0, -1, 0, 1] } );
//let tx_e = tx.conv( {w:3, h:3, norm:0, k: [0, 1, 0, 1, -4, 1, 0, 1, 0] } );
//let tx_e = tx.conv( {w:3, h:3, norm:0, k: [-1, 0, 1, -2, 0, 2, -1, 0, 1] } );
//let tx_e = tx.conv( {w:3, h:3, norm:9, k: [1, 1, 1, 1, 1, 1, 1, 1, 1] } );
//let tx_e = tx.toHSV().split(0);
let tx_e = tx.split(3, {x:32, y:32, w:32, h:32} );
tx_e.repeat_s = true;
tx_e.repeat_t = true;

let ipid = null;
let opid = null;
let width = 0;
let height = 0;
let stride = 0;
let timescale = 0;
let pf = '';

let mxtxt = new evg.Matrix2D();
mxtxt.translate(-50, 100);

filter.initialize = function() {

this.set_cap({id: "StreamType", value: "Video", inout: true} );
this.set_cap({id: "CodecID", value: "raw", inout: true} );
}

filter.configure_pid = function(pid)
{
	if (!ipid) {
		opid = filter.new_pid();
	}
	ipid = pid;
	opid.copy_props(pid);
	width = pid.get_prop('Width');
	height = pid.get_prop('Height');
	stride = pid.get_prop('Stride');
	pf = pid.get_prop('PixelFormat');
	timescale = pid.get_prop('Timescale');
	print(`pid configured: ${width} ${height} ${pf}`);
}

filter.process = function()
{
	if (!ipid) return GF_OK;

	let pck = ipid.get_packet();
	if (!pck) return;


	let opck;
	try {
	  opck = opid.new_packet(pck);
	} catch (e) {
		print("error cloning frame!");
		ipid.drop_packet();
		return;
	}
	opck.copy_props(pck);

	let canvas = new evg.Canvas(width, height, pf, opck.data, stride);
	canvas.centered = centered;

	mx.rotate(0, 0, 0.05);

	canvas.path = path;

	let mmx = new evg.Matrix2D();

	//move to local object space
	let bounds = path.bounds;
	mmx.scale(bounds.w/tx.width, bounds.h/tx.height);
	mmx.translate(bounds.w/2, bounds.h/2);
	mmx.add(mx);

	canvas.matrix = mx;
	tx.mx = mmx;
	canvas.fill(tx);

	mx1.rotate(0, 0, 0.05);
	//let live_tx = new evg.Texture(width, height, pf, opck.data, stride);
	let live_tx = new evg.Texture(canvas);
//	let live_tx = new evg.Texture(pck);
	live_tx.repeat_s = true;
	live_tx.repeat_t = true;
	mmx.identity=true;
	mmx.scale(bounds.w/live_tx.width, bounds.h/live_tx.height);
	mmx.translate(bounds.w/2, bounds.h/2);
	mmx.add(mx1);
	canvas.matrix = mx1;
	live_tx.mx = mmx;
	//live_tx.set_alphaf(0.5);
	canvas.fill(live_tx);

	let t = opck.dts;
	let ms = t*1000;
	t/=timescale;
	ms /= timescale;
	let h = Math.floor(t/3600);
	let m = Math.floor(t/60 - h*60);
	let s = Math.floor(t - h*3600 - m*60);
	ms -= (h*3600 + m*60 + s) * 1000;

	text.text = ['GPAC Overlay demo', `Rasterizing in ${pf} color space` , `Frame time: ${h}:${m}:${s}.${ms}`];
	brush.set_color('white');
	canvas.matrix = mxtxt;
	canvas.path = text;
	canvas.fill(brush);

 	ipid.drop_packet();
    opck.send();
	return GF_OK;
}


