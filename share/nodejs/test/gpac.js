const gpac = require('..');
const filesys = require('fs');

console.log('GPAC NodeJS version ' + gpac.version + ' libgpac ' + gpac.abi_major + ':' + gpac.abi_minor + '.' + gpac.abi_micro);
console.log('' + gpac.copyright_cite);

//run in blocking mode
const RUN_SYNC=0;
//run in step mode but blocking node eventloop
const RUN_STEP=1;
//run in step mode and not blocking node eventloop
const RUN_PROMISE=2;

//test simple session, no custom filters
const TEST_SIMPLE=0;
//test sink
const TEST_SINK=1;
//test forward
const TEST_FORWARD=2;

let run_mode=RUN_SYNC;
let test_task=false;
let run_task_dur=0;
let test_type=TEST_SIMPLE;
let FILTERS=[];
let SINKS=[];
let SRCS=[];
let memtrack=0;
let is_verbose=false;
let start_time=0;
let custom_dash=false;

let fileio_sources=false;
let fileio_sinks=false;

const PCKFWD_REF=0;
const PCKFWD_ALLOC=1;
const PCKFWD_SHARED=2;
const PCKFWD_CLONE=3;
const PCKFWD_COPY=4;
const PCKFWD_FWD=5;
const PCKFWD_ALL=6;


let pck_fwd_mode=PCKFWD_FWD;

print_usage = () => {
	console.log("node gpac.js [options]");
	console.log();
	console.log("This scripts tests usage of GPAC NodeJS bindings.\n\nPossible options are:");
	console.log("-run=MODE:    runs in given MODE:");
	console.log("\tsync (default): blocking filter session, Node EventLoop blocked");
	console.log("\tstep: non-blocking filter session, Node EventLoop blocked");
	console.log("\tasync: non-blocking filter session as promise, Node EventLoop active");
	console.log("-cust=MODE:   custom JS filter test mode:");
	console.log("\tsimple (default): runs source(s), sink(s) and filters");
	console.log("\tsink: runs source(s) into custom JS filter sink");
	console.log("\tfwd: runs source(s) into custom JS filter forwarding to sink(s) (-dst=) or to filter(s) (-f=)");
	console.log("-fwd=MODE:    packet forward mode for forward test mode:");
	console.log("\tfwd (default): uses packet forwarding");
	console.log("\tref: uses packet reference");
	console.log("\talloc: copies data");
	console.log("\tshared: use shared JS data and JS destruction callbacks");
	console.log("\tclone: uses packet cloning");
	console.log("\tcopy: uses packet copy");
	console.log("\tall: cycles through all above modes");

	console.log("-src=URL:     adds a source URL. Can be set multiple times for multiple sources");
	console.log("-dst=URL:     adds a destination URL. Can be set multiple times for multiple sinks");
	console.log("-f=FILTER:    adds a filter (e.g., 'avgen'). Can be set multiple times for filters, cf `gpac -h doc` for link options");
	console.log("-mt:          enables GPAC memory tracking");
	console.log("-p=PROF:      sets profile to PROF");
	console.log("-ib:          enables FileIO wrapper for source(s)");
	console.log("-ob:          enables FileIO wrapper for sink(s)");
	console.log("-task=N:      runs user test task every second for N seconds (0 means until end)");
	console.log("-play=N:      overrides PLAY event in forward mode to the given start time in seconds");
	console.log("-cdash:       test custom DASH algo");
	console.log("-v:           enables info dump, property dump for PIDs and packets");
	console.log("-h:           prints help");
	console.log();
	console.log("Other options will be passed to GPAC");
	console.log();
	console.log("Examples");
	console.log("- Play MyURL to aout and vout:\nnode gpac.js -src=MyURL -f=aout -f=vout\n");
	console.log("- Remux MyURL to DASH and HLS (assumes directory dash exists):\nnode gpac.js -src=MyURL -dst=dash/live.mpd:dual\n");
	console.log("- Run avgen to aout and vout:\nnode gpac.js -f=avgen -f=aout -f=vout\n");
	console.log("- Play MyURL into custom node filter then to aout and vout:\nnode gpac.js -cust=fwd -src=MyURL -f=aout -f=vout\n");
	console.log("- Play MyURL into custom node filter, get input file from NodeJS, keep node eventloop active and use 2 extra threads:\nnode gpac.js -run=async -threads=2 -cust=fwd -ib -src=MyURL -f=aout -f=vout\n");
	console.log();

	console.log();
	process.exit(0);
}
bad_param = (v) => {
	console.log(v);
	process.exit(1);
};

let other_args=['nodegpac'];

let fio_factory = {
	open: function(url, mode) {

		this.file = null;
		this.size = 0;
		this.position = 0;
		this.read_mode = false;
		this.is_eof = false;
		this.url = url;
		mode = mode.replace('b', '');
		mode = mode.replace('t', '');

		try {
			this.file = filesys.openSync(url, mode);
		} catch (e) {
			console.log('Fail to open ' + url + ' in mode ' + mode + ': ' + e);
			return false;
		}
		if (mode.indexOf('w')<0) {
			let stats = filesys.fstatSync(this.file);
			this.size = stats.size;
			if (mode.indexOf('a+')>=0) {
				this.position = this.size;
			}
			this.read_mode = true;
		}
		return true;
	},
	close: function() {
		filesys.closeSync(this.file);
		this.file = null;
	},
	read: function(buf) {
		let nb_bytes = 0;
		try {
			nb_bytes = filesys.readSync(this.file, buf, 0, buf.length, this.position);
		} catch (e) {
			console.log('read error: ' + e);
			return 0;
		}
		if (!nb_bytes) this.is_eof = true;
		this.position += nb_bytes;
		return nb_bytes;
	},
	write: function(buf) {
		let nb_bytes = filesys.writeSync(this.file, buf, 0, buf.length, this.position);
		if (this.position == this.size) {
			this.size += nb_bytes;
		}
		this.position += nb_bytes;
		return nb_bytes;
	},
	seek: function(pos, whence) {
		this.is_eof = false;
		if (pos<0) return -1;
		//seek set
		if (whence==0) {
			this.position = pos;
		}
		//seek cur
		else if (whence==1) {
			this.position += pos;
		}
		//seek end
		else if (whence==2) {
			if (this.size < pos) return -1;
			this.position = this.size - pos;
		} else {
			return -1;
		}
		return 0;
	},
	tell: function() {
		return this.position;
	},
	eof: function() {
		return this.is_eof;
	},
	exists: function(url) {
		try {
			filesys.accessSync(url);
		} catch (err) {
			return false;
		}
		return true;
	}
};


function push_source(url)
{
	if (fileio_sources) {
		let fio = new gpac.FileIO(url, fio_factory);
		url = fio.url;
	}
	SRCS.push(url);
}

function push_sink(url)
{
	if (fileio_sinks) {
		let fio = new gpac.FileIO(url, fio_factory);
		url = fio.url;
	}
	SINKS.push(url);
}

//first pass, init libgpac mem track and profile if set
//this must be done before any new constructor (FileIO, FilterSession) otherwise it is ignored
let profile=null;
let args = process.argv.splice(1);
args.forEach( val => {
	if (val=='-h') print_usage();
	else if (val == '-mt') memtrack = gpac.GF_MemTrackerSimple;
	else if (val.startsWith('-p=')) {
		profile = val.substring(3);
	}
});

if (memtrack || profile)
	gpac.init(memtrack, profile);

//second pass, parse all other args
args.forEach( val => {
	if (val=='-h') { }
	else if (val=='-mt') { }
	else if (val.startsWith('-p=')) { }

	else if (val=='-run=sync') run_mode=RUN_SYNC;
	else if (val=='-run=step') run_mode=RUN_STEP;
	else if (val=='-run=async') run_mode=RUN_PROMISE;
	else if (val.startsWith('-run=')) { bad_param("Unknown run mode "+val); }

	else if (val=='-cust=simple') test_type=TEST_SIMPLE;
	else if (val=='-cust=sink') test_type=TEST_SINK;
	else if (val=='-cust=fwd') test_type=TEST_FORWARD;
	else if (val.startsWith('-cust=')) { bad_param("Unknown custom test mode "+val); }

	else if (val=='-fwd=fwd') pck_fwd_mode=PCKFWD_FWD;
	else if (val=='-fwd=ref') pck_fwd_mode=PCKFWD_REF;
	else if (val=='-fwd=alloc') pck_fwd_mode=PCKFWD_ALLOC;
	else if (val=='-fwd=shared') pck_fwd_mode=PCKFWD_SHARED;
	else if (val=='-fwd=copy') pck_fwd_mode=PCKFWD_COPY;
	else if (val=='-fwd=clone') pck_fwd_mode=PCKFWD_CLONE;
	else if (val=='-fwd=all') pck_fwd_mode=PCKFWD_ALL;
	else if (val.startsWith('-fwd=')) { bad_param("Unknown packet forward mode "+val); }

	else if (val=='-task') { test_task = true; run_task_dur = -1; }
	else if (val.startsWith('-task=')) {
		run_task_dur = parseInt(val.substring(6));
		if (run_task_dur == 0) run_task_dur = -1;
		test_task = true;
	}
	else if (val.startsWith('-src=')) { push_source( val.substring(5) ); }
	else if (val.startsWith('-dst=')) { push_sink( val.substring(5) ); }
	else if (val.startsWith('-f=')) { FILTERS.push( val.substring(3)  ); }
	else if (val == '-v') is_verbose = true;
	else if (val.startsWith('-play=')) {
		start_time = parseFloat(val.substring(6));
	}
	else if (val == '-ib') fileio_sources = true;
	else if (val == '-ob') fileio_sinks = true;
	else if (val == '-cdash') custom_dash = true;

	else if (val.startsWith('-')) {
		other_args.push(val);
	}
});

if (other_args.length > 1)
	gpac.set_args(other_args);

if (!SRCS.length && !SINKS.length && !FILTERS.length) {
	console.log("No source, destination or filter specified, check usage (-h)");
	process.exit(1);
}

let dash_algo = null;
if (custom_dash) {
	dash_algo = {
		on_period_reset: function(type) {
			console.log('period reset type ' + type);
		},
		on_new_group: function(group) {
			console.log('Got new group ' + JSON.stringify(group) );
		},
		on_rate_adaptation: function(group, base_group, force_lower_complexity, stats) {
			console.log('Rate adaptation on group ' + group.idx + ' - stats ' + JSON.stringify(stats) );
			return 0;
		},
		on_download_monitor: function(group, stats) {
			console.log('Download monitor on group ' + group.idx + ' - stats ' + JSON.stringify(stats) );
			return -1;
		}
	};
}


let fs;
if (run_mode==RUN_SYNC) {
	fs = new gpac.FilterSession();
} else {
	fs = new gpac.FilterSession(gpac.GF_FS_FLAG_NON_BLOCKING);
}

fs.on_filter_new = function(f)
{
	if (is_verbose) 
		console.log('New filter created: ' + f.name);
	if (dash_algo && (f.name=="dashin")) {
		try {
			f.bind(dash_algo);
		} catch (e) {
			console.log('Failed to bind dash algo: ' + e);
		}
	}
}
fs.on_filter_del = function(f) {
	if (is_verbose) 
		console.log('Filter deleted: ' + f.name);
}


if (is_verbose) {
	console.log('Error code GF_URL_ERROR means ' + gpac.e2s(gpac.GF_URL_ERROR) );
	console.log('Supported mime video/mp4: ' + fs.is_supported_mime('video/mp4') );
	if (SRCS.length)
		console.log('Supported file ' + SRCS[0] + ': ' + fs.is_supported_source(SRCS[0]) );
}


if (test_task) {
	t = {};
	t.nb_tasks = 0;
	t.execute = function() {
		if (fs.last_task) {
			return false;
		}
		this.nb_tasks++;
		if (run_task_dur > 0) {
			run_task_dur -= 1;
			if (run_task_dur <= 0) {
				console.log('Last task run #' + this.nb_tasks);
				return false;
			}
		}
		console.log('Task run #' + this.nb_tasks);
		return 1000;
	}
	fs.post_task(t);
}

fs.on_event = function(evt)
{
	//console.log('FS event ' + JSON.stringify(evt) );
	if (evt.ui_type == gpac.GF_EVENT_QUIT) {
		fs.abort(gpac.GF_FS_FLUSH_NONE);
		return true;
	}
	return false;
}

//load sources
SRCS.forEach( src => {
	console.log('Loading source URL ' + src);
	src_f = fs.load_src(src);
});

const dump_prop = (type, val, ptype) => {
	if (ptype==gpac.GF_PROP_DATA)
		console.log('\t' + type + ': raw data');
	else if (typeof val == 'object')
		console.log('\t' + type + ': ' + JSON.stringify(val) );
	else
		console.log('\t' + type + ': ' + val + ' (' + (typeof val) + ')');
}

//load custom filter
let cust = null;
if (test_type != TEST_SIMPLE) {
	cust = fs.new_filter("NodeJS_Test");
	//we accept any number of input pids
	cust.set_max_pids(-1);
	cust.pids = [];

	if (test_type == TEST_FORWARD) {
		cust.push_cap('StreamType', 'Visual', gpac.GF_CAPS_INPUT_OUTPUT);
		cust.push_cap('StreamType', 'Audio', gpac.GF_CAPS_INPUT_OUTPUT);
	} else {
		cust.push_cap('StreamType', 'Visual', gpac.GF_CAPS_INPUT);
		cust.push_cap('StreamType', 'Audio', gpac.GF_CAPS_INPUT);
	}

	cust.configure_pid = function(pid, is_remove)
	{
		if (this.pids.indexOf(pid) < 0) {
			this.pids.push(pid);
			pid.nb_pck = 0;

			if (test_type == TEST_SINK) {
				let evt = new gpac.FilterEvent(gpac.GF_FEVT_PLAY);
				pid.send_event(evt);
				if (is_verbose) {
					console.log('Input PID Props: ');
					pid.enum_props( dump_prop );
				}
				pid.opid = null;
			}

			else {
				//create output pid
				pid.opid = this.new_pid();
				pid.opid.copy_props(pid);
				if (is_verbose) {
					console.log('Output PID props: ');
					pid.opid.enum_props( dump_prop );
				}
			}
		} else if (is_remove) {
			console.log('PID remove !');
		} else {
			console.log('PID reconfigure !');
			pid.opid.copy_props(pid);
		}
		return gpac.GF_OK;
	}
	cust.pck_clone_cache = null;
	if (pck_fwd_mode==PCKFWD_ALL) {
		cust.fwd_mode = 0;
		cust.fwd_loop = true;
	} else {
		cust.fwd_mode = pck_fwd_mode;
		cust.fwd_loop = false;
	}

	cust.process = function() {
		let nb_eos=0;

		this.pids.forEach(pid =>{
			if (pid.eos) {
				nb_eos++;
				if (pid.opid) pid.opid.eos = true;
				return;
			}
			//begin of packet fetch loop
			while (1) {

			let pck = pid.get_packet();
			if (!pck) return;

			if (is_verbose) {
				console.log('PCK props: ' + JSON.stringify(pck) );
				pck.enum_props( dump_prop );
			}
			pid.nb_pck ++;
			if (test_type == TEST_SINK) {
				pid.drop_packet();
				continue;
			}
			//send by reference
			if (this.fwd_mode==PCKFWD_REF) {
				let dst = pid.opid.new_pck_ref(pck);
				dst.copy_props(pck);
				dst.send();
			}
			//full copy mode
			else if (this.fwd_mode==PCKFWD_ALLOC) {
				let dst = pid.opid.new_pck(pck.size);
				dst.copy_props(pck);
				new Uint8Array(dst.data).set(new Uint8Array(pck.data));
				dst.send();
			}
			//keep ref to packet and send new packet using shared data
			else if (this.fwd_mode==PCKFWD_SHARED) {
				/*
					Ultimately we would like to test ab = pck.data + pck.ref() and pck.unref() in destructor
					however this crashes node:
						https://github.com/node-ffi-napi/ref-napi/issues/54
				*/
				let ab = new ArrayBuffer(pck.size);
				new Uint8Array(ab).set(new Uint8Array(pck.data));
				//keep ref
				pck.ref();
				let dst = pid.opid.new_pck_shared(ab, () => { pck.unref();});
				dst.copy_props(pck);
				dst.send();
			}
			//pck_clone
			else if (this.fwd_mode==PCKFWD_CLONE) {
				let dst = pid.opid.new_pck_clone(pck);
				dst.send();
			}
			//pck_copy
			else if (this.fwd_mode==PCKFWD_COPY) {
				let dst = pid.opid.new_pck_copy(pck);
				dst.send();
			} else {
				pid.opid.forward(pck);
			}
			if (this.fwd_loop) {
				this.fwd_mode++;
				if (this.fwd_mode==6) this.fwd_mode=0;
			}
			pid.drop_packet();
			} //end of packet fetch loop
		});

		if (nb_eos == this.pids.length)
			return gpac.GF_EOS;

		return gpac.GF_OK;
	}
	cust.process_event = function(evt)
	{
		//shortcut play event, canceling it and sending a play event with a different start
		if ((evt.type==gpac.GF_FEVT_PLAY) && start_time) {
			let anevt = new gpac.FilterEvent(gpac.GF_FEVT_PLAY);
			anevt.start_range = start_time;
			this.pids.forEach(pid => pid.send_event(anevt));
			return true;
		}
		return false;
	}
}

//load sinks
if (test_type != TEST_SINK) {
	SINKS.forEach( sink => {
		console.log('Loading dest ' + sink);
		let sink_f = fs.load_dst(sink);
		//set sink source to be our custom filter
		if (cust && !FILTERS.length)
			sink_f.set_source(cust);
	});
}

//load filters
FILTERS.forEach( fname => {
	console.log('Loading filter ' + fname);
	let f = fs.load(fname);

	if (test_type != TEST_SINK) {
		//set source to be our custom filter
		if (cust && !SINKS.length)
			f.set_source(cust);
	}
});

//print custom filter connections
if (cust && is_verbose) {
	console.log('CustomFilter connections ');
	cust.print_connections();
}

console.log('');

const session_done = () => {
	console.log('Session done running, graph: ');
	fs.print_graph();
};

//runin sync mode
if (run_mode==RUN_SYNC) {
	console.log('Running in blocking mode');
	fs.run();
	session_done();
	return;
}

//runin step mode
if (run_mode==RUN_STEP) {
	console.log('Running in non-blocking mode');
	while (true) {
		fs.run();
		if (fs.last_task) break;
	}
	session_done();
	return;
}

//Run session step as Promise
const FilterSessionPromise = (fs_run_task) => {
  var fsrun_promise = () => {
    return (fs.last_task==false) ? fs_run_task().then(fsrun_promise) : Promise.resolve();
  }
  return fsrun_promise();
};

const run_task = () => {
	return new Promise((resolve, reject) => {
	  resolve( fs.run() );
	});
}
FilterSessionPromise(run_task).then( ).finally( session_done );

console.log('Entering NodeJS EventLoop');
