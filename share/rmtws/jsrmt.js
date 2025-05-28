import { Sys as sys } from 'gpaccore'


let all_filters = [];



session.reporting(true);



function on_all_connected(cb) {

	session.post_task( ()=> {

		let local_connected = true;
		let all_js_filters = [];

		session.lock_filters(true);

		for (let i=0; i<session.nb_filters; i++) {
			let f = session.get_filter(i);

			if (f.is_destroyed()) continue;

			if (!f.nb_opid && !f.nb_ipid) {
				local_connected = false;
				print("Filter not connected: ");
				print(JSON.stringify(gpac_filter_to_object(f)));
				break;
			}

			all_js_filters.push(gpac_filter_to_object(f));
		}

		session.lock_filters(false);

		if (local_connected) {
			cb(all_js_filters); // should prop be inside the lock?
			return false;
		}

		return 2000;
	});

}




let filter_props_lite = ['name', 'status', 'bytes_done', 'type', 'ID', 'nb_ipid', 'nb_opid', 'idx', 'itag']
let filter_args_lite = []
let pid_props_lite = []

function gpac_filter_to_object(f, full=false) {
	let jsf = {};

	for (let prop in f) {
		if (full || filter_props_lite.includes(prop))
			jsf[prop] = f[prop];
	}

	jsf['gpac_args'] = [] ; // filtrer par type de filtre ?

	if (full) {		//TODO: remove tmp hack to avoid pfmt error on ffenc
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
		let pidname = f.ipid_props(d, "name");
		let jspid = {};

		f.ipid_props(d, (name, type, val) => {
			if (full || pid_props_lite.includes(name))
				jspid[name] = {'type': type, 'val': val};

		});
		jspid["buffer"] = f.ipid_props(d, "buffer");
		// jspid["buffer_total"] = f.ipid_props(d, "buffer_total");
		jspid['source_idx'] = f.ipid_source(d).idx;

		jsf['ipid'][pidname] = jspid;
	}

	for (let d=0; d<f.nb_opid; d++) {
		let pidname = f.opid_props(d, "name");
		let jspid = {};

		f.opid_props(d, (name, type, val) => {
			if (full || pid_props_lite.includes(name))
				jspid[name] = {'type': type, 'val': val};

		});
		jspid["buffer"] = f.opid_props(d, "buffer");
		// jspid["buffer_total"] = f.opid_props(d, "buffer_total");
		jsf['opid'][pidname] = jspid;
	}

	return jsf;

}

let filter_uid = 0;

session.set_new_filter_fun( (f) => {
		print("new filter " + f.name);
		f.idx = filter_uid++;
		f.iname = ''+f.idx;

		all_filters.push(f);

		console.log("NEW FILTER ITAG " + f.itag);
		if (f.itag == "NODISPLAY")
			return


} );

session.set_del_filter_fun( (f) => {
	print("delete filter " + f.iname + " " + f.name);
	let idx = all_filters.indexOf(f);
	if (idx>=0)
		all_filters.splice(idx, 1);

	console.log("RM FILTER ITAG " + f.itag);
	if (f.itag == "NODISPLAY")
		return


});

session.set_event_fun( (evt) => {

});








let all_clients = [];
let cid = 0;


let remove_client = function(client_id)  {
	for (let i = 0; i<all_clients.length; i++) {
		if (all_clients[i].id == client_id) {
			all_clients.splice(i,1);
			return
		}
	}
}

function JSClient(id, client) {
	this.id = id;
	this.client = client;

	this.details_needed = {};

	this.on_client_data = function(msg) {

		console.log("All clients:");
		for (let jc of all_clients) {
			console.log("Client ", jc.id, jc.client.peer_address );
		}

		console.log("on_client_data on client id ", this.id, " len ", msg.length, msg);
		console.log("this has peer:", this.client.peer_address);

		let text = msg;
		if (text.startsWith("json:")) {
			try {
				let jtext = JSON.parse(text.substr(5));
				if (!('message' in jtext)) return;

				if ( jtext['message'] == 'get_all_filters' ) {
					print("Sending all filters when ready");
					this.send_all_filters();

				}

			} catch(e) {
				console.log(e);
			}
		}

	}


	this.send_all_filters = function () {

		on_all_connected( (all_js_filters) => {

			print("----- all connected -----");
			print(JSON.stringify(all_js_filters, null, 1));
			print("-------------------------");

			this.client.send(JSON.stringify({ 'message': 'filters', 'filters': all_js_filters }));

			session.post_task( ()=> {

				let js_filters = [];

				session.lock_filters(true);

				for (let i=0; i<session.nb_filters; i++) {
					let f = session.get_filter(i);
					js_filters.push(gpac_filter_to_object(f));
				}

				session.lock_filters(false);

				if (this.client) {
					this.client.send(JSON.stringify({ 'message': 'update', 'filters': js_filters }));
				}



				return 1000;
			});


		});
	}




}

session.rmt_on_new_client = function(client) {
	console.log("rmt on client");
	print(typeof(client));

	let js_client = new JSClient(++cid, client);

	all_clients.push(js_client);

	console.log("New ws client ", js_client.id, " gpac peer ", js_client.client.peer_address);

	js_client.client.on_data = (msg) =>  {
		if (typeof(msg) == "string")
			js_client.on_client_data(msg);
		else {
			let buf = new Uint8Array(msg)
			console.log("Got binary message of type", typeof(msg), "len ", buf.length, "with data:", buf);

		}
	}

	js_client.client.on_close = function() {
		console.log("ON_CLOSE on client ", js_client.id, " ", client.peer_address);
		remove_client(js_client.id);
		js_client.client = null;
	}
}