
filter.pids = [];

//metadata
filter.set_name("JSGen");
filter.set_desc("JS-based packet generator filter");
filter.set_version("0.1beta");
filter.set_author("GPAC team");
filter.set_help("This filter provides a very simple javascript text packet generator");

//exposed arguments
filter.set_arg("str", "string to send", GF_PROP_STRING, "GPAC JS Filter Packet");
filter.max_pids = -1;

filter.set_cap("StreamType", "Text", true);
filter.set_cap("CodecID", "subs", true);

filter.my_shared_string = "Shared data test";

filter.initialize = function() {
 print(GF_LOG_DEBUG, "Init at " + Date() + " nb pids " + this.pids.length);

 this.opid = this.new_pid();
 this.opid.set_prop("StreamType", "Text");
 this.opid.set_prop("CodecID", "subs");
 this.opid.set_prop("Timescale", "1000");
// this.opid.set_prop("DecoderConfig", "My Super Config");
 this.opid.nb_pck = 0;
 let ab = new ArrayBuffer(11);
 let abv = new Uint8Array(ab);
 for (let i=0; i<10; i++)
 	abv[i] = 48+i;
 
 this.opid.set_prop("DecoderConfig", ab);

 }

filter.update_arg = function(name, val)
{
	print(GF_LOG_INFO, "Value " + name + " is " + val);
}

filter.post_task( function() {
	if (this.opid == null) {
		print(GF_LOG_INFO, "last task callback at " + Date() );
		return -1;
	}
	print(GF_LOG_INFO, "task callback at " + Date() );
	return 1000;
})

filter.process = function()
{
	if (!this.opid)
		return GF_EOS;

	if (this.opid.nb_pck>=100) {
 		this.opid.eos = true;
 		this.opid.remove();
 		this.opid = null;
 		return;
	}
	this.opid.nb_pck++;
	
	print(GF_LOG_INFO, "Pck " + this.opid.nb_pck);
	let pck;
	if (this.opid.nb_pck<50) {
		pck = this.opid.new_packet(this.my_shared_string, true);
		this.my_shared_string += ".";
		pck.cts = 1000*this.opid.nb_pck;
	} else {
		pck = this.opid.new_packet("Packet number " + this.opid.nb_pck);
		pck.cts = 1000*this.opid.nb_pck;
		if (this.opid.nb_pck>90)
			pck.truncate(pck.size / 2);
		if (this.opid.nb_pck>80)
			pck.append("\nMore data at CTS " + pck.cts);
	}
	pck.dts = 1000*this.opid.nb_pck;
	pck.dur = 1000;
	pck.sap = GF_FILTER_SAP_1;
	pck.set_prop("MyProp", "great", true);
	pck.send();
}

filter.remove_pid = function(pid)
{
}

