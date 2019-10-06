import * as evg from 'evg'

//metadata
filter.set_name("Graphics Burning Test !");
filter.set_desc("JS-based vector graphics generator");
filter.set_version("0.1beta");
filter.set_author("GPAC team");
filter.set_help("This filter provides testing of gpac's vector graphics bindings");

let path = new evg.Path();
path.ellipse(0, 0, 200, 100);
path.ellipse(0, 0, 100, 50);
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

let tx = new evg.Texture();
tx.load("../auxiliary_files/logo.png", true);

let ipid = null;
let opid = null;
let width = 0;
let height = 0;
let stride = 0;
let timescale = 0;
let pf = '';

let mxtxt = new evg.Matrix2D();
mxtxt.translate(-50, -100);

filter.initialize = function() {

this.set_cap("StreamType", "Video", true);
this.set_cap("StreamType", "Video", false);
this.set_cap("CodecID", "raw", true);
this.set_cap("CodecID", "raw", false);
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
	ipid.drop_packet();

	let canvas = new evg.Canvas(width, height, pf, opck.data, stride);
	canvas.centered = centered;

	mx.rotate(0, 0, 0.1);

	canvas.path = path;
	canvas.matrix = mx;

	tx.repeat_s = true;
	tx.repeat_t = true;
	let mmx = new evg.Matrix2D();
	mmx.scale(200/tx.width, 100/tx.height);
	mmx.translate(100, 50);
	mmx.add(mx);
	tx.mx = mmx;
	tx.set_alphaf(0.5);
	canvas.fill(tx);

	let t = opck.dts;
	let ms = t*1000;
	t/=timescale;
	ms /= timescale;
	let h = Math.floor(t/3600);
	let m = Math.floor(t/60 - h*60);
	let s = Math.floor(t - h*3600 - m*60);
	ms -= (h*3600 + m*60 + s) * 1000;

	text.text = ['GPAC Overlay demo', `Rasterizing in ${pf} color space`, `Frame time: ${h}:${m}:${s}.${ms}`];
	brush.set_color('white');
	canvas.matrix = mxtxt;
	canvas.path = text;
	canvas.fill(brush);

    opck.send();
	return GF_OK;
}


