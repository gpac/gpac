const gpac = require('../nodejs');

console.log('GPAC NodeJS version ' + gpac.version + ' libgpac ' + gpac.abi_major + ':' + gpac.abi_minor + '.' + gpac.abi_micro);

var FILTERS = []
var gpac_args = []

gpac.init(gpac.GF_MemTrackerSimple, "0");


process.argv.forEach( val =>  {
	if (val.startsWith('-f=')) { FILTERS.push( val.substring(3)  ); }
	else gpac_args.push(val);

} )

gpac.set_args(gpac_args);

let fs = new gpac.FilterSession();


////////// RMT_WS example /////////
gpac.enable_rmtws(true);



let filter_props_lite = ['name', 'status', 'bytes_done', 'type', 'ID', 'nb_ipid', 'nb_opid', 'idx', 'itag']
let filter_args_lite = []
let pid_props_lite = []

function gpac_filter_to_object(f, full=false) {
	let jsf = {};

	for (let prop in f) {
		if (full || filter_props_lite.includes(prop))
			jsf[prop] = f[prop];
	}

	jsf['gpac_args'] = [] ;

	if (full) {
		// let all_args = f.all_args(false); // full args
		let all_args = f.all_args(true); // full args => error in js interface (param is reverse of value_only)
		// console.log(JSON.stringify(all_args));
		for (let arg of all_args) {
			if (arg && (full || filter_args_lite.includes(arg.name)))
				jsf['gpac_args'].push(arg)

		}
	}

	jsf['ipid'] = {};
	jsf['opid'] = {};

	for (let d=0; d<f.nb_ipid; d++) {
		let pidname = f.ipid_prop(d, "name");
		let jspid = {};

		f.ipid_enum_props(d, (name, val, type) => {
			if (full || pid_props_lite.includes(name))
				jspid[name] = {'type': type, 'val': val};

		});

		jspid["buffer"] = f.ipid_prop(d, "buffer");
		// jspid["buffer_total"] = f.ipid_prop(d, "buffer_total");
		jspid['source_idx'] = f.ipid_source(d).idx;

		jsf['ipid'][pidname] = jspid;
	}

	for (let d=0; d<f.nb_opid; d++) {
		let pidname = f.opid_prop(d, "name");
		let jspid = {};

		f.opid_enum_props(d, (name, val, type) => {
			if (full || pid_props_lite.includes(name))
				jspid[name] = {'type': type, 'val': val};

		});

		jspid["buffer"] = f.opid_prop(d, "buffer");
		// jspid["buffer_total"] = f.opid_prop(d, "buffer_total");
		jspid["dest_idx"] = [];

		let sinks = f.opid_sinks(d);
		for (fsink of sinks) {
			jspid["dest_idx"].push(fsink.idx);
		}
		jsf['opid'][pidname] = jspid;
	}
	return jsf;

}


let all_clients = [];
let cid = 0;

let get_all_filters = function() {
	var all_filters = [];

	for (let i=0; i<fs.nb_filters; i++) {
		let f = fs.get_filter(i);
		let jf = gpac_filter_to_object(f);

		all_filters.push(jf)
	}

	return all_filters;
}

let on_client_data = function(js_client, msg) {

	console.log("All clients:");
	for (let jc of all_clients) {
		console.log("Client ", jc.id, jc.gpac.peer_address );
	}

	console.log("on_client_data on client id ", js_client.id, " len ", msg.length, msg);
	console.log("this has peer:", js_client.gpac.peer_address);

	//js_client.gpac.send("reply from this function on client" + js_client.id + " orig: " + msg);

	if (msg.startsWith("json:")) {

		console.log("Got json message");

		let jtext = JSON.parse(msg.substr(5));
		console.log("parsed json object", jtext, ('message' in jtext));

		if (!('message' in jtext)) return;

		if ( jtext['message'] == 'get_all_filters' ) {
			console.log("Sending all filters");

			fs.print_graph()
			fs.print_stats()

			if (js_client && js_client.gpac && js_client.gpac.peer_address)
				js_client.gpac.send(JSON.stringify({ 'message': 'filters', 'filters': get_all_filters() }));


			t = {};
			t.execute = function() {

				if (js_client && js_client.gpac && js_client.gpac.peer_address) {
					js_client.gpac.send(JSON.stringify({ 'message': 'update', 'filters': get_all_filters() }));

					return 1000;
				}
				else {
					return -1;
				}
			}
			fs.post_task(t);

		}
	}
}

let remove_client = function(client_id)  {
	for (let i = 0; i<all_clients.length; i++) {
		if (all_clients[i].id == client_id) {
			all_clients.splice(i,1);
			return
		}
	}
}

gpac.rmt_on_new_client = function(client) {
	console.log("rmt on client");
	console.log(typeof(client));

	let js_client = {
		id: ++cid,
		gpac: client

	}

	all_clients.push(js_client);

	console.log("New ws client ", js_client.id, " gpac peer ", js_client.gpac.peer_address);

	js_client.gpac.on_data = (msg) =>  {
		if (typeof(msg) == "string")
			on_client_data(js_client, msg);
		else {
			let buf = new Uint8Array(msg)
			console.log("Got binary message of type", typeof(msg), "len ", buf.length, "with data:", buf);

		}
	}

	js_client.gpac.on_close = function() {
		console.log("ON_CLOSE on client ", js_client.id, " ", client.peer_address);
		remove_client(js_client.id);
	}


}


gpac.enable_userws(true);

gpac.userws_on_new_client = function(client) {

    console.log("[USERWS] new client ", client.peer_address);

    client.on_data = (msg) =>  {
        console.log("[USERWS] client", client.peer_address, "received", msg);

        client.send("ACK");
    }

    client.on_close = () => {
        console.log("[USERWS] client", client.peer_address, "disconnected");
    }
}

///////////////////
let filter_uid = 0;

fs.on_filter_new = function(f) {
	console.log('New filter created: ' + f.name);
	f.idx = filter_uid++;
	f.iname = ''+f.idx;

}
fs.on_filter_del = function(f) {
	console.log('Filter deleted: ' + f.name);
}

fs.on_event = function(evt) {
	if (evt.ui_type == gpac.GF_EVENT_QUIT) {
		fs.abort(gpac.GF_FS_FLUSH_NONE);
		return true;
	}
	return false;
}

const session_done = () => {
	console.log('Session done running, graph: ');
	fs.print_graph();
};


//load filters
FILTERS.forEach( fname => {
	console.log('Loading filter ' + fname);
	let f = fs.load(fname);
});

console.log('Running in blocking mode');
fs.run();
session_done();
return;
