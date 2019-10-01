import {XMLHttpRequest} from 'xhr'

filter.pids = [];

//metadata
filter.set_name("JSInspect");
filter.set_desc("JS-based inspect filter");
filter.set_version("0.1beta");
filter.set_author("GPAC team");
filter.set_help("This filter porvides a very simple javascript inspecter");

//exposed arguments
filter.set_arg("raw", "if set, accept undemuxed input PIDs", GF_PROP_BOOL, "false");
filter.set_arg("fwd", "if set, forward incoming packets to output PIDs", GF_PROP_BOOL, "false");
//filter.max_pids = -1;

filter.initialize = function() {
 print(GF_LOG_DEBUG, "Init at " + Date() + " nb pids " + this.pids.length);

 if (this.raw)
	this.set_cap("StreamType", "Unknown", false, true);
 else
	this.set_cap("StreamType", "File", false, true);

 filter.xhr = new XMLHttpRequest();
 filter.xhr.responseType = "document";
 filter.xhr.onload = function()
 {
 	print('load status: ' + this.status);
 	let reps = this.response.documentElement.getElementsByTagName('Representation');
 	print('nb reps: ' + reps.length);

 };
 filter.xhr.onerror = function() {
  print("Request failed");
};
filter.xhr.onreadystatechange = function() {
  print(`XHR state ${this.readyState}`);
  if (this.readyState != 4) return;

  print(`headers: ${this.getAllResponseHeaders()}`);

};
 filter.xhr.open("GET", "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-1s/mp4-live-1s-mpd-AV-NBS.mpd");
 filter.xhr.send();
}

filter.update_arg = function(name, val)
{
	print(GF_LOG_INFO, "Value " + name + " is " + val);
}

filter.configure_pid = function(pid)
{
	if (this.pids.indexOf(pid)<0) {
		print(GF_LOG_INFO, "Configure Pid " + typeof pid);
		let evt = new FilterEvent(GF_FEVT_PLAY);
		pid.send_event(evt);
		this.pids.push(pid);
		pid.nb_pck = 0;
		pid.done_eos = false;

		if (this.fwd==true) {
			pid.opid = this.new_pid();
			pid.opid.copy_props(pid);
		}
	} else {
		print(GF_LOG_INFO, "Reconfigure Pid " + pid.name);		
	}
   return;

	let i=0;
	while (1) {
		let prop = pid.enum_properties(i);
		if (!prop) break;
		i++;
		let str = "Prop ";
		str += prop.name;
		str += " (type ";
		str += prop.type;
		str += " ): ";
		str += JSON.stringify(prop.value);
		print(GF_LOG_INFO, str);
	}

	let prop = pid.get_prop("CodecID");
	print(GF_LOG_INFO, "Parent filter is " + pid.filter_name);
	print(GF_LOG_INFO, "Args are " + pid.args);
	print(GF_LOG_INFO, "Source filter is " + pid.src_name);
	print(GF_LOG_INFO, "Source args are " + pid.src_args);
}

filter.process = function()
{
	this.pids.forEach(function(pid) {
		while (1) {

			let pck = pid.get_packet();
			if (!pck) {
				if (pid.eos && !pid.done_eos) {
					print(GF_LOG_INFO, "pid is eos");

/*					let stats = pid.get_stats(GF_STATS_LOCAL);
					for (let p in stats) {
						print(GF_LOG_INFO, "prop " + p + ": " + stats[p]);
					}
*/
					pid.done_eos = true;
				}
				return;
			}
			pid.nb_pck++;
				pid.drop_packet();
				continue;

			print(GF_LOG_INFO, "PID" + pid.name + " PCK" + pid.nb_pck + " DTS " + pck.dts + " CTS " + pck.cts + " SAP " + pck.sap + " size " + pck.size);
			let i=0;
			while (1) {
				let prop = pck.enum_properties(i);
				if (!prop) break;
				i++;
				let str = "Prop ";
				str += prop.name;
				str += " (type ";
				str += prop.type;
				str += " ): ";
				str += prop.value;
				print(GF_LOG_INFO, str);
			}
			let data = pck.data;
			if (data) {
				let view = new Uint8Array(data);
				print('got buffer, size ' + view.length + ' first 4 bytes ' + view[0].toString(16) + '' + view[1].toString(16) + '' + view[2].toString(16) + '' + view[3].toString(16));
			}

			if (filter.fwd==true) {
				if (pid.nb_pck<500)
					pid.opid.forward(pck);
				else {
					let shared = false;
					if (pck.frame_ifce);
						shared = true;

					let dst = pid.opid.new_packet(pck, shared);
					dst.copy_props(pck);
					dst.set_prop("MyProp", "great", true);
					dst.send();
				}
			}

			pid.drop_packet();

			if (pid.nb_pck>1000) {
				pid.discard = true;
				return;
			}

		}
	}
	);
}

filter.remove_pid = function(pid)
{
	let index = this.pids.indexOf(pid);
    if (index > -1) {
      this.pids.splice(index, 1);
   }
}

