import { Sys as sys } from 'gpaccore'
import {XMLHttpRequest} from 'xhr'

globalThis.sys = sys;

let server_ports=0;
let do_quit = false;
let force_quit = false;
let all_requests = [];

const MANI_DASH = 1;
const MANI_HLS = 2;
const HLS_MASTER = 1;
const HLS_VARIANT = 2;
const LIVE_EDGE_MAX_DIST = 5;
const DEACTIVATE_TIMEOUT_MS = 5000;
//how long we will make the client wait - this should be refined depending on whether repair is used (fragments pushed on the fly) or not (full file loaded)
const MABR_TIMEOUT_SAFETY = 1000;

const CACHE_TYPE_HTTP = 0;
const CACHE_TYPE_MOD = 1;
const CACHE_TYPE_MABR = 2;

const DEFAULT_MABR_UNLOAD_SEC = 4;
const DEFAULT_KEEPALIVE_SEC = 4;
const DEFAULT_ACTIVATE_CLIENTS = 1;
const DEFAULT_MCACHE = false;
const DEFAULT_REPAIR = false;
const DEFAULT_CORRUPTED = false;
const DEFAULT_TIMESHIFT = 10;
const DEFAULT_GCACHE = false;
const DEFAULT_CHECKIP = false;

//metadata
filter.set_name("mediaserver");
filter.set_class_hint(GF_FS_CLASS_NETWORK_IO);
filter.set_desc("Media Server");
filter.set_version("1.0");
filter.set_author("GPAC team - (c) Telecom Paris 2024 - license LGPL v2");

let filter_help = `This filter is an HTTP server and proxy for GET and HEAD requests supporting Multicast ABR sources.

This filter does not produce PIDs, it attaches to HTTP output filter.
If no HTTP output filter is specified in the session, the filter will create one.
The file \`.gpac_auth\`, if present in current working directory, will be used for authentication unless \`--rdirs\` is set.

If more options need to be specified for the HTTP output filter, they can be passed as local options or using global options:
EX gpac mediaserver:cors=on --user_agent=MyUser

Although not recommended, a server may be specified explicitly:
EX gpac mediaserver httpout:OPTS
In this case, the first \`httpout\` filter created will be used.

Default request handling of \`httpout\` filter through read / write directories is disabled.

# Services Configuration
The service configuration is set using [-scfg](). It shall be a JSON file containing a single array of services.
Each service is a JSON object with one or more of the following properties:
- id: (string, default null) Service identifier used for logs
- active: (boolean, default true) service is ignored if false
- http: (string, default null) URL of remote service to proxy (either resource name or server path)
- gcache: (boolean, default ${DEFAULT_GCACHE}) use gpac local disk cache when fetching media from HTTP for this service
- local: (string, default null) local mount point of this service
- keepalive: (number, default ${DEFAULT_KEEPALIVE_SEC}) remove the service if no request received for the indicated delay in seconds (0 force service to stay in memory forever)
- mabr: (string, default null) address of multicast ABR source for this service
- timeshift: (number, default ${DEFAULT_TIMESHIFT}) time in seconds a cached file remains in memory
- unload: (number, default ${DEFAULT_MABR_UNLOAD_SEC} multicast unload policy
- activate: (number, default ${DEFAULT_ACTIVATE_CLIENTS}) multicast activation policy
- repair: (boolean, default ${DEFAULT_REPAIR}) enable unicast repair in MABR stack
- mcache: (boolean, default ${DEFAULT_MCACHE}) cache manifest files (experimental)
- corrupted: (boolean, default ${DEFAULT_CORRUPTED}) forward corrupted files if parsable (valid container syntax, broken media)
- check_ip: (boolean, , default ${DEFAULT_CHECKIP} monitor IP address and port rather than connection when tracking active clients
- noproxy: (boolean) disable proxy for service when local mount point is set. Default is \`true\` if both \`local\` and \`http\` are set, \`false\` otherwise
- sources: (array, default null) list of sources objects for file-only services. Each source has the following property:
- name: name as used in resource path,
- url: local or remote URL to use for this resource.
- js: (string, default null) built-in or custom request resolver

Any JSON object with the property \`comment\` set will be ignored.

Not all properties are used for each type of service and other properties can be defined by custom request resolvers.

# Proxy versus Server
All services using \`http\` option can be exposed by the server without exposing the origin URL, rather than being proxied. To enable this, the \`local\` service configuration option must be set to:
- the exposed server path, in which case manifest names are not rewritten
- or the exposed manifest path, in which case manifest names are rewritten, but only one manifest can be exposed (does not work with dual MPD and M3U8 services)

EX { 'http': 'https://test.com/live/dash/live.mpd', 'local': '/service1/'}
The server will translate any request \`/service1/foo/bar.ext\` into \`https://test.com/live/dash/foo/bar.ext\`.

EX { 'http': 'https://test.com/live/dash/live.mpd', 'local': '/service1/manifest.mpd'}
The server will translate:
- request \`/service1/manifest.mpd\` into \`https://test.com/live/dash/live.mpd\`
- any request \`/service1/foo/bar.ext\` into \`https://test.com/live/dash/foo/bar.ext\`

Note: The URL must point to a self-contained subdirectory of the remote site. Any URLs outside this directory will either fail or be resolved as absolute path on the remote site.

When \`local\` is not set, these services are always acting as proxies for the \`http\` URL.

When \`noproxy\` is explicitly set to false for the services with both \`http\` and \`local\`, the remote URL will be available as a proxy service as well.

# HTTP Proxy and Relay
The server can act as a proxy for HTTP requests, either for any requests or by domain or resource name.

__Service configuration parameters used :__ \`http\` (mandatory), \`gcache\`, \`local\`.

Configuration for activating proxy for a specific network path:
EX { 'http': 'https://test.com/video/'}

Configuration for activating proxy for any network path:
EX { 'http': '*'}

Configuration for a relay on a given path:
EX { 'http': 'https://test.com/some/path/to/video/', 'local': '/myvids/'}

This will resolve any request \`http://localhost/myvids/*\` to \`https://test.com/some/path/to/video/*\`

Note: The requests are never cached in memory in this mode, but can be cached on disk if \`gcache\` is set.

# HTTP Streaming Cache
The server can act as a cache for live HTTP streaming sessions. The live edge can be cached in memory for a given duration.

__Service configuration parameters used :__ \`http\` ( mandatory), \`timeshift\`, \`mcache\`, \`gcache\`, \`keepalive\` and \`local\`.

Configuration for proxying while caching a live HTTP streaming service:
EX { 'http': 'https://test.com/dash/live.mpd', 'timeshift': '30' }

Configuration for relay caching a live HTTP streaming service:
EX { 'http': 'https://test.com/dash/live.mpd', 'timeshift': '30', 'local': '/myservice/test.mpd'}

The \`local\` service configuration option can be set to:
- the exposed server path, in which case manifest names are not rewritten
- or the exposed manifest path, in which case manifest names are rewritten, but only one manifest can be exposed (does not work with dual MPD and M3U8 services)

# Multicast ABR Gateway
The server can be configured to use a multicast ANR source for an HTTP streaming service, without any HTTP source.

__Service configuration parameters used :__ \`mabr\` (mandatory), \`local\` (mandatory), \`corrupted\`, \`timeshift\` and \`keepalive\`.

The multicast source can be DVB-MABR (e.g. \`mabr://235.0.0.1:1234/\`), ATSC3.0 (e.g. \`atsc://\`) or ROUTE (e.g. \`route://235.0.0.1:1234/\`).
- If the multicast is replayed from a file, netcap ID shall be set in this multicast URL (e.g. \`:NCID=N\`).
- If a specific IP interface is used, it can also be set in multicast URL (e.g. \`:ifce=IP\`).

For example, with \`local\` set to \`/service/live.mpd\` with \`mabr\` set, the server will expose the multicast service as \`http://localhost/service/live.mpd\`.
The manifest name can be omitted, in which case the exact manifest name used in the broadcast shall be used (and known to the client).

Configuration for exposing a MABR session:
EX { 'mabr': 'mabr://234.0.0.1:1234', 'local': '/service1', 'timeshift': '30' }

# Multicast ABR Gateway with HTTP cache
The server can be configured to use a multicast source as an alternate data source of a given HTTP streaming service.

__Service configuration parameters used :__ \`http\` (mandatory), \`mabr\` (mandatory), \`local\`, \`corrupted\`, \`timeshift\`, \`repair\`, \`gcache\`, \`mcache\`, \`unload\`, \`active\`, \`keepalive\` and \`js\`.

The multicast service can be dynamically loaded at run-time using the \`unload\` service configuration option:
- if 0, the multicast is started when loading the server and never ended,
- otherwise, the multicast is started dynamically and ended \`unload\` seconds after last deactivation.

The qualities in the multicast service can be dynamically activated or deactivated using the \`activate\` service configuration option:
- if 0, multicast streams are never deactivated,
- otherwise, a multicast representation is activated only if at least \`active\` clients are consuming it, and deactivated otherwise.

The multicast service can use repair options of the MABR stack using \`repair\` service configuration option:
- if false, the file will not be sent until completely received (this increases latency),
- otherwise, file data will be pushed as soon as available in order (after reception or repair).

If the \`corrupted\` option is set together with \`repair\`, HTTP-based repair is disabled and corrupted files are patched using the \`repair=strict\` mode of the \`routein\` filter.
If files are completely lost, they will be fetched from \`http\`source.
Warning: This may likely result in decoding/buffering pipeline errors and could fail with some players expecting no timeline holes (such as browsers). GPAC supports this.

The number of active clients on a given quality is computed using the client connection state: any disconnect/reconnect from a client for the same quality will trigger a deactivate+activate sequence.
If \`check_ip\` is set to true, the remote IP address+port are used instead of the connection. This however assumes that each client has a unique IP/port which may not always be true (NATs).

If \`timeshift\` is 0 for the service, multicast segments will be trashed as soon as not in use (potentially before the client request).

Note: Manifests files coming from multicast are currently never cached.

Configuration for caching a live HTTP streaming service with MABR backup:
EX { 'http': 'https://test.com/dash/live.mpd', 'mabr': 'mabr://234.0.0.1:1234', 'timeshift': '30'}

For such services, the custom HTTP header \`X-From-MABR\` is defined:
- for client request, a value of \`no\` will disable MABR cache for this request; if absent or value is \`yes\`, MABR cache will be used if available
- for client response, a value of \`yes\` indicates the content comes from the MABR cache; if absent or value is \`no\`, the content comes from HTTP


The \`js\` option can be set to a JS module exporting the following functions:
- init : (mandatory) The function is called once at the start of the server. Parameters:
  - scfg: the service configuration object
  - return value: must be true if configuration and initialization are successful, false otherwise.

- service_activation : (optional) The function is called when the service is activated or deactivated. Parameters:
  - do_activate (boolean): if true, service is being loaded otherwise it is being unloaded
  - return value: none

- quality_activation : (mandatory) The function is called when the given quality is to be activated service is activated or deactivated. Parameters (in order):
  - do_activate (boolean): if true, quality is being activated otherwise it is being deactivated
  - service_id (integer): ID of the service as announced in the multicast
  - period_id (string): ID of the DASH Period, ignored for HLS
  - adaptationSet_ID (integer): ID of the DASH AdaptationSet, ignored for HLS
  - representation_ID (string): ID of the DASH representation or name of the HLS variant playlist
  - return value: shall be true if activation/deactivation shall proceed and false if activation/deactivation shall be canceled.

# File Services
A file system directory can be exposed as a service. 

__Service configuration parameters used :__ \`local\` (mandatory), \`sources\` (mandatory), \`gcache\` and \`keepalive\`.

The \`local\` service configuration option must be set to the desired service path, and the \`sources\` service configuration option must one or more valid sources.
Each source is either a file, a directory or a remote URL.

Configuration for exposing a directory:
EX { 'local': '/dserv/', 'sources': [ { 'name': 'foo/', 'url': 'my_dir/' } ] }
This service will expose the content of directory \`my_dir/*\` as \`http://localhost/dserv/foo/*\`.

In this mode, file serving is handled directly by httpout filter and no memory caching is used.
If the source is a remote HTTP one, the \`gcache\` option will indicate if GPAC local cache shall be used.

# Module development

A JS module can be specified using the \`js\` option in the service configuration. The module export functions are:
## init (mandatory)
The function is called once at the start of the server
Parameter: the service configuration object
return value must be true if configuration and initialization are successful, false otherwise

## resolve (mandatory)
Parameter: an HTTP request object from GPAC

The function returns an array of two values \`[result, delay]\`:
- result: null if error, a resolved string indicating either a local file or the reply body, or an object
- delay: if true, the reply is being delayed by the module for later processing

When an object is returned, the request is handled by the JS module in charge of sending the reply and reading the data. The object shall have the following properties:
- read: same semantics as the request read method
- on_close: optional function called when the request is closed

# Built-in modules


`;


filter.set_arg({ name: "scfg", desc: "service configuration file", type: GF_PROP_STRING} );
filter.set_arg({ name: "quit", desc: "exit server once last service has been deactivated", type: GF_PROP_BOOL, def: "false"} );

let services_defs = [];

let all_services = [];

function do_log(level, msg)
{
	if (arguments.length==3) print(level, `${arguments[2]}: ${msg}`);
	else print(level, msg);
}

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

function update_manifest(manifest_ab, target_url, mabr_service)
{
	let mani = sys.mpd_parse(manifest_ab);
	if (!mani) return;
	
	let hls_active_rep = null;
	let service = mabr_service ? mabr_service :  all_services.find(s => {
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
			do_log(GF_LOG_ERROR, `Failed to locate service for HLS variant ${target_url}`);
			return;
		}
		service = create_service(target_url, true, null);
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
			do_log(GF_LOG_ERROR, `Service ${service.id} cannot find rep for target ${target_url}`);		
			return;
		}
		hls_active_rep.segments = mani.segments;
		hls_active_rep.hls_seq_start = mani.seq_start;
		hls_active_rep.live_seg_num = mani.live_seg_num;
		service.manifest.live = mani.live;

		do_log(GF_LOG_DEBUG, `Service ${service.id} updated HLS manifest`); // + JSON.stringify(service.manifest));
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
	do_log(GF_LOG_DEBUG, `Service ${service.id} manifest ${ (updated ? 'updated' : 'received')} for ${target_url}`); // + ': ' + JSON.stringify(service.manifest));
}

function locate_service(target_url)
{
	let service = null;
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
	return service;
}

function locate_service_quality(target_url, in_service)
{
	let service = in_service;
	let with_base_url = null;
	if (!service) {
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
	}
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
	do_log(GF_LOG_DEBUG, `Located service ${service.id} for URL ${seg_name}${(rep ? (' rep: ' + rep.ID ) : '' )}`);
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
globalThis.cat_buffer = cat_buffer;


//custom HTTPout request handler 
let httpout = {};
httpout.on_request = (req) => 
{
	//use pre-authentifcation done by server
	if (req.auth_code!=200) {
		req.reply = req.auth_code;
		req.send();
		return;
	}

	//not yet supported
	if ((req.method !== 'GET') && (req.method !== 'HEAD')) {
		req.reply = 405;
		req.send('Method not allowed by server');
		return;
	}

	do_log(GF_LOG_DEBUG, `Got request ${req.method} for ${req.url}`);

	//setup request
	req.target_url = null;
	req.is_head = (req.method === 'HEAD');
	req.start_time = sys.clock_ms();
	req.bytes_in_req = 0;
	req.cache_file = null;
	req.ab_offset = 0;
	req.ab_queue = null;
	req.ab_total_bytes = 0;
	req.data = null;
	req.waiting_mabr = 0;
	req.xhr_done = false;
	req.activate_rep_timeout = null;
	req.xhr = null;
	req.br_start=0;
	req.br_end=0;
	req.from_cache=false;
	req.no_cache=false;
	req.do_cache = false;
	req.jsmod=null;
	req.up_rate=0;

	all_requests.push(req);

	req.get_down_rate = function() {
		let bitrate = this.bytes_in_req * 8; // /1000 for kbits but we then need to divide by time/1000 for secs
		let time = sys.clock_ms() - this.start_time;
		if (!time) time=1;
		return [Math.floor(1000*bitrate/time), time];
	}
	req.get_up_rate = function() {
		return this.up_rate;
	}
	req.on_done = function() {
		let bitrate, time;
		[bitrate, time] = this.get_down_rate();
		bitrate = Math.floor(bitrate/1000);
		let orig = 'HTTP'
		if (this.from_cache) {
			orig = 'local cache';
		} else if (this.cache_file) {
			orig = ((this.cache_file.cache_type==CACHE_TYPE_MABR) ? 'MABR' : 'HTTP') +' cache';
		} else if (this.jsmod) {
			orig = 'JS module';
		}
		do_log(GF_LOG_INFO, `Done sending ${this.url} (${this.reply}) from ${orig} in ${time} ms at ${bitrate} kbps (${this.bytes_in_req} bytes)`);
	};

	//used when reading from JS module
	req._read_from_mod = function(ab) {
		let res = this.jsmod.read(ab);
		if (res==0) {
			this.on_done();
			return 0;
		}
		if (res<0) return res;
		this.bytes_in_req += res;
		return res;
	}
	//used when reading from XHR without cache
	req._read_from_queue = function(ab) {
        if (!this.ab_queue.length) {
            if (this.xhr_done) {
				this.on_done();
				if (this.activate_rep_timeout) this.activate_rep_timeout.update();
				return 0;
			}
			return -1;
        }
        let q_ab=this.ab_queue[0];
        let to_read = q_ab.byteLength - this.ab_offset;
        if (to_read > ab.byteLength) to_read = ab.byteLength;
        new Uint8Array(ab, 0, to_read).set(new Uint8Array(q_ab, this.ab_offset, to_read) );
        this.ab_offset += to_read;
        if (this.ab_offset == q_ab.byteLength) {
			this.ab_offset = 0;
			this.ab_queue.shift();
        }
		this.bytes_in_req += to_read;
        return to_read;	
    }

    //used when reading from MABR service or cached file
	req._read_from_buffer = function(ab) {
		if (this.is_head || !this.cache_file) return 0;

		let src_ab = this.cache_file.data;
		if (src_ab.byteLength <= this.ab_offset) {
			if (this.cache_file.done) {
				this.on_done();
				this.cache_file.nb_users --;
				this.cache_file = null;
				if (this.activate_rep_timeout) this.activate_rep_timeout.update();
				return 0;
			}
			//waiting for data
			return -1;
		}

		let to_read = src_ab.byteLength - this.ab_offset;
		if (to_read > ab.byteLength) to_read = ab.byteLength;
		new Uint8Array(ab, 0, to_read).set(new Uint8Array(src_ab, this.ab_offset, to_read) );
		this.ab_offset += to_read;
		this.bytes_in_req += to_read;
		return to_read;
    };

	req._read_abort = function(ab) {
		return 0;
	};

	//end of session
	req.close = function(code) {
		if (code<0) 
			do_log(GF_LOG_DEBUG, `Client session ${this.url} is closed: ${sys.error_string(code)}`);

		let ridx = all_requests.indexOf(this);
		if (ridx>=0) all_requests.splice(ridx, 1);
		if (do_quit && !all_requests.length && !all_services.length) force_quit = true;
		if (this.jsmod && typeof this.jsmod.on_close == 'function') this.jsmod.on_close();
		this.jsmod = null;

		this.ab_queue = null;
		if (this.cache_file) this.cache_file.nb_users --;

		if (this.activate_rep_timeout) this.activate_rep_timeout.update();
		else if (req.service && req.service.keepalive && !req.service.mabr) {
			let timeout = (code<GF_OK) ? 100 : 1000*req.service.keepalive;
			req.service.unload_timeout = sys.clock_ms() + timeout;
		}
		//we have an XHR, abort it if needed and move to XHR cache for later reuse
		if (this.xhr) {
			if (this.xhr && (this.xhr.readyState < 4)) this.xhr.abort();
			if (this.service) {
				this.xhr.last_used = sys.clock_ms();
				this.service.xhr_cache.push(this.xhr);
				this.xhr = null;
			}
		}
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
		if (file.pid) file.pid.deactivate_timeout = 0;
	};
	req.flush_request = function() {
		this.reply = this.cache_file.xhr_status;
		this.headers_out.push( { "name" : 'Content-Type', "value": this.cache_file.mime} );
		if (this.cache_file.headers)
			this.cache_file.headers.forEach( hdr => this.headers_out.push(hdr) );
		if (this.cache_file.range)
			this.headers_out.push( { "name" : 'Content-Range', "value": this.cache_file.range} );
		if (this.cache_file.aborted) {
		} else if (this.cache_file.done) {
			this.headers_out.push( { "name" : 'Content-Length', "value": this.cache_file.data.byteLength} );
		} else {
			this.headers_out.push( { "name" : 'Transfer-Encoding', "value": 'chunked'} );
		}
		if (this.cache_file.cache_type==CACHE_TYPE_MABR) {
			this.headers_out.push( { "name" : 'X-From-MABR', "value": 'yes'} );
		}
		this.send();
	};


	req.fetch_unicast = function() {
		//already setup through set_cache_file - typically happens when N requests are pending on MABR file that is later cancelled
		//if source can still be cached, a cache file will be created and assigned to all pending requests for the same url
		if (this.cache_file) return;

		this.ab_queue = [];
		this.manifest_ab = null;
		this.read = this._read_from_queue;

		//DASH/HLS rep, create a cache file for media
		if (active_rep && !this.no_cache) {
			this.cache_file = this.service.create_cache_file(this.target_url, "video/mp4", this.br_start, this.br_end, CACHE_TYPE_HTTP);
			this.cache_file.nb_users ++;
			this.read = this._read_from_buffer;
		}
		//DASH/HLS manifest, create a cache file for media
		else if (this.service && this.manifest_type && this.service.mani_cache) {
			this.cache_file = this.service.create_cache_file(this.target_url,(this.manifest_type==MANI_DASH) ? "application/dash+xml" : "application/vnd.apple", this.br_start, this.br_end, CACHE_TYPE_HTTP);
			this.cache_file.nb_users ++;
			this.read = this._read_from_buffer;
		}
		
		this.xhr = null;
		if (this.service && this.service.xhr_cache.length) {
			this.xhr = this.service.xhr_cache.shift();
		}
		if (!this.xhr) 
			this.xhr = new XMLHttpRequest();

		this.xhr.last_used = sys.clock_ms();
		let do_cache = false;
		if (!this.cache_file) {
			do_cache = req.do_cache;
		}
		this.xhr.cache = do_cache ? "normal" : "none";
		this.xhr.responseType = "push";
		this.xhr.onprogress = function (evt) {
			if (!evt) return;
			if (evt.buffer.byteLength) {
				let new_bytes = evt.buffer.slice(0);
				req.ab_queue.push(new_bytes);
				req.ab_total_bytes += new_bytes.byteLength;
				req.up_rate = evt.bps;

				//gather manifest
				if (req.manifest_type) {
					req.manifest_ab = cat_buffer(req.manifest_ab, new_bytes);
				}				
				//gather cache (can be media or manifest)
				if (req.cache_file) {
					req.cache_file.data = cat_buffer(req.cache_file.data, new_bytes);
				}
				let client_rate, req_time;
				[client_rate, req_time] =  req.get_down_rate();
				let buf_size = req.ab_total_bytes - req.bytes_in_req;
				//do_log(GF_LOG_DEBUG, `Up rate ${evt.bps} bps - down rate ${client_rate} bps ${buf_size} queued bytes`);
				if (buf_size > 1000000) return 10000;
				else if (buf_size > 200000) return client_rate;
				return 0;
			}
		};

		this.xhr.onerror = function() {
			do_log(GF_LOG_ERROR, `Request failed for ${req.target_url}`);
			if (req.reply) return;
			req.reply = 502;
			req.send();
			req.read = req._read_abort;
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
				do_log(GF_LOG_DEBUG, `${req.target_url} received`);
				if (req.manifest_type) {
					update_manifest(req.manifest_ab, req.target_url, req.service);
					req.manifest_ab = null;
				}
				//cache can be set for manifests or media 
				if (req.cache_file) {
					do_log(GF_LOG_INFO, `${req.target_url} closing cache file`);
					
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
			if (req.cache_file) req.cache_file.headers = [];
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
				if (req.cache_file) {
					//cache some headers
					if ((h_name !== 'content-length')
						&& (h_name !== 'transfer-encoding')
						&& (h_name !== 'server')
						&& (h_name !== 'date')
					) {
						req.cache_file.headers.push(hdr);
					}
				}

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
			do_log(GF_LOG_DEBUG, `Sending reply to ${req.url} code ${req.xhr.status}`);
			req.reply = req.xhr.status;
			req.send();

			if (req.cache_file) {
				req.cache_file.xhr_status = req.reply;
				if (req.reply == 206) req.cache_file.range = content_range;
				//prune if error
				if (req.reply>=400) {
					req.cache_file.headers = null;
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
		this.xhr.open(this.method, this.target_url);
		this.xhr.send();	
	};

	//preprocess headers to locate host and extract range
	let disable_mabr_cache = false;
	let host = null;
	let referer = null;
	req.headers_in.forEach(h => {
		let hlwr = h.name.toLowerCase();
		if (hlwr == "host") host = h.value;
		if (hlwr == "referer") referer = h.value;
		else if (hlwr == ":authority") host = h.value;
		else if ((hlwr == "x-from-mabr") && (h.value.toLowerCase() == 'no')) disable_mabr_cache = true;
		else if (hlwr == "range") {
			let hdr_range = h.value.split('=');
			let range = [1];
			if (range.indexOf(',')>=0) {
				req.no_cache = true;
			} else {
				let idx = range.indexOf('-');
				req.br_start = parseInt(range.substring(0, idx));
				req.br_end = parseInt(range.substring(idx+1));
				//only cache closed ranges
				if (!idx || !req.br_end) req.no_cache = true;
			}
		}
	});

	let check_any_allowed = true;
	//support http://myhost/http://url - not really standard
	if (!req.service &&
		(req.url.startsWith('/http://')
		|| req.url.startsWith('/http%3A%2F%2F')
		|| req.url.startsWith('/https://')
		|| req.url.startsWith('/https%3A%2F%2F')
	)) {
		req.target_url = req.url.substring(1);
		if ((target_url.indexOf("127.0.0.1")>=0) || (target_url.indexOf("localhost")>=0)) {
			req.reply = 405;
			req.send('Must use Host for localhost proxying');
			return;
		}
	}
	//look in mount points
	if (!req.target_url) {
		let host = null;
		let full_name = true;
		let service_def = services_defs.find( s => (s.local && (req.url.indexOf(s.local)==0) ) );
		if (!service_def) {
			service_def = services_defs.find( s => (s.local_base && (req.url.indexOf(s.local_base)==0) ) );
			full_name = false;
		}
		//special case for MABR services, check we are loaded
		if (service_def && service_def.mabr) {
			req.service = locate_service(req.url);
			//if http source, only start once we fetch the manifest, otherwise start now
			if (!req.service && !service_def.http)
				req.service = create_service(null, true, service_def);
			req.target_url = req.url;
			//fall through
		}
		//for other services, format target url
		if (service_def && !req.service) {
			check_any_allowed = false;
			req.do_cache = service_def.gcache;
			if (service_def.http) {
				if (full_name) {
					req.target_url = req.url.replace(service_def.local, service_def.http);
				} else {
					req.target_url = req.url.replace(service_def.local_base, service_def.http_proto + '://' + service_def.http_host + service_def.http_path);
				}
			} else {
				req.target_url = req.url;
			}
		} 

		if (!service_def && referer) {
			let src = referer.split('://');
			src = src[(src.length>1) ? 1 : 0];
			src = src.split('/');
			src.shift();
			src = '/'+src.join('/');
			service_def = services_defs.find( s => (s.local_base && (src.indexOf(s.local_base)==0) ) );
			if (service_def) {
				check_any_allowed = false;
				req.do_cache = service_def.gcache;
				req.target_url = service_def.http_proto + '://' + service_def.http_host + req.url;
			}
		}

		//local dir or custom JS
		if (service_def && !service_def.http && !service_def.mabr) {
			//custom JS handler
			if (service_def.js_mod) {
				let resolved, delayed;
				[resolved, delayed ] = service_def.js_mod.resolve(req);

				if (!resolved) {
					if (!req.reply) {
						req.reply = 404;
						req.send();
					}
					return;	
				}
				if (delayed) return;

				if (typeof resolved == 'string') {
					if (req.from_cache) {
						req.read = req._read_from_buffer;
						req.target_url = resolved;
					} else if (resolved.indexOf('http')>=0) {
						req.target_url = resolved;
					} else if (typeof resolved == 'string') {
						req.body = resolved;
						req.send();
						return;	
					}
				} else if (typeof resolved == 'object') {
					req.jsmod = resolved;
					if (typeof resolved.read == 'function') {
						req.read = req._read_from_mod;
						return;
					}
					//otherwise we read from cache and module will populate it
					req.read = req._read_from_buffer;
					req.from_cache = true;
				} else {
					req.reply = 501;
					req.send('Invalid module resolution');
					return;
				}
			}
			//custom mount point
			else if (service_def.sources.length && service_def.local_base) {
				let my_url = req.url.replace(service_def.local_base, '');
				let e = service_def.sources.find( a => (a.name == my_url) );
				if (!e) e = service_def.sources.find( a => my_url.startsWith(a.name) );
				if (!e) {
					req.reply = 404;
					req.send();
					return;
				}
				if (e.url.startsWith('http')) {
					req.target_url = e.url;
				} else if (e.url.indexOf('://')>=0) {
					req.reply = 501;
					req.send('Protocol not supported');
					return;
				} else {
					my_url = my_url.replace(e.name, e.url);
					if (sys.file_exists(my_url)) {
						req.body = my_url;
						req.send();
						return;
					} else {
						req.reply = 404;
						req.send();
						return;	
					}
				}
			}
		}
		if (req.target_url && !req.from_cache && check_any_allowed) {
			//rewrite request url
			req.url = req.target_url.substring(req.target_url.indexOf('/', 8));
		}
	}
	//running as real proxy, check host
	if (!req.target_url) {
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
			let e = server_ports.find( e => e == hport);
			if (e) {
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
	if (! req.target_url) {
		req.reply = 404;
		req.send('No such service');
		return;
	}

	do_log(GF_LOG_INFO, `Processing request ${req.method} for ${req.url} (resolved ${req.target_url})`);

	let url_lwr = req.target_url.toLowerCase();
	if (url_lwr.indexOf('.m3u8')>=0) req.manifest_type = MANI_HLS;
	else if (url_lwr.indexOf('.mpd')>=0) req.manifest_type = MANI_DASH;
	else req.manifest_type = 0;

	//locate service
	let period, active_rep;
	[req.service, active_rep, period] = locate_service_quality(req.target_url, req.service);

	let purge_delay = req.service ? req.service.purge_delay : 0;
	if (!purge_delay) req.no_cache = true;

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

	if (!req.service && check_any_allowed) {
		print('check any allowed');
		//pure proxy, check allowed domains
		let service_def = services_defs.find( s => (!s.noproxy && (host === s.http_host) && req.url.startsWith(s.http_path)) );
		if (!service_def) service_def = services_defs.find( s => (s.http === '*') );
		if (!service_def) {
			do_log(GF_LOG_ERROR, `Proxying of ${req.url} not allowed`);
			req.reply = 404;
			req.send('Requested service is not exposed by server');
			return;
		}
		req.do_cache = service_def.gcache;
	}
	
	if (req.service) {
		req.do_cache = req.service.gcache;
		//deactivate timeout, will be reactivated when removing reques
		req.service.unload_timeout = 0;
		if (req.manifest_type && !req.service.mani_cache && !req.from_cache) req.no_cache = true;
	}

	//look in mem cache
	if (req.service && !req.no_cache) {
		let f = req.service.get_file(req.from_cache ? req.target_url : req.url, req.br_start, req.br_end);
		//only cache manifest if not too old - for now we hardcode this to 1 sec, we should get this from the manifest
		if (f && !f.sticky && req.manifest_type && (f.received + 1000 < sys.clock_ms())) {
			//trash
			req.service.mem_cache.splice(req.service.mem_cache.indexOf(f) , 1);
			do_log(GF_LOG_DEBUG, `Cached manifest ${req.url} too old, removing and refreshing`);
			f = null;
		}

		if (f) {
			do_log(GF_LOG_INFO, `File ${req.url} present in cache ${ f.done ? '(completed)' : '(transfer in progress)'}`);
			req.service.mark_active_rep(active_rep, req);
			return req.set_cache_file(f);
		}
	}
	if (!req.service) {
		if (req.target_url.startsWith('http')<0) {
			req.reply = 404;
			req.send('invalid path');
			return;
		}
		do_log(GF_LOG_INFO, `No associated service configured, going for HTTP`);
		req.fetch_unicast();
		return;
	}

	//load MABR service if present
	if (req.service.mabr && !req.service.mabr_loaded && !disable_mabr_cache) {
		req.service.load_mabr();
	}
	//multicast service only
	if (req.service.mabr && !req.service.url && !active_rep && !disable_mabr_cache) {
		req.waiting_mabr_start = sys.clock_ms();
		req.waiting_mabr = req.waiting_mabr_start + 4*MABR_TIMEOUT_SAFETY;
		req.service.pending_reqs.push(req);
		return;
	}
	if (req.from_cache) {
		req.service.pending_reqs.push(req);
		return;
	}

	if (active_rep) {
		//mark rep active - this will trigger mcast joining so do this before checking mcast
		req.service.mark_active_rep(active_rep, req);

		if (disable_mabr_cache) {
			do_log(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} disbaled MABR for this request, going for HTTP`);
		}
		else if (req.service && req.service.mabr_failure) {
			do_log(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} MABR setup failed, going for HTTP`);
		}
		//check if we have mabr active
		else if (active_rep.live_edge && active_rep.mabr_active) {
			if (req.service.source && req.service.source.probe_url(req.service.mabr_service_id, req.url)) {
				req.waiting_mabr = 0;
				do_log(GF_LOG_INFO, `Waiting for file ${req.url} from MABR (ID ${req.service.mabr_service_id} (announced in multicast))`);
			} else {
				do_log(GF_LOG_INFO, `Waiting for file ${req.url} from MABR (ID ${req.service.mabr_service_id})`);
				//set a timeout for the request in case MABR fails
				req.waiting_mabr_start = sys.clock_ms();
				if (req.service.repair) {
					req.waiting_mabr = req.waiting_mabr_start + 2*active_rep.seg_dur/3 + MABR_TIMEOUT_SAFETY;
				} else {
					//we gather full files when not in repair, we need a longer timeout
					req.waiting_mabr = req.waiting_mabr_start + 3*active_rep.seg_dur/2 + MABR_TIMEOUT_SAFETY;
				}
			}
			req.service.pending_reqs.push(req);
			return;
		} else if (active_rep.mabr_active) {
			do_log(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} num ${active_rep.seg_num} is not on live edge ${active_rep.live_seg_num}, going for HTTP`);
		} else if (req.service.mabr_service_id) {
			do_log(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} has no active multicast, going for HTTP`);
		} else if (req.service.mabr) {
			do_log(GF_LOG_INFO, `Rep ${active_rep.ID} seg ${req.url} waiting for multicast bootstrap, going for HTTP`);
		}
	}
	//fetch from unicast
	req.fetch_unicast();
}

function create_service(http_url, force_mcast_activate, forced_sdesc)
{
	//mabr service is null by default
	let s = {};
	s.url = http_url;
	s.mabr = null;
	s.local = null;
	s.mabr_loaded = false;
	s.mabr_failure = false;
	s.mem_cache = [];
	s.id = all_services.length+1;
	s.mabr_service_id = 0;
	s.source = null;
	s.sink = null;
	s.pending_reqs=[];
	s.active_reps_timeouts=[];
	s.nb_mabr_active = 0;
	s.mabr_unload_timeout = 0;
	s.unload_timeout = 0;
	s.purge_delay = 0;
	s.mani_cache = false;
	s.gcache = false;
	s.check_ip = DEFAULT_CHECKIP;
	s.keepalive = DEFAULT_KEEPALIVE_SEC;
	s.xhr_cache = [];
	s.pending_deactivate = [];
	if (http_url && http_url.toLowerCase().startsWith('https://')) s.force_tls = true;
	else s.force_tls = false;
	s.hdlr = null;
	s.manifest = null;
	//do we have a mcast service for this ?
	let url_noport = forced_sdesc ? null : http_url.replace(/([^\/\:]+):\/\/([^\/]+):([0-9]+)\//, "$1://$2/");
	let serv_cfg = forced_sdesc ? forced_sdesc : services_defs.find(e => e.http == url_noport);

	if (serv_cfg) {
		do_log(GF_LOG_DEBUG, `Service ${forced_sdesc ? forced_sdesc.local_base : http_url} has custom config${ (serv_cfg.mabr ? ' and MABR' : '')}`);
		s.mabr = serv_cfg.mabr;
		s.unload = serv_cfg.unload;
		s.mabr_min_active = serv_cfg.activate;
		s.purge_delay = 1000 * serv_cfg.timeshift;
		s.mani_cache = serv_cfg.mcache;
		s.repair = serv_cfg.repair;
		s.corrupted = serv_cfg.corrupted;
		s.local = serv_cfg.local;
		s.keepalive = serv_cfg.keepalive;
		s.gcache = serv_cfg.gcache;
		s.check_ip = serv_cfg.check_ip;
		if (serv_cfg.id) s.id = serv_cfg.id;
	}

	if (!http_url) {
		s.base_url = serv_cfg.local_base;
		s.base_url_no_proto = serv_cfg.local_base;
	} else {
		//remove cgi and remove resource name
		let str = http_url.split('?');
		str = str[0].split('/')
		str.pop();
		s.base_url = str.join('/') + '/';
		//remove protocol scheme
		s.base_url_no_proto = s.base_url.split('://')[1];
	}

	s.get_file = function(url, br_start, br_end)
	{
		let file = null;
		for (let i=0; i<this.mem_cache.length; i++) {
			let f = this.mem_cache[i];
			if (f.aborted) continue;
			if (f.url.indexOf(url)>=0) {}
			//in case the whole server path was not sent in mabr
			else if (f.cache_type && (url.indexOf(f.url)>=0)) {}
			else continue;
			//only deal with exact byte ranges - we could optimize to allow sub-ranges
			if (f.br_start==br_start && f.br_end==br_end) return f;
		}
		return null;	
	};
	s.unqueue_pending_request = function(url, from_mabr)
	{
		let ext=null;
		let pending = this.pending_reqs.find (r =>{
			if (url.indexOf(r.url)>=0) return true;
			//in case the whole server path was not sent in mabr
			if (from_mabr && (r.url.indexOf(url)>=0) ) return true;
			if (r.from_cache && (r.url.indexOf(url)>=0) ) return true;
			return false;
		});
		//if no match and direct multicast, locate manifests by extension
		if (!pending && from_mabr && !s.url) {
			let ext = url.split('.').pop().toLowerCase();
			if ((ext==="mpd") || (ext==="m3u8")) {
				pending = this.pending_reqs.find (r => r.url.indexOf(ext) );
			}
		}
		if (pending)
			this.pending_reqs.splice(this.pending_reqs.indexOf(pending), 1);
		return pending;
	};

	s.create_cache_file = function(url, mime, br_start, br_end, cache_type=CACHE_TYPE_MOD, pid=null)
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
		file.headers = null;
		file.nb_users = 0;
		file.sticky = 0;
		file.br_start = br_start;
		file.br_end = br_end;
		file.cache_type = cache_type;
		file.range = null;
		file.pid = pid;
		if (cache_type) {
			file.xhr_status = 200;
		}

		//get any pending request(s) for this file
		while (true) {
			//we use req.url , eg server path, and not target_url which includes server name
			let pending = this.unqueue_pending_request(url, (cache_type==CACHE_TYPE_MABR));
			if (!pending) break;
			pending.waiting_mabr = 0;
			pending.set_cache_file(file);
			do_log(GF_LOG_DEBUG, `Found pending request for ${file.url}, canceling timeout`);
			if (pid) pid.deactivate_timeout=0;
		}
		do_log(GF_LOG_DEBUG, `Start ${(cache_type==CACHE_TYPE_MABR) ? 'MABR ' : ''}reception of ${file.url} mime ${file.mime} - ${this.mem_cache.length} files in service cache`);
		return file;
	};

	s.mark_active_rep = function(rep, request)
	{
		if (!rep || !request) return;
		let use_same_conn = false;
		let connection_id = s.check_ip ? (request.ip + ':'+request.port) : request.netid;

		let idx = s.pending_deactivate.indexOf(rep);
		if (idx>=0) s.pending_deactivate.splice(idx, 1);

		//mark number of active requests for this quality
		if (!rep.nb_active) {
			do_log(GF_LOG_INFO, `Service ${this.id} Rep ${rep.ID} is now active`);
		} else {
			//check if we have active requests on the same connection to be deactivated. If so, extend timeout of last found
			//and do not change nb_active
			//this avoids activate/deactivate on same connection which would trigger spurious mcast joins/leaves
			let same_conn = this.active_reps_timeouts.filter(e => e.rep.ID == rep.ID && e.cid == connection_id);
			if (same_conn.length) {
				let rep_to = same_conn[same_conn.length-1];
				rep_to.update();
				do_log(GF_LOG_DEBUG, `Service ${this.id} same connection used for same quality ${rep.ID}, extending inactive timeout`);
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

		if (serv_cfg && serv_cfg.js_mod) {
			let req_ok = serv_cfg.js_mod.quality_activation(do_activate, this.mabr_service_id, rep.period_id, rep.AS_ID, rep.ID);
			if (!req_ok) return;
		}

		if (do_activate) {
			this.nb_mabr_active++;
			this.mabr_unload_timeout = 0;
			do_log(GF_LOG_INFO, `Service ${this.id} MABR activated for Rep ${rep.ID} - active in service ${this.nb_mabr_active}`);
		} else if (this.nb_mabr_active) {
			this.nb_mabr_active--;
			do_log(GF_LOG_INFO, `Service ${this.id} MABR deactivated for Rep ${rep.ID} - active in service ${this.nb_mabr_active}`);
			if (!this.nb_mabr_active && this.unload) {
				this.mabr_unload_timeout = sys.clock_ms() + 1000*this.unload;
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
		//set require source ID in case we use a capture file, we could get the first PID before returning from the add_filter function
		args += ':gcache=0:stsi:RSID';
		//multicast is dynamically enabled/disabled, start with all services tuned but disabled
		//only do this if HTTP mirror - otherwise, activate everything to make sure we fetch the manifests and init segments
		if (this.url && this.mabr_min_active>0) args += ':tunein=-3';
		//add repair option last
		if (! this.url) {
			this.repair = true;
			args += ':repair=' + (s.corrupted ? 'strict' : 'strict');
		}
		else if (this.repair) {
			if (s.corrupted) {
				args += ':repair=strict';
			} else {
				//escape URL option
				args += '::repair_urls='+this.url;
			}
		}

		this.source = session.add_filter(args);
		if (!this.source) {
			do_log(GF_LOG_ERROR, `Service ${this.id} Failed to load MABR demux for ${this.mabr}`);
			this.mabr_failure = true;
			return;
		}
		this.source.require_source_id();	
		this.mabr_loaded = true;

		//create custom sink accepting files
		this.sink = session.new_filter("RouteSink"+this.id);
		this.sink.set_cap({id: "StreamType", value: "File", in: true} );
		this.sink.max_pids=-1;
		this.sink.pids=[];

		this.sink.on_setup_error = function(srcf, err) {
			do_log(GF_LOG_WARNING, `Service ${s.id} MABR setup error ${sys.error_string(err)}`);
			s.mabr_failure = true;
			if (s.sink) session.remove_filter(s.sink);
			if (s.source) session.remove_filter(s.source);
			s.sink = null;
			s.source = null;
		};
		this.sink.watch_setup_failure(this.source);
		
		this.sink.configure_pid = function(pid)
		{
			//reconfigure (new file), get previsou file and mar as done
			if (this.pids.indexOf(pid)>=0) {
				//get previous file
				if (pid.url) {
					let file = s.mem_cache.find(f => f.url===pid.url);
					if (file && !file.done) {
						do_log(GF_LOG_INFO, `${pid.url} closing cache file as new file from MABR starts ${pid.get_prop('URL')}`);
						file.done = true;
					}
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
				//auto-deactivate unless unload is set
				if (s.mabr_min_active) {
					pid.deactivate_timeout = 5000 + sys.clock_ms();
				}
				else
					pid.deactivate_timeout = 0;
			}

			if (!s.mabr_service_id) {
				s.mabr_service_id = pid.get_prop('ServiceID');
				do_log(GF_LOG_INFO, `MABR configured for service ${s.mabr_service_id}`);
			}
			//get new URL for this pid	- can be null when service is just being announced
			pid.url = pid.get_prop('URL');
			if (pid.url && (pid.url.charAt(0) != '/')) pid.url = '/' + pid.url;
			pid.mime = pid.get_prop('MIMEType');
			pid.corrupted = false;
			let is_manifest=false;
			if (pid.url) {
				let purl = pid.url.toLowerCase();
				//do not cache HLS/DASH manifests
				if ((purl.indexOf('.m3u8')>=0)|| (purl.indexOf('.mpd')>=0)) is_manifest  = true;
				else is_manifest = false;
			}
			pid.do_skip = false;
			pid.is_manifest = false;
			if (is_manifest) {
				if (s.url == null) pid.is_manifest = true;
				else pid.do_skip = true;
				pid.deactivate_timeout = 0;
			}
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
					do_log(GF_LOG_ERROR, `Service ${this.id} receiving corrupted MABR packet and cache file was already setup, bug in code !`);
				}
				do_log(GF_LOG_INFO, `Corrupted MABR packet for ${pid.url} (valid container ${corrupted==2}), switching to HTTP`);
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
				do_log(GF_LOG_WARNING, `MABR Repair failed for ${pid.url} (valid container ${corrupted==2}), broken data sent to client`);
			}
			if (!file) file = this.create_cache_file(pid.url, pid.mime, 0, 0, CACHE_TYPE_MABR, pid);

			do_log(GF_LOG_DEBUG, `Service ${this.id} receiving MABR packet for ${pid.url} size ${pck.size} end ${pck.end}`);
	
			//reagregate packet 
			if (pck.size)
				file.data = cat_buffer(file.data, pck.data);

			if (pck.start && pck.end) {
				do_log(GF_LOG_INFO, `Service ${this.id} got MABR file ${pid.url} in one packet`);
			}
			else if (pck.start) {
				do_log(GF_LOG_INFO, `Service ${this.id} start receiving MABR file ${pid.url}`);
			}
			else if (pck.end) {
				do_log(GF_LOG_INFO, `Service ${this.id} received MABR file ${pid.url}`);
			}
			if (pck.end) {
				do_log(GF_LOG_INFO, `Service ${this.id} MABR file ${pid.url} end`);
				file.done = true;
				if (pid.is_manifest) {
					update_manifest(file.data, file.url, s);
				}
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
				if (pid.deactivate_timeout && (pid.deactivate_timeout<sys.clock_ms())) {
					print(GF_LOG_INFO, `PID ${pid.url} without active requests - deactivating multicast`);
					let evt = new FilterEvent(GF_FEVT_DASH_QUALITY_SELECT);
					evt.service_id = s.mabr_service_id;
					evt.period_id = pid.get_prop('Period');
					evt.as_id = pid.get_prop('ASID');
					evt.rep_id = pid.get_prop('Representation');
					evt.select = 1;
					session.fire_event(evt, s.source, false, true);
					pid.deactivate_timeout = 0;
				}
			});
		};
		//don't use dash client, we just need to get the files
		this.sink.set_source(this.source);
		do_log(GF_LOG_INFO, `Service ${this.id} loaded MABR for ${this.mabr}${this.repair ? ' with repair' : ''}`);

		if (serv_cfg && serv_cfg.js_mod && typeof serv_cfg.js_mod.service_activation === 'function') {
			serv_cfg.js_mod.service_activation(true);
		}
	};
	s.unload_mabr = function() {
		if (this.source) {
			do_log(GF_LOG_INFO, `Service ${this.id} stopping MABR`);
			if (this.sink) session.remove_filter(this.sink);
			if (this.source) session.remove_filter(this.source);
			this.sink = null;
			this.source = null;
			this.mabr_loaded = false;
			this.mabr_failure = false;

			if (serv_cfg && serv_cfg.js_mod && typeof serv_cfg.js_mod.service_activation === 'function') {
				serv_cfg.js_mod.service_activation(false);
			}
		}
	};

	all_services.push(s);
	do_log(GF_LOG_INFO, `Created Service ID ${s.id} for ${s.url ? s.url : serv_cfg.local_base}${s.mabr ? ' with MABR' : ''}`);
	//and load if requested - we force mcast activation when an acess to the mpd is first detected
	if (force_mcast_activate && s.mabr)
		s.load_mabr();
	return s;
}
globalThis.all_services = all_services;
globalThis.create_service = create_service;

let script_args=null;
function do_init()
{
	if (http_out_f == null) {
		let rdir = (sys.file_exists('.gpac_auth')) ? '.gpac_auth' : "gmem";
		sys.args.forEach(a => {
			if (a.indexOf('rdirs=')>=0) rdir = null;
		});
		if (script_args && script_args.indexOf('rdirs=')>=0) rdir = null;

		let http_args = "httpout";
		if (rdir) http_args += ":rdirs="+rdir;
		if (script_args) http_args += script_args;

		try {
			let http = session.add_filter(http_args);	
			server_ports = http.get_arg("port");
			do_log(GF_LOG_INFO, `Starting HTTP server on port ${server_ports}`);
		} catch (e) {
			do_log(GF_LOG_ERROR, `Failed to start HTTP server`);
			sys.exit(1);
		}
	} else {
		server_ports = http_out_f.get_arg("port");
		do_log(GF_LOG_INFO, `Attached server running on port ${server_ports}`);
	}
	//preload services
	services_defs.forEach(sd => {
		if (sd.unload) return;
		let s = create_service(sd.http, false, sd);
		if (!s) {
			do_log(GF_LOG_ERROR, `Failed to create service ${sd.http}`);
		} else {
			s.load_mabr();
		}
	});

	session.post_task( () => {
		let do_gc = false;
		let now = sys.clock_ms();
		for (let i=0; i<all_services.length; i++) {
			let s = all_services[i];
			//cleanup mem cache
			for (let i=0; i<s.mem_cache.length; i++) {
				let f = s.mem_cache[i];
				if (f.nb_users || f.sticky) continue;
				if (!f.aborted && (f.received + s.purge_delay >= now)) continue;
				s.mem_cache.splice(i, 1);
				i--;
				do_log(GF_LOG_DEBUG, `Removing ${f.url} from cache - remain ${s.mem_cache.length}`);
			}

			//check timeout on pending reqs waiting for MABR
			for (let i=0; i<s.pending_reqs.length; i++) {
				let req = s.pending_reqs[i];
				if (!req.waiting_mabr || (req.waiting_mabr>now)) continue;
				if (! s.url) {
					do_log(GF_LOG_DEBUG, `MABR timeout for ${req.url} after ${now - req.waiting_mabr_start} ms`);
					s.pending_reqs.splice(i, 1);
					i--;
					req.reply = 404;
					req.send();
					continue;
				}
				if (s.source && s.source.probe_url(s.mabr_service_id, req.url)) {
					req.waiting_mabr = 0;
					continue;
				}
				do_log(GF_LOG_DEBUG, `MABR timeout for ${req.url} after ${now - req.waiting_mabr_start} ms - fetching from HTTP at ` + req.target_url);
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
				do_log(GF_LOG_INFO, `Service ${s.id} Rep ${rep.ID} no longer on multicast`);
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
					do_log(GF_LOG_INFO, `Service ${s.id} Rep ${active_rep.ID} is now inactive`);
					if (s.keepalive)
						s.unload_timeout = sys.clock_ms() + 1000*s.keepalive;
				} else {
					do_log(GF_LOG_DEBUG, `Service ${s.id} Rep ${active_rep.ID} is still active (active clients ${active_rep.nb_active})`);
				}
				if (active_rep.mabr_active && s.mabr_min_active && (active_rep.nb_active<s.mabr_min_active) ) {
					if (!mabr_canceled) {
						s.activate_mabr(active_rep, false);
					} else {
						active_rep.mabr_deactivate_timeout = sys.clock_ms() + 2000;
						s.pending_deactivate.push(active_rep);
					}
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
			//check timeout on MABR service deactivation
			if (s.mabr_unload_timeout && (s.mabr_unload_timeout < now)) {
				s.mabr_unload_timeout = 0;
				if (!s.nb_mabr_active) {
					s.unload_mabr();
					do_gc = true;
				}
			}
			//check service unload
			if (s.unload_timeout && (s.unload_timeout < now)) {
				s.unload_mabr();
				do_log(GF_LOG_INFO, `Service ${s.id} unloading`);
				all_services.splice(i, 1);
				if (typeof s.on_close == 'function') s.on_close();				
				if (!all_services.length && do_quit) {
					print("No more services and quit requested - exiting");
					session.remove_filter(http_out_f);
					return false;
				}
				i--;
				do_gc = true;
			}
		}
		if (force_quit) {
			print("No more requests and quit requested - exiting");
			session.remove_filter(http_out_f);
			return false;
		}

		if (do_gc) sys.gc();
		if (session.last_task) return false;
		//perform cleanup every 100ms
		return 100;

	}, "mediaserver_cleanup");
}

let http_out_f = null;
//catch filter creation
session.set_new_filter_fun( (f) => {
	//bind our custom http logic
	if (!http_out_f && (f.name == "httpout")) {
		http_out_f = f;
		f.bind(httpout);
	}
});

let mods_pending=0;

function get_base_url(url)
{
	let str = url.split('/');
	str.pop();
	return str.join('/') + '/';
}

filter.initialize = function() {

	let gpac_help = sys.get_opt("temp", "gpac-help");
	let gpac_doc = (sys.get_opt("temp", "gendoc") == "yes") ? true : false;
	if (gpac_help || gpac_doc) {
		filter._help = filter_help;
		filter.set_help(filter._help);

		let	scripts = sys.enum_directory(filter.jspath, "*.js");
		scripts.forEach(s => {
			if (s.name == "init.js") return;
			import(s.name).then(obj => { 

				filter._help += '## ' + obj.description + '\nModule is loaded when using `js='+s.name.replace('.js', '')+'`\n\n' + obj.help + '\n';
				filter.set_help(filter._help);
			}).catch(err => {});
		});
		return GF_OK;
	}
	
	do_quit = filter.quit;
	script_args = filter.src_args;
	if (script_args) {
		let local_args = ["quit", "services"];
		script_args = script_args.split(':');
		local_args.forEach(a => {	
			let e = script_args.find(e => e.startsWith(a));
			if (e) script_args.splice(script_args.indexOf(e), 1);
		});
		script_args = script_args.join(':');
	} else {
		script_args="";
	}
	if (!filter.scfg || !filter.scfg.length) {
		//todo: we should allow to specify a default service description file in ~/.gpac
		print("No service description provided, configuring as proxy for any URL");
		services_defs = [ {'http': '*', 'unload': true} ];
	} else {
		try {
			let ab = sys.load_file(filter.scfg, true);
			services_defs = JSON.parse(ab);
			if (!Array.isArray(services_defs)) throw "Invalid JSON, expecting array";

			services_defs = services_defs.filter(e => ((typeof e.active === 'undefined') || e.active) && typeof e.comment === 'undefined' );
			services_defs.forEach(sd => {
				//check options are valid
				if (typeof sd.http == 'undefined') sd.http = null;
				else if (typeof sd.http != 'string') throw "Missing or invalid http property, expecting string got "+typeof sd.http;
				if (typeof sd.mabr == 'undefined') sd.mabr = null;
				else if (typeof sd.mabr != 'string') throw "Missing or invalid mabr property, expecting string got "+typeof sd.mabr;
				if (typeof sd.local == 'undefined') sd.local = null;
				else if (typeof sd.local != 'string') throw "Missing or invalid local property, expecting string got "+typeof sd.local;
				if (typeof sd.unload != 'number') sd.unload = DEFAULT_MABR_UNLOAD_SEC;
				else if (sd.unload < 0) throw "Invalid unload property, expecting positive number";				
				if (typeof sd.activate != 'number') sd.activate = DEFAULT_ACTIVATE_CLIENTS;
				else if (sd.activate < 0) throw "Invalid activate property, expecting positive number";
				if (typeof sd.timeshift != 'number') sd.timeshift = DEFAULT_TIMESHIFT;
				else if (sd.timeshift < 0) throw "Invalid timeshift property, expecting positive number";
				if (typeof sd.mcache != 'boolean') sd.mcache = DEFAULT_MCACHE;
				if (typeof sd.repair != 'boolean') sd.repair = DEFAULT_REPAIR;
				if (typeof sd.corrupted != 'boolean') sd.corrupted = DEFAULT_CORRUPTED;
				if (typeof sd.gcache != 'boolean') sd.gcache = DEFAULT_GCACHE;
				if (typeof sd.keepalive != 'number') sd.keepalive = DEFAULT_KEEPALIVE_SEC;
				else if (sd.keepalive < 0) throw "Invalid keepalive property, expecting positive number";
				if (typeof sd.check_ip != 'boolean') sd.check_ip = DEFAULT_CHECKIP;
				if (typeof sd.id == 'undefined') sd.id = null;
				if (typeof sd.sources == 'undefined') sd.sources = [];
				else if (!Array.isArray(sd.sources)) {
					throw "Invalid sources property, expecting array";
				} else {
					sd.sources.forEach(a => {
						if (typeof a.name != 'string') throw "Invalid name in source element, expecting string";
						if (typeof a.url != 'string') throw "Invalid url in source element, expecting string";
					});
				}
				if (typeof sd.js == 'undefined') sd.js = null;
				else if (typeof sd.js != 'string') throw "Missing or invalid js property, expecting string got "+typeof sd.local;
				if (typeof sd.noproxy == 'undefined') sd.noproxy = (sd.http && sd.local) ? true : false;
				else if (typeof sd.noproxy != 'boolean') throw "Missing or invalid noproxy property, expecting boolean got "+typeof sd.noproxy;

				if (!sd.http && !sd.local) throw "At least one of `http` or `local` shall be specified";

				//safety checks
				if (!sd.unload) sd.keepalive = 0;
				if (sd.unload>sd.keepalive) sd.keepalive = sd.unload;

				//extract proto, host and path
				if (sd.http) {
					sd.http = sd.http.replace(/([^\/\:]+):\/\/([^\/]+):([0-9]+)\//, "$1://$2/");
					//remove default ports
					let url_lwr = sd.http.toLowerCase();
					if (url_lwr.startsWith('https://')) {
						sd.http = sd.http.replace(":443/", "/");
					}
					if (url_lwr.startsWith('http://')) {
						sd.http = sd.http.replace(":80/", "/");
					}
					let idx = sd.http.indexOf('://');
					sd.http_proto = sd.http.substring(0, idx);
					let str = sd.http.substring(idx+3).split('/');
					sd.http_host = str[0];
					str.shift();
					str.pop();
					sd.http_path = '/'+str.join('/') + '/';
				} else {
					sd.http_proto = null;
					sd.http_host = null;
					sd.http_path = null;
				}
				//if local is a file, get base path
				sd.local_base = null;
				if (sd.local) {
					sd.local = sd.local.split('?')[0];
					if (!sd.local.length) sd.local = null;
					else if (sd.local.charAt(sd.local.length-1) != '/') {
						let str = sd.local.split('/')
						str.pop();
						sd.local_base = str.join('/') + '/';
					} else {
						sd.local_base = sd.local;
						sd.local = null;
					}
				}
				//load JS module if present
				if (sd.js) {
					mods_pending++;
					let script_src = sd.js;
					//local path
					if (sys.file_exists(script_src)) {}
					else if (sys.file_exists(sd.js+'.js')) {
						script_src = sd.js+'.js';
					}
					//path in our filter directory
					else {
						script_src = filter.jspath + sd.js + '.js';
						if (!sys.file_exists(script_src)) throw "File " + sd.js + " does not exist";
					}
					import(script_src).then(obj_mod => {
						if (typeof obj_mod.init !== 'function') throw "Invalid module, missing `init` function";
						if (sd.mabr) {
							if (typeof obj_mod.quality_activation !== 'function') throw "Invalid module, missing `quality_activation` function";
						} else {
							if (typeof obj_mod.resolve !== 'function') throw "Invalid module, missing `resolve` function";
						}
						mods_pending--;
						sd.logname = sd.js;
						sd.log = function(level, msg) {
							do_log(level, msg, this.logname);
						};						
						obj_mod.init(sd);
						sd.js_mod = obj_mod;
					}).catch(e => {
						do_log(GF_LOG_ERROR, `Failed to load service script ${sd.js}: ${e}`);
						sys.exit(1);
					});
				} else {
					sd.js_mod = null;
				}
			});
		} catch (e) {
			do_log(GF_LOG_ERROR, `Failed to load services configuration ${filter.scfg}: ${e}`);
			sys.exit(1);
		}
	}
	
	//after this, filter object is no longer available in JS since we don't set caps or setup filter.process
	session.post_task( () => {
		if (mods_pending) return 100;
		do_init();
		return false;
	}, "init", 200);
	
};

