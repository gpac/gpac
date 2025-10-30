export const description = "Source Remultiplexer";

const DEF_FORMAT = 'mp4';
const DEF_FILTERING = 'av';

export const help = `This module remultiplexes the source files in a desired format without transcoding.

__Service configuration parameters used :__ \`local\` (mandatory), \`sources\` (mandatory).

__Service configuration additional parameters__
- fmt: default format to use (default is ${DEF_FORMAT}). Supported formats are:
  - \`src\`: no remultiplexing
  - \`mp4\`: fragmented MP4
  - \`ts\`: MPEG-2 TS
  - \`gsf\`: GPAC streaming format
  - \`dash\`: MPEG-DASH format, single quality
  - \`hls\`: HLS format, single quality

Sources are described using the \`sources\` array in the service configuration.

CGI parameters for request are:
- fmt: (string, same as service configuration \`fmt\`) multiplexing format. If set to \`src\`, all other CGI parameters are ignored.
- start: (number, default 0) start time in second of re-multiplexed content.
- speed: (number, default 1) speed (>=0), keep video stream only and remove non SAP frames.
- media: (string, default \`${DEF_FILTERING}\`) media filtering type
  - 'av': keep both audio and video
  - 'a': keep only audio
  - 'v': keep only video


Configuration for serving a directory with remultiplexing to mp4:
EX {"local": "/service1/", "js": "remux", "sources": [{"name": "vids", "url": "/path/to/vids/"}], "fmt": "mp4"}
`;

let config = null;
export function init(service_config) 
{
    config = service_config;
    //validate configuration
    if (!config.local_base) {
        config.log(GF_LOG_ERROR, 'Missing `local`mount point in service configuration');
        return false;
    }
    if (!config.sources.length) {
        config.log(GF_LOG_ERROR, 'No sources defined Missing `local` mount point in service configuration');
        return false;
    }
};


export function resolve(req) {
    try {

    let orig_url = req.url;
    let cgi = req.url.split('?');
    let req_url = cgi[0];
    let multi_pid=false;
    let start = 0;
    let speed = 1;
    let media = DEF_FILTERING;
    let fmt=DEF_FORMAT;
    if (typeof config.fmt == 'string')
        fmt = config.fmt;

    if (cgi.length>1) {
        cgi = cgi[1].split('&');
        cgi.forEach(a => { 
            a = a.split('=');
            if ((a[0] == 'fmt') && (a.length>=1)) fmt = a[1];
            if ((a[0] == 'start') && (a.length>=1)) start = parseFloat(a[1]);
            if ((a[0] == 'speed') && (a.length>=1)) speed = parseFloat(a[1]);
            if ((a[0] == 'media') && (a.length>=1)) media = a[1];
        });
    }
    
    let loc_url = null;
    let loc_name = null;
    //check if we have loaded a service for this URL, for DASH/HLS - todo we need to handle multi-user access and seek requests !
    req.service = all_services.find( e => e.local == config.local);
    if (req.service) {
        req.from_cache = true;
        if (fmt =='dash') req_url += "_live.mpd";
        else if (fmt =='hls') req_url += "_live.m3u8";
        return [req_url, false];
    }
    let my_url = req_url.replace(config.local_base, '');
    let e = config.sources.find( a => (a.name == my_url) );
    if (!e) e = config.sources.find( a => my_url.startsWith(a.name) );
    if (!e) {
        config.log(GF_LOG_WARNING, 'Invalid source URL ' + req.url + ' for ' + config.sources.length);
        return [null, false];
    }
    loc_url = e.url;
    loc_name = e.name;
    if (loc_url.indexOf('://') < 0) {
        loc_url = my_url.replace(e.name, e.url);
        if (!sys.file_exists(loc_url)) {
            config.log(GF_LOG_WARNING, 'URL not found ' + loc_url);
            return [null, false];
        }
    }

    if (fmt=='src') {
        req.target_url = loc_url;
        return [loc_url, false];
    }

    loc_name = sys.basename(loc_name);

    let link_args='';
    if (speed>2) link_args = 'video';
    else if (speed<=0) link_args = 'video';
    else if (media=='a') link_args = 'audio';
    else if (media=='v') link_args = 'video';
    let mux_args=null;
    let src_opt='';
    let def_mime=null;
    let segdur=4;
    let cdur=1;
    let asto=4;
    let stl=''; 
    //stl=':stl';

    fmt=fmt.toLowerCase();
    if ((fmt=='mp4')||(fmt=='fmp4')) {
        mux_args = 'mp4mx:frag:cdur=1';
        def_mime = 'video/mp4';
    }
    else if ((fmt=='m2ts')||(fmt=='ts')) {
        mux_args = 'm2tsmx:nb_pack=20';
        def_mime = 'video/mp2t';
    }
    else if (fmt=='gsf') {
        mux_args = 'gsfmx';
        def_mime = 'application/x-gpac-sf';
    }
    else if (fmt=='dash') {
        //we use segment timeline, should always be supported and we have no clue about the SAP frequency in source
        mux_args = `dasher:segdur=${segdur}:cdur=${cdur}:asto=${asto}:dynamic${stl}:sreg:tsalign=0:mname=${loc_name}_live.mpd`;
        def_mime = 'application/dash+xml';
        multi_pid=true;
    }
    else if (fmt=='hls') {
        mux_args = `dasher:segdur=${segdur}:cdur=${cdur}:asto=${asto}:dynamic:sreg:tsalign=0:mname=${loc_name}_live.m3u8`;
        def_mime = 'application/dash+xml';
        multi_pid=true;
    }
    else {
        req.reply = 401;
        req.send('Invalid request format, please use one of mp4, ts or gsf');
        return [null, false];
    }

    //gather our state
    let s_state = {};
    let args = 'src='+loc_url;
    if (multi_pid) {
        //override both URL and sourcepath properties
        args += ':gpac:#FileAlias=' + loc_name;
    }
    s_state.src = session.add_filter(args);
    s_state.src.require_source_id();	
    let prev_f = s_state.src;

    if ((speed>2) || (speed<=0)) {
        s_state.reframer = session.add_filter('reframer:saps=1,2,3', s_state.src, link_args);
        s_state.reframer.require_source_id();	
        prev_f = s_state.reframer;
    }
    //todo - add transcoding here

    s_state.mux = session.add_filter(mux_args, prev_f, link_args);
    s_state.mux.require_source_id();

    s_state.sink = session.new_filter("TransmuxSink");
    s_state.sink.set_cap({id: "StreamType", value: "File", in: true} );
    s_state.sink.set_source(s_state.mux, link_args);
    if (multi_pid) s_state.sink.max_pids = -1;
    
    s_state.sink.pids = [];
	s_state.sink.configure_pid = function(pid) {
        if (this.pids.indexOf(pid)<0) {
            this.pids.push(pid);
            //send play
            let evt = new FilterEvent(GF_FEVT_PLAY);
            evt.start_range = start;
            evt.timestamp_based = 3;
            pid.send_event(evt);

            if (!multi_pid) {
                let mime = pid.get_prop('MIMEType');
                if (!mime) mime = 'video/mp4';
                req.headers_out.push( { "name" : 'Content-Type', "value": mime} );
                req.headers_out.push( { "name" : 'Transfer-Encoding', "value": 'chunked'} );
                req.headers_out.push( { "name" : 'Cache-Control', "value": 'no-cache'} );
                req.reply = 200;
                req.send();
            }
        }
        let init_name = pid.get_prop('InitName');
        if (init_name) {
            pid.url = init_name;
        } else {
            pid.url = pid.get_prop('URL');
        }
        pid.is_manifest = false;
        if (pid.get_prop('IsManifest')) pid.is_manifest = true;
    };

    if (multi_pid) {
        //create service for this session 
        req.service = create_service(null, false, config);

        s_state.sink.process = function() {
            this.pids.forEach(function(pid) {
                let s = req.service;
                while (1) {
                    let pck = pid.get_packet();
                    if (!pck) break;

                    let file = s.mem_cache.find(f => f.url===pid.url);
                    if (pck.start) {
                        let url = pck.get_prop('FileName');
                        if (url) pid.url = url;
                        if (file && !file.done) {
                            config.log(GF_LOG_DEBUG, `closing cache file ${file.url}`);
                            file.done = true;
                        }
                        file = s.mem_cache.find(f => f.url===pid.url);
                    }

                    if (!file) {
                        file = s.create_cache_file(pid.url, pid.mime, 0, 0, 1);
                        if (pid.is_manifest) file.sticky = true;
                    } else if (pck.start) {
                        file.received = sys.clock_ms();
                        file.data = new ArrayBuffer();
                        file.done = false;
                        file.aborted = false;
                    }
                    if (pck.size)
                        file.data = cat_buffer(file.data, pck.data);
        
                    if (pck.end) {
                        file.done = true;
                    }        
                    pid.drop_packet();
                }
            });
        };
        req.from_cache = true;
        req.target_url = req_url+'_live.' + (fmt=='hls' ? 'm3u8' : 'mpd');
        req.url = req.target_url;
        req.service.on_close = function() {
            config.log(GF_LOG_DEBUG, `Removing transmux filter chain file for ${orig_url}`);
            session.remove_filter(s_state.sink, s_state.src);
        };
        if (req.service.keepalive) req.service.keepalive = 10;
        return [req.target_url, false];
    }

    s_state.waiting = false;
    s_state.pck_offset = 0;
    s_state.sink.process = function()
    {
        //This is a sink so never blocking, we will get called as fast as possible (whenever input is here)
        //we  reschedule so that we always sleep unless a packet is needed
        //we don't need to be called very often
        if (!s_state.waiting) this.reschedule(20000);
    };
    //custom read method, fetch directly from sink as we don't provide any caching for the result
	s_state.read = function(ab) {
        this.waiting = false;
        let pid =  this.sink.pids[0];
        let pck = pid.get_packet();
        if (!pck) {
            if (pid.eos && !pid.is_flush) return 0;
            this.waiting = true;
            return -1;
        }
		let src_ab = pck.data;
		let to_read = src_ab.byteLength - this.pck_offset;
		if (to_read > ab.byteLength) to_read = ab.byteLength;
		new Uint8Array(ab, 0, to_read).set(new Uint8Array(src_ab, this.pck_offset, to_read) );
		this.pck_offset += to_read;
        if (this.pck_offset === src_ab.byteLength) {
            pid.drop_packet();
            this.pck_offset = 0;
        }
		return to_read;
    };
    s_state.on_close = function() {
        config.log(GF_LOG_DEBUG, `Removing transmux filter chain for ${orig_url}`);
        session.remove_filter(this.sink, this.src);
        this.src  = null;
        this.sink = null;
        this.mux = null;
    };
    return [s_state, false];

    } catch (e) {
        config.log(GF_LOG_ERROR, `Failed to setup filter chain for ${orig_url}`);
        req.reply = 404;
        req.send('Failed to setup filter chain: ' + e);
        return [null, false];
    }
}
