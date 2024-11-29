import { Sys as sys } from 'gpaccore'
import {XMLHttpRequest} from 'xhr'

let use_port=0;
let use_cache = false;
let check_ip = false;

const MANI_DASH = 1;
const MANI_HLS = 2;
const HLS_MASTER = 1;
const HLS_VARIANT = 2;
const LIVE_EDGE_MAX_DIST = 5;
const DEACTIVATE_TIMEOUT_MS = 5000;
//how long we will make the client wait - this should be refined depending on whether repair is used (fragments pushed on the fly) or not (full file loaded)
const MABR_TIMEOUT_SAFETY = 1000;

const DEFAULT_UNLOAD_SEC = 4;
const DEFAULT_ACTIVATE_CLIENTS = 1;
const DEFAULT_MCACHE = false;
const DEFAULT_REPAIR = false;
const DEFAULT_CORRUPTED = false;

let def_purge_delay =0;

//metadata
filter.set_name("proxy");
filter.set_class_hint(GF_FS_CLASS_MM_IO);
filter.set_desc("HTTP Proxy");
filter.set_version("1.0");
filter.set_author("GPAC team - (c) Telecom Paris 2024 - license LGPL v2");
filter.set_help("This filter is an HTTP proxy for GET and HEAD requests supporting Multicast ABR sources.\n"
+"\n"
+"This filter does not produce PIDs, it attaches to HTTP output filter. If no HTTP server is specified in the session, the filter will create a server running on the specified [-port](). "
+"If more options need to be specified for the HTTP server, it is recommended to specify the server explicitly.\n"
+"EX gpac http_proxy httpout:OPTS\n"
+"\n"
+"# Services Configuration\n"
+"The service configuration is set using [-sdesc](). It shall be a JSON file containing a single array of services.\n"
+"Each service is a JSON object which shall have the following properties:\n"
+" - http: (string) URL of root manifest (MPD for DASH or master m3u8 for HLS)\n"
+" - mcast: (string, default null) multicast address for this service\n"
+" - timeshift: (integer) override [–timeshift]() for this service\n"
+" - unload: (integer, default "+DEFAULT_UNLOAD_SEC+") multicast unload policy\n"
+" - activate: (integer, default "+DEFAULT_ACTIVATE_CLIENTS+") multicast activation policy\n"
+" - repair: (boolean, default "+DEFAULT_REPAIR+") enable unicast repair in MABR stack (experimental)\n"
+" - mcache: (boolean, default "+DEFAULT_MCACHE+") cache manifest files (experimental)\n"
+" - corrupted: (boolean, default "+DEFAULT_CORRUPTED+") forward corrupted files if parsable (valid container syntax, broken media)\n"
+"\n"
+"# HTTP Streaming Cache\n"
+"The proxy will cache live edge segments in HTTP streaming sessions for [–timeshift]() seconds. This caching is always done in memory.\n"
+"A service with a [–timeshift]() value of 0 will disable this functionality, and N clients asking from the same segment will result in N requests being forwarded.\n"
+"\n"
+"Manifests files fetched from origin can be cached using the `mcache` service configuration option but this is currently not recommended for low latency services.\n"
+"\n"
+"# Multicast ABR cache\n"
+"The proxy can be configured to use a multicast source as an alternate data source for a given service through the `mcast` service description file.\n"
+"The multicast source can be DBB-MABR (e.g. `mabr://235.0.0.1:1234/`), ATSC3.0 (e.g. `atsc://`) or ROUTE (e.g. `route://235.0.0.1:1234/`).\n"
+"If the multicast is replayed from a file, netcap ID shall be set in this multicast URL (e.g. `:NCID=N`).\n"
+"If a specific IP interface is used, it can also be set in multicast URL (e.g. `:ifce=IP`).\n"
+"\n"
+"The multicast service can be dynamically loaded at run-time using the `unload` service configuration option:\n"
+"- if 0, the multicast is started when loading the proxy and never ended,\n"
+"- otherwise, the multicast is started dynamically and ended `unload` seconds after last deactivation.\n"
+"\n"
+"The qualities in the multicast service can be dynamically activated or deactivated using the `activate` service configuration option:\n"
+"- if 0, multicast streams are never deactivated,\n"
+"- otherwise, a multicast representation is activated only if at least `active` clients are consuming it, and deactivated otherwise.\n"
+"\n"
+"The multicast service can use repair options of the MABR stack using `repair` service configuration option:\n"
+"- if false, the file will not be sent until completely received. This increases latency,\n"
+"- otherwise, file will be send as soon as new data arrived\n"
+"Using `repair=true` is experimental, the repair stack does yet support this feature for the moment. Only use if you're sure you don't have any multicast losses (local tests).\n"
+"\n"
+"The number of active clients on a given quality is computed using the client connection state: any disconnect/reconnect from a client for the same quality will trigger a deactivate+activate sequence.\n"
+"If [-checkip]() is used, the remote IP address+port are used instead of the connection. This however assumes that each client has a unique IP/port which may not always be true (NATs).\n"
+"\n"
+"If [–timeshift]() is 0 for the service, multicast segments will be trashed as soon as not in use (potentially before the client request).\n"
+"\n"
+"Manifests files coming from multicast are currently never cached.\n"
);

filter.set_arg({ name: "port", desc: "port to use when no server HTTP is specified", type: GF_PROP_UINT, def: "80"} );
filter.set_arg({ name: "sdesc", desc: "service configuration file", type: GF_PROP_STRING} );
filter.set_arg({ name: "gcache", desc: "use gpac cache when fetching media", type: GF_PROP_BOOL, def: "false" } );
filter.set_arg({ name: "timeshift", desc: "timeshift buffer in seconds for HTTP streaming services", type: GF_PROP_UINT, def: "10"} );
filter.set_arg({ name: "checkip", desc: "monitor IP address for activation", type: GF_PROP_BOOL, def: "false"} );

let services_defs = [
//	{'http': "http://127.0.0.1:8080/live.mpd", "mcast": "mabr://235.0.0.1:1234/", "start": false, "keep": false, "netcap": "" }
];

let all_services = [];

function setup_representation(rep)
{
	rep.mabr_active = false;
	rep.mabr_deactivate_timeout = 0;
	rep.nb_active = 0;
	rep.seg_id = null;
	rep.seg_dur = 0;
	rep.hls_seq_start = 0;
	rep.live_edge = false;
	rep.radical = null;
	rep.suffix = null;
	rep.last_edge_compute = 0;
	if (rep.template) {
		let idx = rep.template.indexOf('$');
		rep.radical = rep.template.substring(0, idx);
		let remain = rep.template.substring(idx+1);
		idx = remain.indexOf('$');
		rep.suffix = remain.substring(idx+1);
	}
}

function update_manifest(mani, target_url)
{
	let hls_active_rep = null;
	let service = all_services.find(s => {
		if (target_url.indexOf(s.url)>=0) return true;
		if (!s.manifest || !s.manifest.m3u8) return false;
		if (!s.manifest) return false;
		let period = s.manifest.periods[0];
		//for HLS, locate rep for this variant URL
		let rep = period.reps.find(e => {
			if (!e.xlink) return false;
			//absolute URLs
			if (e.xlink == target_url) return true;
			let idx = target_url.indexOf(e.xlink);
			if (idx<0) return false;
			if (target_url.startsWith(s.base_url)) return true;
			return false;  
		});
		if (rep) {
			hls_active_rep = rep;
			return true;
		}
		return false;
	});
	if (!service) {
		if (mani.m3u8 == HLS_VARIANT) {
			print(GF_LOG_ERROR, `Failed to locate service for HLS variant ${target_url}`);
			return;
		}
		service = create_service(target_url, true);
	}

	//HLS master playlist
	if (mani.m3u8 == HLS_MASTER) {
		//we assume master playlist is static
		if (service.manifest) return;
		let period = mani.periods[0];
		period.reps = period.reps.sort((a, b) => a.bandwidth - b.bandwidth);
		for (let r=0; r<period.reps.length; r++) {
			setup_representation( period.reps[r] );
		}
		service.manifest = mani;
		return;
	}
	//HLS variant playlist
	if (mani.m3u8 == HLS_VARIANT) {
		if (!service.manifest) return;
		let period = service.manifest.periods[0];
		if (!hls_active_rep) {
			print(GF_LOG_ERROR, `Service ${service.id} cannot find rep for target ${target_url}`);		
			return;
		}
		hls_active_rep.segments = mani.segments;
		hls_active_rep.hls_seq_start = mani.seq_start;
		hls_active_rep.live_seg_num = mani.live_seg_num;
		service.manifest.live = mani.live;

		print(GF_LOG_DEBUG, `Service ${service.id} updated HLS manifest`); // + JSON.stringify(service.manifest));
		return;
	}

	for (let p=0; p<mani.periods.length; p++) {
		let period = mani.periods[p];
		period.reps = period.reps.sort((a, b) => a.bandwidth - b.bandwidth);
		for (let r=0; r<period.reps.length; r++) {
			setup_representation( period.reps[r] );
		}
	}
	let updated = false;
	if (!service.manifest) {
		service.manifest = mani;
	} else {
		updated = true;
		service.manifest.live_utc = mani.live_utc;
		//update existing manifest
		//remove old periods
		for (let p=0; p<service.manifest.periods.length; p++) {
			let period = service.manifest.periods[p];
			let new_p = mani.periods.find(e => e.id == period.id);
			if (new_p) continue;
			service.manifest.periods.splice(p);
			p--;
			continue;
		}
		//push new periods
		for (let p=0; p<mani.periods.length; p++) {
			let rep_inserted = false;
			let period = mani.periods[p];
			let old_p = service.manifest.periods.find(e => e.id == period.id);
			if (!old_p) {
				service.manifest.periods.push(period);
				continue;
			}
			//same period, update reps
			for (let r=0; r<period.reps.length; r++) {
				let rep = period.reps[r];
				let old_r = old_p.reps.find(e => (e.ID == rep.ID || e.bandwidth == rep.bandwidth));
				//rep injection is not prohibited, rep removal is
				if (!old_r) {
					setup_representation( rep);
					old_p.reps.push(rep);
					rep_inserted = true;
					continue;
				}
				//same rep, update segments and live edge info
				old_p.bandwidth = rep.bandwidth;
				old_r.segments = rep.segments;
				old_r.live_seg_num = rep.live_seg_num;
			}
			if (rep_inserted) {
				old_p.reps = old_p.reps.sort((a, b) => a.bandwidth - b.bandwidth);
			}
		}
	}
	print(GF_LOG_DEBUG, `Service ${service.id} manifest ${ (updated ? 'updated' : 'received')} for ${target_url}`); // + ': ' + JSON.stringify(service.manifest));
}

function locate_service_quality(target_url)
{
	let service = null;
	let with_base_url = null;
	for (let i=0; i<all_services.length; i++) {
		service = all_services[i];
		if (service.url == target_url) break;
		if (! service.manifest) {
			service=null;
			continue;
		}

		//todo, deal with base urls ?
		let base_url = service.base_url;
		if (target_url.startsWith(base_url)) break;
		service=null;
	}
	if (!service) return [null,null,null];
	if (!service.manifest) return [service, null,null];

	if (!with_base_url) with_base_url = service.base_url;

	//extract seg name
	let seg_name = target_url.substring(with_base_url.length);

	let period = null;
	let rep = null;
	let now = sys.get_utc();
	//find segment
	for (let p=0; p<service.manifest.periods.length; p++) {
		period = service.manifest.periods[p];
		for (let r=0; r<period.reps.length; r++) {
			rep = period.reps[r];
			rep.seg_id = null;
			rep.seg_dur = 0;
			rep.live_edge = false;
			rep.seg_num = 0;
			if (rep.xlink && (seg_name.indexOf(rep.xlink)>=0)) {
				rep=null;
				continue;
			}
			if (rep.radical && ! seg_name.startsWith(rep.radical)) {
				rep=null;
				continue;
			}

			//m3u8 or SegmentTimeline
			if (rep.segments.length) {
				let found_rep = false;
				for (let si=0; si<rep.segments.length; si++) {
					let s_idx = rep.segments.length - 1 - si;
					let seg = rep.segments[s_idx];
					if (seg.url == seg_name) {
						rep.seg_dur = seg.duration * 1000 / rep.timescale;
						if (rep.radical) {
							let num_time = seg_name.substring(rep.radical.length);
							let idx = num_time.indexOf(rep.suffix);
							if (idx>=0) {
								rep.seg_id = num_time.substring(0, idx);
							}	
						} else if (service.manifest.m3u8) {
							rep.seg_id = '' + (rep.hls_seq_start + s_idx);
						}
						if (si<=LIVE_EDGE_MAX_DIST) {
							rep.live_edge = true;
						}
						found_rep = true;
						break;
					}
				}
				if (found_rep) break;
			}

			if (rep.template) {
				let num_time = seg_name.substring(rep.radical.length);
				let idx = num_time.indexOf(rep.suffix);
				if (idx>=0) {
					rep.seg_id = num_time.substring(0, idx);
				}
				if (!rep.seg_dur) 
					rep.seg_dur = rep.duration * 1000 / rep.timescale;

				//compute live edge time divide then multiply to avoid overflow
				if (rep.last_edge_compute + rep.seg_dur < now) {
					let live_edge_num = now;
					live_edge_num -= service.manifest.ast;
					live_edge_num -= period.start;
					live_edge_num /= 1000;
					live_edge_num /= rep.duration;
					live_edge_num *= rep.timescale;
					live_edge_num = Math.ceil(live_edge_num) + 1;
					rep.live_seg_num = live_edge_num;
					rep.last_edge_compute = now;
				}
				//todo in live, figure out if we are on the live edge
				if (rep.live_seg_num && rep.seg_id) {
					rep.seg_num = parseInt(rep.seg_id);
					let seg_num = rep.seg_num - rep.live_seg_num;
					if (Math.abs(seg_num) <= LIVE_EDGE_MAX_DIST) {
						rep.live_edge = true;
					}
				}
				break;
			}
			rep = null;
		}
		if (rep) {
			rep.period_id = period.ID;
			break;
		}
		period = null;
	}
	print(GF_LOG_DEBUG, `Located service ${service.url} for URL ${seg_name}${(rep ? (' rep: ' + rep.ID ) : '' )}`);
	return [service, rep, period];
}



function cat_buffer(dst_ab, src_ab) 
{
	let tmp = new Uint8Array(dst_ab ? dst_ab.byteLength + src_ab.byteLength : src_ab.byteLength);
	if (dst_ab)
		tmp.set(new Uint8Array(dst_ab), 0);
	tmp.set(new Uint8Array(src_ab), dst_ab ? dst_ab.byteLength : 0);
	return tmp.buffer;
}


//custom HTTPout request handler 
let httpout = {};
httpout.on_request = (req) => 
{
	let start=0, end=0;
	let not_cachable=false;

	//not yet supported
	if ((req.method !== 'GET') && (req.method !== 'HEAD')) {
		req.reply = 405;
		req.send('Method not allowed by proxy');
		return;
	}

	print(GF_LOG_DEBUG, `Got request ${req.method} for ${req.url}`);
	req.target_url = null;
	req.xhr = null;
	if (req.url.startsWith('/http://') || req.url.startsWith('/https://')) {
		req.target_url = req.url.substring(1);
		if ((target_url.indexOf("127.0.0.1")>=0) || (target_url.indexOf("localhost")>=0)) {
			req.reply = 405;
			req.send('Must use Host for localhost proxying');
			return;
		}
	} else {
		let host = null;
		req.headers_in.forEach(h => {
			let hlwr = h.name.toLowerCase();
			if (hlwr == "host") host = h.value;
			else if (hlwr == ":authority") host = h.value;
			if (hlwr == "range") {
				let hdr_range = h.value.split('=');
				let range = [1];
				if (range.indexOf(',')>=0) {
					not_cachable = true;
				} else {
					let idx = range.indexOf('-');
					start = parseInt(range.substring(0, idx));
					end = parseInt(range.substring(idx+1));
					//only cache closed ranges
					if (!idx || !end) not_cachable = true;
				}
			}
		});
		if (!host) {
			req.reply = 405;
			req.send('Invalid host name');
			return;
		}
		let hport = req.tls ? 443 : 80;
		let splith = host.split(':');
		if (splith.length>1) {
			hport = parseInt(splith[splith.length-1]);
		}
		if ((host.indexOf("127.0.0.1")>=0) || (host.indexOf("localhost")>=0)) {
			if (hport === use_port) {
				req.reply = 405;
				req.send('Invalid host pointing to this proxy');
				return;
			}
		}
		let use_tls=false;
		if (hport!==443) {
			let url_no_proto = host + req.url;
			let service = all_services.find( s => (url_no_proto.indexOf(s.base_url_no_proto)>=0) );
			if (service && service.force_tls) use_tls = true;
		} else {
			use_tls = true;
		}
		//remove default ports in host
		if (use_tls && (hport == 443)) host = host.replace(":443", "");
		else if (!use_tls && (hport == 80)) host = host.replace(":80", "");

		req.target_url = (use_tls ? 'https://' : 'http://') + host + req.url;
	}
	print(GF_LOG_INFO, `Proxying request ${req.method} for ${req.target_url}`);
	print(GF_LOG_DEBUG, `\tRequest details ${ JSON.stringify(req) }`);

	//used when reading from XHR without cache
	req._read_from_queue = function(ab) {
        if (!req.ab_queue.length) {
            if (req.xhr_done) {
				let bitrate = req.bytes_in_req * 8; // /1000 for kbits but we then need to divide by time/1000 for secs
				let time = sys.clock_ms() - req.start_time;
				if (!time) time=1;
				bitrate = Math.floor(bitrate/time);
				print(GF_LOG_INFO, `Done sending ${req.url} (${req.reply}) from HTTP in ${time} ms at ${bitrate} kbps`);
				if (req.activate_rep_timeout) req.activate_rep_timeout.update();
				return 0;
			}
			return -1;
        }
        let q_ab=req.ab_queue[0];
        let to_read = q_ab.byteLength - req.ab_offset;
        if (to_read > ab.byteLength) to_read = ab.byteLength;
        new Uint8Array(ab, 0, to_read).set(new Uint8Array(q_ab, req.ab_offset, to_read) );
        req.ab_offset += to_read;
        if (req.ab_offset == q_ab.byteLength) {
			req.ab_offset = 0;
			req.ab_queue.shift();
        }
		req.bytes_in_req += to_read;
        return to_read;	
    }

    //used when reading from MABR service
	req._read_from_buffer = function(ab) {
		if (req.is_head || !req.cache_file) return 0;

		let src_ab = req.cache_file.data;
		if (src_ab.byteLength <= req.ab_offset) {
			if (req.cache_file.done) {
				let bitrate = req.bytes_in_req * 8; // /1000 for kbits but we then need to divide by time/1000 for secs
				let time = sys.clock_ms() - req.start_time;
				if (!time) time=1;
				bitrate = Math.floor(bitrate/time);
				print(GF_LOG_INFO, `Done sending ${req.url} (${req.reply}) from ${(req.cache_file.mabr ? 'MABR' : 'HTTP')} cache in ${time} ms at ${bitrate} kbps`);
				req.cache_file.nb_users --;
				req.cache_file = null;
				if (req.activate_rep_timeout) req.activate_rep_timeout.update();
				return 0;
			}
			//waiting for data
			return -1;
		}

		let to_read = src_ab.byteLength - req.ab_offset;
		if (to_read > ab.byteLength) to_read = ab.byteLength;
		new Uint8Array(ab, 0, to_read).set(new Uint8Array(src_ab, req.ab_offset, to_read) );
		req.ab_offset += to_read;
		req.bytes_in_req += to_read;
		return to_read;
    };

	//closed from client
	req.close = function(code) {
		print(GF_LOG_DEBUG, `Client session ${req.url} is closed: ${sys.error_string(code)}`);
		if (req.xhr) {
			if (req.xhr.readyState < 4) req.xhr.abort();
			if (req.service) {
				req.xhr.last_used = sys.clock_ms();
				req.service.xhr_cache.push(req.xhr);
				req.xhr = null;
			}
		}
		if (req.activate_rep_timeout) req.activate_rep_timeout.update();
		req.ab_queue = null;
		if (req.cache_file) req.cache_file.nb_users --;
	};

	req.set_cache_file = function(file) {
		if (this.cache_file) return;
		this.cache_file = file;
		file.nb_users ++;
		this.read = this._read_from_buffer;
		if (file.xhr_status) {
			this.flush_request();
		} else {
			if (!file.pending_reqs) file.pending_reqs = [];
			file.pending_reqs.push(this);
		}
	};
	req.flush_request = function() {
		this.reply = this.cache_file.xhr_status;
		this.headers_out.push( { "name" : 'Content-Type', "value": this.cache_file.mime} );
		if (this.cache_file.range)
			this.headers_out.push( { "name" : 'Content-Range', "value": this.cache_file.range} );
		if (this.cache_file.aborted) {
		} else if (this.cache_file.done) {
			this.headers_out.push( { "name" : 'Content-Length', "value": this.cache_file.data.byteLength} );
		} else {
			this.headers_out.push( { "name" : 'Transfer-Encoding', "value": 'chunked'} );
		}
		this.send();
	};


	req.fetch_unicast = function() {
		//already setup through set_cache_file - typically happens when N requests are pending on MABR file that is later cancelled
		//if source can still be cached, a cache file will be created and assigned to all pending requests for the same url
		if (req.cache_file) return;

		req.ab_queue = [];
		req.manifest_ab = null;
		req.read = req._read_from_queue;

		//DASH/HLS rep, create a cache file for media
		if (active_rep && !not_cachable) {
			req.cache_file = req.service.create_cache_file(req.target_url, "video/mp4", start, end, false);
			req.cache_file.nb_users ++;
			req.read = req._read_from_buffer;
		}
		//DASH/HLS manifest, create a cache file for media
		else if (req.service && req.manifest_type && req.service.mani_cache) {
			req.cache_file = req.service.create_cache_file(req.target_url,(req.manifest_type==MANI_DASH) ? "application/dash+xml" : "application/vnd.apple", start, end, false);
			req.cache_file.nb_users ++;
			req.read = req._read_from_buffer;
		}
		
		req.xhr = null;
		if (req.service && req.service.xhr_cache.length) {
			req.xhr = req.service.xhr_cache.shift();
		}
		if (!req.xhr) 
			req.xhr = new XMLHttpRequest();

		req.xhr.last_used = sys.clock_ms();
		req.xhr.cache = (!req.cache_file && use_cache) ? "normal" : "none";
		req.xhr.responseType = "push";
		req.xhr.onprogress = function (evt) {
			if (!evt) return;
			if (evt.buffer.byteLength) {
				let new_bytes = evt.buffer.slice(0);
				req.ab_queue.push(new_bytes);
				//gather manifest
				if (req.manifest_type) {
					req.manifest_ab = cat_buffer(req.manifest_ab, new_bytes);
				}
				
				//gather cache (can be media or manifest)
				if (req.cache_file) {
					req.cache_file.data = cat_buffer(req.cache_file.data, new_bytes);
				}
			}
		};

		req.xhr.onerror = function() {
			print(GF_LOG_ERROR, `Request failed for ${req.target_url}`);
			if (req.reply) return;
			req.reply = 502;
			req.send();
			if (req.cache_file) {
				req.cache_file.xhr_status = req.reply;
				req.cache_file.aborted = true;
				req.cache_file.done = true;				
				//flush all pending requests that were waiting for XHR status
				if (req.cache_file.pending_reqs) {
					req.cache_file.pending_reqs.forEach(r => r.flush_request() );
					req.cache_file.pending_reqs = null;
				}
			}
		};

		req.xhr.onreadystatechange = function() {
			if (this.readyState == 4) {
				print(GF_LOG_DEBUG, `${req.target_url} received`);
				if (req.manifest_type) {
					let mani = sys.mpd_parse(req.manifest_ab);
					req.manifest_ab = null;
					if (mani) {
						update_manifest(mani, req.target_url);
					}
				}
				//cache can be set for manifests or media 
				if (req.cache_file) {
					req.cache_file.done = true;
					//purge delay applies on received file
					req.cache_file.received = sys.clock_ms();
				}
				req.xhr_done = true;
				//and move to XHR cache
				if (req.service) {
					req.xhr.last_used = sys.clock_ms();
					req.service.xhr_cache.push(req.xhr);
					req.xhr = null;
				}
				return;
			}
			if (this.readyState != 2) return;
			let content_range = null;
			//we got our headers
			let headers = this.getAllResponseHeaders().split('\r\n');
			headers.forEach(n => {
				let h = n.split(': ');
				let hdr = { "name" : h[0], "value": h[1]};
				//filter headers
				let h_name = h[0].toLowerCase();
				if (h_name === 'upgrade') return;
				if (h_name === ':status') return;
				if (h_name === 'content-range') content_range = h[1];

				req.headers_out.push(hdr);
				if (h_name == 'content-type') {
					if ((h[1].indexOf('application/vnd.apple')>=0) || (h[1].indexOf('x-mpegurl')>=0)) {
						req.manifest_type = MANI_HLS;
					} 
					else if (h[1].indexOf('dash+xml')>=0) {
						req.manifest_type = MANI_DASH;
					}
					if (req.cache_file) req.cache_file.mime = h[1];
				}
			}); 
			print(GF_LOG_DEBUG, `Sending reply to ${req.url} code ${req.xhr.status}`);
			req.reply = req.xhr.status;
			req.send();

			if (req.cache_file) {
				req.cache_file.xhr_status = req.reply;
				if (req.reply == 206) req.cache_file.range = content_range;
				//prune if error
				if (req.reply>=400) {
					req.cache_file.aborted = true;
					req.xhr_done = true;
				}
				//flush all pending requests that were waiting for XHR status
				if (req.cache_file.pending_reqs) {
					req.cache_file.pending_reqs.forEach(r => r.flush_request() );
					req.cache_file.pending_reqs = null;
				}
			}
		};
		req.xhr.open(req.method, req.target_url);
		req.xhr.send();	
	};

	req.is_head = (req.method === 'HEAD');
	req.start_time = sys.clock_ms();
	req.bytes_in_req = 0;
	req.cache_file = null;
	req.ab_offset = 0;
	req.ab_queue = null;
	req.data = null;
	req.waiting_mabr = 0;
	req.xhr_done = false;
	req.activate_rep_timeout = null;

	let url_lwr = req.target_url.toLowerCase();
	if (url_lwr.indexOf('.m3u8')>=0) req.manifest_type = MANI_HLS;
	else if (url_lwr.indexOf('.mpd')>=0) req.manifest_type = MANI_DASH;
	else req.manifest_type = 0;

	//locate service
	let period, active_rep;
	[req.service, active_rep, period] = locate_service_quality(req.target_url);

	let purge_delay = req.service ? req.service.purge_delay : def_purge_delay;
	if (!purge_delay) not_cachable = true;

	//example of how to force highest bandwidth - this assumes segment alignment across all reps and bitstream switching
	//not working for HLS as we would need to extract the segment name from the target variant playlist which we may not have
	if (0 && active_rep && !req.service.m3u8) {
		let reps = period.reps.filter(r => (active_rep.as_idx== r.as_idx) && (r!==active_rep) );
		if (reps.length) {
			let force_rep = reps[ reps.length-1];
			let idx = req.target_url.indexOf(active_rep.radical);
			let new_url = req.target_url.substring(0, idx) + force_rep.radical + active_rep.seg_id + active_rep.suffix;
			req.target_url = new_url;
			force_rep.seg_id = active_rep.seg_id;
			force_rep.seg_dur = active_rep.seg_dur;
			active_rep = force_rep;
		}
	}

	if (req.service && req.manifest_type && !req.service.mani_cache) not_cachable = true;

	//look in mem cache
	if (req.service && !not_cachable) {
		let f = req.service.get_file(req.url, start, end);
		//only cache manifest if not too old - for now we hardcode this to 1 sec, we should get this from the manifest
		if (f && req.manifest_type && (f.received + 1000 < sys.clock_ms())) {
			//trash
			req.service.mem_cache.splice(req.service.mem_cache.indexOf(f) , 1);
			print(GF_LOG_DEBUG, `Cached manifest ${req.url} too old, removing and refreshing`);
			f = null;
		}

		if (f) {
			print(GF_LOG_INFO, `File ${req.url} present in cache ${ f.done ? '(completed)' : '(transfer in progress)'}`);
			req.service.mark_active_rep(active_rep, req);
			return req.set_cache_file(f);
		}
	}
	if (!req.service) {
		print(GF_LOG_INFO, `No associated service configured, going for HTTP`);
		req.fetch_unicast();
		return;
	}

	//load MABR service if present
	if (req.service.mabr && !req.service.mabr_loaded) {
		req.service.load_mabr();
	}

	if (active_rep) {
		//mark rep active - this will trigger mcast joining so do this before checking mcast
		req.service.mark_active_rep(active_rep, req);

		//check if we have mabr active
		if (active_rep.live_edge && active_rep.mabr_active) {
			print(GF_LOG_INFO, `Waiting for file ${req.url} from MABR (ID ${req.service.mabr_service_id})`);
			//set a timeout for the request in case MABR fails
			req.waiting_mabr_start = sys.clock_ms();
			if (req.service.repair) {
				req.waiting_mabr = req.waiting_mabr_start + 2*active_rep.seg_dur/3 + MABR_TIMEOUT_SAFETY;
			} else {
				//we gather full files when not in repair, we need a longer timeout
				req.waiting_mabr = req.waiting_mabr_start + 3*active_rep.seg_dur/2 + MABR_TIMEOUT_SAFETY;
			}
			req.service.pending_reqs.push(req);
			return;
		} else if (active_rep.mabr_active) {
			print(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} num ${active_rep.seg_num} is not on live edge ${active_rep.live_seg_num}, going for HTTP`);
		} else if (req.service.mabr_service_id) {
			print(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} has no active multicast, going for HTTP`);
		} else {
			print(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} waiting for multicast bootstrap, going for HTTP`);
		}
	}
	//fetch from unicast
	req.fetch_unicast();
}

function create_service(http_url, force_mcast_activate)
{
	//mabr service is null by default
	let s = {};
	s.url = http_url;
	s.mabr = null;
	s.mabr_loaded = false;
	s.mem_cache = [];
	s.id = all_services.length+1;
	s.mabr_service_id = 0;
	s.source = null;
	s.sink = null;
	s.pending_reqs=[];
	s.active_reps_timeouts=[];
	s.nb_mabr_active = 0;
	s.unload_timeout = 0;
	s.purge_delay = def_purge_delay;
	s.mani_cache = false;
	s.xhr_cache = [];
	s.pending_deactivate = [];
	if (http_url.toLowerCase().startsWith('https://')) s.force_tls = true;
	else s.force_tls = false;

	s.manifest = null;
	//do we have a mcast service for this ?
	let url_noport = http_url.replace(/([^\/\:]+):\/\/([^\/]+):([0-9]+)\//, "$1://$2/");
	let mabr_cfg = services_defs.find(e => e.http == url_noport);
	if (mabr_cfg) {
		s.mabr = mabr_cfg.mcast;
		s.unload = mabr_cfg.unload;
		s.mabr_min_active = mabr_cfg.activate;
		s.purge_delay = 1000 * mabr_cfg.timeshift;
		s.mani_cache = mabr_cfg.mcache;
		s.repair = mabr_cfg.repair;
		s.corrupted = mabr_cfg.corrupted;
		print(GF_LOG_DEBUG, `Service ${http_url} has custom config${ (mabr_cfg.mcast ? ' and MABR' : '')}`);
	}

	//remove cgi and remove resource name
	let str = http_url.split('?');
	str = str[0].split('/')
	str.pop();
	s.base_url = str.join('/') + '/';
	//remove protocol scheme
	s.base_url_no_proto = s.base_url.split('://')[1];

	s.get_file = function(url, start, end)
	{
		let file = null;
		for (let i=0; i<this.mem_cache.length; i++) {
			let f = this.mem_cache[i];
			if (f.aborted) continue;
			if (f.url.indexOf(url)>=0) {}
			//in case the whole server path was not sent in mabr
			else if (f.mabr && (url.indexOf(f.url)>=0)) {}
			else continue;
			//only deal with exact byte ranges - we could optimize to allow sub-ranges
			if (f.start==start && f.end==end) return f;
		}
		return null;	
	};
	s.unqueue_pending_request = function(url, from_mabr)
	{
		let pending = this.pending_reqs.find (r =>{
			if (url.indexOf(r.url)>=0) return true;
			//in case the whole server path was not sent in mabr
			if (from_mabr && (r.url.indexOf(url)>=0) ) return true;
			return false;
		});
		if (pending)
			this.pending_reqs.splice(this.pending_reqs.indexOf(pending), 1);
		return pending;
	};

	s.create_cache_file = function(url, mime, br_start, br_end, from_mabr)
	{
		let file = {};
		file.url = url;
		file.mime = mime || 'video/mp4';
		file.received = sys.clock_ms();
		file.data = new ArrayBuffer();
		this.mem_cache.push(file);
		file.done = false;
		file.aborted = false;
		file.xhr_status = 0;
		file.pending_reqs = null;
		file.nb_users = 0;
		file.start = br_start;
		file.end = br_end;
		file.mabr = from_mabr;
		file.range = null;
		if (from_mabr) {
			file.xhr_status = 200;
		}

		//get any pending request(s) for this file
		while (true) {
			//we use req.url , eg server path, and not target_url which includes server name
			let pending = this.unqueue_pending_request(url, from_mabr);
			if (!pending) break;
			pending.waiting_mabr = 0;
			pending.set_cache_file(file);
			print(GF_LOG_DEBUG, `Found pending request for ${file.url}, canceling timeout`);
		}
		print(GF_LOG_DEBUG, `Start ${file.mabr ? 'MABR ' : ''}reception of ${file.url} mime ${file.mime} - ${this.mem_cache.length} files in service cache`);
		return file;
	};

	s.mark_active_rep = function(rep, request)
	{
		if (!rep || !request) return;
		let use_same_conn = false;
		let connection_id = check_ip ? (request.ip + ':'+request.port) : request.netid;

		let idx = s.pending_deactivate.indexOf(rep);
		if (idx>=0) s.pending_deactivate.splice(idx, 1);

		//mark number of active requests for this quality
		if (!rep.nb_active) {
			print(GF_LOG_INFO, `Service ${this.id} Rep ${rep.ID} is now active`);
		} else {
			//check if we have active requests on the same connection to be deactivated. If so, extend timeout of last found
			//and do not change nb_active
			//this avoids activate/deactivate on same connection which would trigger spurious mcast joins/leaves
			let same_conn = this.active_reps_timeouts.filter(e => e.rep.ID == rep.ID && e.cid == connection_id);
			if (same_conn.length) {
				let rep_to = same_conn[same_conn.length-1];
				rep_to.update();
				print(GF_LOG_DEBUG, `Service ${this.id} same connection used for same quality ${rep.ID}, extending inactive timeout`);
				use_same_conn = true;
			}
		}
		if (!use_same_conn) {
			rep.nb_active++;
			request.activate_rep_timeout = {"rep": rep, "timeout": 0, "cid": connection_id, "mabr_canceled": false};
			request.activate_rep_timeout.update = function() {
				//timeout for active quality is segment duration + safety
				this.timeout = sys.clock_ms() + this.rep.seg_dur + DEACTIVATE_TIMEOUT_MS;
			};
			request.activate_rep_timeout.update();
			this.active_reps_timeouts.push( request.activate_rep_timeout );
		}

		//check if we need to activate multicast
		if (this.mabr_loaded && !rep.mabr_active 
			&& (!this.mabr_min_active || (rep.nb_active>=this.mabr_min_active))
		) {
			this.activate_mabr(rep, true);
		}
	};

	s.activate_mabr = function(rep, do_activate)
	{
		if (!this.mabr_service_id || !this.source || !rep) return;	

		if (do_activate) {
			this.nb_mabr_active++;
			this.unload_timeout = 0;
			print(GF_LOG_INFO, `Service ${this.id} MABR activated for Rep ${rep.ID} - active in service ${this.nb_mabr_active}`);
		} else if (this.nb_mabr_active) {
			this.nb_mabr_active--;
			print(GF_LOG_INFO, `Service ${this.id} MABR deactivated for Rep ${rep.ID} - active in service ${this.nb_mabr_active}`);
			if (!this.nb_mabr_active && this.unload) {
				this.unload_timeout = sys.clock_ms() + 1000*this.unload;
			}
		}

		//no deactivation of multicast channels
		if (this.mabr_min_active==0) {
			rep.mabr_active = true;
			return;	
		}
		let evt = new FilterEvent(GF_FEVT_DASH_QUALITY_SELECT);
		evt.service_id = this.mabr_service_id;
		evt.period_id = rep.period_id;
		evt.as_id = rep.AS_ID;
		evt.rep_id = rep.ID;
		evt.select = do_activate ? 0 : 1;
		rep.mabr_active = do_activate;
		session.fire_event(evt, this.source, false, true);
	};
	
	s.load_mabr = function()
	{
		if (!this.mabr || this.mabr_loaded) return;
		//load routein in no-cache mode, single pid per TSI
		let args = 'src=' + this.mabr;
		if (this.mabr.indexOf('gpac:')<0) args += ':gpac';
		args += ':gcache=0:stsi';
		//multicast is dynamically enabled/disabled, start with all services tuned but disabled
		if (this.mabr_min_active>0) args += ':tunein=-3';
		//add repair option last and escape it
		if (this.repair) args += '::repair_urls='+this.url;
		
		this.source = session.add_filter(args);
		if (!this.source) {
			print(GF_LOG_ERROR, `Service ${this.id} Failed to load MABR demux for ${this.mabr}`);
			return;
		}
		this.source.require_source_id();	
		this.mabr_loaded = true;

		//create custom sink accepting files
		this.sink = session.new_filter("RouteSink"+this.id);
		this.sink.set_cap({id: "StreamType", value: "File", in: true} );
		this.sink.max_pids=-1;
		this.sink.pids=[];
	
		this.sink.configure_pid = function(pid)
		{
			//reconfigure (new file), get previsou file and mar as done
			if (this.pids.indexOf(pid)>=0) {
				//get previous file
				if (pid.url) {
					let file = s.mem_cache.find(f => f.url===pid.url);
					if (file) file.done = true;
				}	
			} else {
				//initial declaration
				this.pids.push(pid);
				//send play
				let evt = new FilterEvent(GF_FEVT_PLAY);
				evt.start_range = 0.0;
				pid.send_event(evt);
				//no repair, we must dispatch full files
				if (!s.repair) pid.framing = true;
			}

			//get new URL for this pid	
			pid.url = pid.get_prop('URL');
			if (pid.url && (pid.url.charAt(0) != '/')) pid.url = '/' + pid.url;
			pid.mime = pid.get_prop('MIMEType');
			if (!s.mabr_service_id) {
				s.mabr_service_id = pid.get_prop('ServiceID');
				print(GF_LOG_INFO, `MABR configured for service ${s.mabr_service_id}`);
			}
			pid.corrupted = false;
			let purl = pid.url.toLowerCase();
			//do not cache HLS/DASH manifests
			if ((purl.indexOf('.m3u8')>=0)|| (purl.indexOf('.mpd')>=0)) pid.do_skip  = true;
			else pid.do_skip = false;
		};
	
		this.push_to_cache = function (pid, pck) {
			let file = this.mem_cache.find(f => f.url===pid.url);
			let corrupted = pck.corrupted ? 1 : 0;
			if (corrupted && pck.get_prop('PartialRepair')) {
				corrupted = s.corrupted ? 0 : 2;
			}
			//file corrupted and no repair, move to HTTP
			if (!s.repair && corrupted) {
				//cache file shall never be created at this point since we only aggregate full files when no repair
				if (file) {
					print(GF_LOG_ERROR, `Service ${this.id} receiving corrupted MABR packet and cache file was already setup, bug in code !`);
				}
				print(GF_LOG_INFO, `Corrupted MABR packet for ${pid.url} (valid container ${corrupted==2}), switching to HTTP`);
				//get any pending request(s) for this file
				while (true) {
					let pending = this.unqueue_pending_request(pid.url, true);
					if (!pending) break;
					//signal mabr was canceled
					pending.waiting_mabr = 0;
					if (pending.activate_rep_timeout) {
						pending.activate_rep_timeout.update();
						pending.activate_rep_timeout.mabr_canceled = true;
					}
					//the first one will create a cache file if possible, associated it to the other pending requests and potentially removing them
					pending.fetch_unicast();
				}
				return;
			} else if (corrupted) {
				print(GF_LOG_WARNING, `MABR Repair failed for ${pid.url} (valid container ${corrupted==2}), broken data sent to client`);
			}
			if (!file) file = this.create_cache_file(pid.url, pid.mime, 0, 0, true);

			print(GF_LOG_DEBUG, `Service ${this.id} receiving MABR packet for ${pid.url} size ${pck.size} end ${pck.end}`);
	
			//reagregate packet 
			if (pck.size)
				file.data = cat_buffer(file.data, pck.data);

			if (pck.start && pck.end) {
				print(GF_LOG_INFO, `Service ${this.id} got MABR file ${pid.url} in one packet`);
				file.done = true;
			}
			else if (pck.start) {
				print(GF_LOG_INFO, `Service ${this.id} start receiving MABR file ${pid.url}`);
			}
			else if (pck.end) {
				print(GF_LOG_INFO, `Service ${this.id} received MABR file ${pid.url}`);
				file.done = true;
			}
		};
	
		this.sink.process = function(pid)
		{
			this.pids.forEach(function(pid) {
				while (1) {
					let pck = pid.get_packet();
					if (!pck) break;
					if (!pid.do_skip && !pid.corrupted)
						s.push_to_cache(pid, pck);
					pid.drop_packet();
				}
			});
		};
		//don't use dash client, we just need to get the files
		this.sink.set_source(this.source);
		print(GF_LOG_INFO, `Service ${this.id} loaded MABR for ${this.mabr}${this.repair ? ' with repair' : ''}`);
	};

	all_services.push(s);
	print(GF_LOG_INFO, `Created Service ID ${s.id} for ${http_url}${s.mabr ? ' with MABR' : ''}`);
	//and load if requested - we force mcast activtaion when an acess to the mpd is first detected
	if (force_mcast_activate && s.mabr) s.load_mabr();
	return s;
}

function do_init()
{
	if (http_out_f == null) {
		print(`Starting HTTP proxy on port ${use_port}`);
		let http = session.add_filter("httpout:port="+use_port+":rdirs=gmem");	
	} else {
		use_port = http_out_f.get_arg("port");
		print(`Attached proxy to server running on port ${use_port}`);
	}
	//preload services
	services_defs.forEach(sd => {
		if (sd.unload) return;
		let s = create_service(sd.http, false);
		if (!s) {
			print(GF_LOG_ERROR, `Failed to create service ${sd.http}`);
		} else {
			s.load_mabr();
		}
	});

	session.post_task( () => {
		let do_gc = false;
		let now = sys.clock_ms();
		all_services.forEach(s => {
			//cleanup mem cache
			for (let i=0; i<s.mem_cache.length; i++) {
				let f = s.mem_cache[i];
				if (f.nb_users) continue;
				if (!f.aborted && (f.received + s.purge_delay >= now)) continue;
				s.mem_cache.splice(i, 1);
				i--;
				print(GF_LOG_DEBUG, `Removing ${f.url} from cache - remain ${s.mem_cache.length}`);
			}

			//check timeout on pending reqs waiting for MABR
			for (let i=0; i<s.pending_reqs.length; i++) {
				let req = s.pending_reqs[i];
				if (!req.waiting_mabr || (req.waiting_mabr>now)) continue;
				print(`MABR timeout for ${req.url} after ${now - req.waiting_mabr_start} ms - fetching from HTTP at ` + req.target_url);
				s.pending_reqs.splice(s.pending_reqs.indexOf(req), 1);
				i--;
				if (req.activate_rep_timeout) {
					req.activate_rep_timeout.update();
					req.activate_rep_timeout.mabr_canceled = true;
				}
				req.waiting_mabr = 0;
				//the first one will create a cache file if possible, associated it to the other pending requests and potentially removing them
				let prev_len = s.pending_reqs.length;
				req.fetch_unicast();
				//if some pending were removed, restart the loop
				if (prev_len != s.pending_reqs.length) {
					i = -1;
				}
			}
			//check reps that could be deactivated after a MABR timeout
			for (let i=0; i<s.pending_deactivate.length; i++) {
				let rep = s.pending_deactivate[i];
				if (rep.mabr_deactivate_timeout < now) continue;
				s.pending_deactivate.splice(i, 1);
				i--;
				print(GF_LOG_INFO, `Service ${s.id} Rep ${rep.ID} no longer on multicast`);
				s.activate_mabr(rep, false);
			}

			//check timeout on active reps
			for (let i=0; i<s.active_reps_timeouts.length; i++) {
				let o = s.active_reps_timeouts[i];
				if (o.timeout > now) break;
				let active_rep = o.rep;
				let mabr_canceled = o.mabr_canceled;
				s.active_reps_timeouts.splice(i, 1);
				i--;
				active_rep.nb_active--;
				if (!active_rep.nb_active) {
					print(GF_LOG_INFO, `Service ${s.id} Rep ${active_rep.ID} is now inactive`);
				} else {
					print(GF_LOG_DEBUG, `Service ${s.id} Rep ${active_rep.ID} is still active (active clients ${active_rep.nb_active})`);
				}
				if (active_rep.mabr_active && s.mabr_min_active && (active_rep.nb_active<s.mabr_min_active) ) {
					if (!mabr_canceled) {
						print(GF_LOG_INFO, `Service ${s.id} Rep ${active_rep.ID} no longer on multicast`);
						s.activate_mabr(active_rep, false);
					} else {
						active_rep.mabr_deactivate_timeout = sys.clock_ms() + 2000;
						s.pending_deactivate.push(active_rep);
					}
				}
			}
			//check timeout on MABR service deactivation
			if (s.unload_timeout && (s.unload_timeout < now)) {
				s.unload_timeout = 0;
				if (!s.nb_mabr_active && s.source) {
					print(GF_LOG_INFO, `Service ${s.id} stopping MABR`);
					session.remove_filter(s.sink);
					session.remove_filter(s.source);
					s.sink = null;
					s.source = null;
					s.mabr_loaded = false;
					do_gc = true;
				}
			}
			//cleanup XHRs
			for (let i=0; i<s.xhr_cache.length; i++) {
				let xhr = s.xhr_cache[i];
				if (xhr.last_used + 10000 >= now) continue;
				s.xhr_cache.splice(i, 1);
				i--;
				do_gc = true;
			}

		});
		if (do_gc) sys.gc();
		if (session.last_task) return false;
		//perform cleanup every 100ms
		return 100;

	}, "proxy_clean");
}

let http_out_f = null;
//catch filter creation
session.set_new_filter_fun( (f) => {
	//bind our custom http logic
	if (f.name == "httpout") {
		http_out_f = f;
		f.bind(httpout);
	}
});


session.post_task( () => {
	do_init();
	return false;
}, "init", 200);

filter.initialize = function() {
	def_purge_delay = filter.timeshift * 1000;
	use_port = filter.port;
	use_cache = filter.gcache;
	check_ip = filter.checkip;

	if (filter.sdesc) {
		try {
			let ab = sys.load_file(filter.sdesc, true);
			services_defs = JSON.parse(ab);
			if (!Array.isArray(services_defs)) throw "Invalid JSON, expecting array";
			services_defs.forEach(sd => {
				if (typeof sd.http != 'string') throw "Missing or invalid http property, expecting string got "+typeof sd.http;
				if (typeof sd.mcast == 'undefined') sd.mcast = null;
				else if (typeof sd.mcast != 'string') throw "Missing or invalid mabr property, expecting string got "+typeof sd.mcast;
				
				if (typeof sd.unload != 'number') sd.unload = DEFAULT_UNLOAD_SEC;
				if (typeof sd.activate != 'number') sd.activate = DEFAULT_ACTIVATE_CLIENTS;
				if (typeof sd.timeshift != 'number') sd.timeshift = filter.timeshift;
				if (typeof sd.mcache != 'boolean') sd.mcache = DEFAULT_MCACHE;
				if (typeof sd.repair != 'boolean') sd.repair = DEFAULT_REPAIR;
				if (typeof sd.corrupted != 'boolean') sd.corrupted = DEFAULT_CORRUPTED;

				sd.http = sd.http.replace(/([^\/\:]+):\/\/([^\/]+):([0-9]+)\//, "$1://$2/");
				//remove default ports
				let url_lwr = sd.http.toLowerCase();
				if (url_lwr.startsWith('https://')) {
					sd.http = sd.http.replace(":443/", "/");
				}
				if (url_lwr.startsWith('http://')) {
					sd.http = sd.http.replace(":80/", "/");
				}
			});
		} catch (e) {
			print(`Failed to load services configuration ${filter.sdesc}: ${e}`);
			sys.exit(1);
		}
	}
	
	//after this, filter object is no longer available in JS since we don't set caps or setup filter.process
};

