/* code retrieved and adapted from http://www.w3.org/2010/05/video/mediaevents.html */
var svgNS = "http://www.w3.org/2000/svg";

var interaction_buttons = [ 
	{ text: "load()", action: load },
	{ text: "play()", action: function() { document._video.play(); } },
	{ text: "pause()", action: function() { document._video.pause(); } },
	{ text: "currentTime+=10", action: function() { document._video.currentTime+=10; } },
	{ text: "currentTime-=10", action: function() { document._video.currentTime-=10; } },
	{ text: "playbackRate++", action: function() { document._video.playbackRate++; } },
	{ text: "playbackRate--", action: function() { document._video.playbackRate--; } },
	{ text: "playbackRate+=0.1", action: function() { document._video.playbackRate+=0.1; } },
	{ text: "playbackRate-=0.1", action: function() { document._video.playbackRate-=0.1; } },
	{ text: "volume+=0.1", action: function() { document._video.volume+=0.1; } },
	{ text: "volume-=0.1", action: function() { document._video.volume-=0.1; } },
	{ text: "muted=true", action: function() { document._video.muted=true; } },
	{ text: "muted=false", action: function() { document._video.muted=false; } },
	{ text: "Update", action: function() { update_properties(); } },
	{ text: "Add Listeners", action: function() { init_events(); } },
	{ text: "Remove Listeners", action: function() { remove_events(); } }
];

var movie_buttons = [ 
	{ text: "Sintel Teaser", action: function() { switchVideo(0); } },
	{ text: "Bunny trailer", action: function() { switchVideo(1); } },
	{ text: "Bunny movie", action: function() { switchVideo(2); } },
	{ text: "Test movie", action: function() { switchVideo(3); } },
	{ text: "Local movie", action: function() { switchVideo(4); } }
];

var media_events = new Array();
media_events["loadstart"] = 0;
media_events["progress"] = 0;
media_events["suspend"] = 0;
media_events["abort"] = 0;
media_events["error"] = 0;
media_events["emptied"] = 0;
media_events["stalled"] = 0;
media_events["loadedmetadata"] = 0;
media_events["loadeddata"] = 0;
media_events["canplay"] = 0;
media_events["canplaythrough"] = 0;
media_events["playing"] = 0;
media_events["waiting"] = 0;
media_events["seeking"] = 0;
media_events["seeked"] = 0;
media_events["ended"] = 0;
media_events["durationchange"] = 0;
media_events["timeupdate"] = 0;
media_events["play"] = 0;
media_events["pause"] = 0;
media_events["ratechange"] = 0;
media_events["volumechange"] = 0;

var media_controller_events = new Array();

// was extracted from the spec in January 2013
media_controller_events["emptied"] = 0;
media_controller_events["loadedmetadata"] = 0;
media_controller_events["loadeddata"] = 0;
media_controller_events["canplay"] = 0;
media_controller_events["canplaythrough"] = 0;
media_controller_events["playing"] = 0;
media_controller_events["ended"] = 0;
media_controller_events["waiting"] = 0;
media_controller_events["ended"] = 0;
media_controller_events["durationchange"] = 0;
media_controller_events["timeupdate"] = 0;
media_controller_events["play"] = 0;
media_controller_events["pause"] = 0;
media_controller_events["ratechange"] = 0;
media_controller_events["volumechange"] = 0;

// was extracted from the spec in January 2013
var media_properties = [ "error", "src", "currentSrc", "crossOrigin", "networkState", "preload", "buffered", "readyState", "seeking", "currentTime", "duration", "startDate", "paused", "defaultPlaybackRate", "playbackRate", "played", "seekable", "ended", "autoplay", "loop", "mediaGroup", "controller", "controls", "volume", "muted", "defaultMuted", "audioTracks", "videoTracks", "textTracks", "width", "height", "videoWidth", "videoHeight", "poster" ];

var media_properties_elts = new Array();

function init() {
    document._video = document.getElementById("video");

	createInteractionButtons();
	createMovieButtons();
	createEventTable();
	createMediaProperties();

    init_properties();
}

function createButtons(buttons, offset) {
	var buttonsG = document.getElementById("buttons");
	for (var i = 0; i < buttons.length; i++) {
	   var g = document.createElementNS(svgNS, "g");
	   g.setAttributeNS(svgNS, "transform", "translate(0, "+(offset+i*21)+")");
	   buttonsG.appendChild(g);
	
	   var rect = document.createElementNS(svgNS, "rect");
	   rect.setAttributeNS(svgNS, "width", "120");
	   rect.setAttributeNS(svgNS, "height", "20");
	   rect.setAttributeNS(svgNS, "fill", "lightgrey");
	   rect.addEventListener("click", buttons[i].action, false);
	   g.appendChild(rect);

	   var text = document.createElementNS(svgNS, "text");
	   text.setAttributeNS(svgNS, "x", "60");
	   text.setAttributeNS(svgNS, "y", "15");
	   text.setAttributeNS(svgNS, "text-align", "middle");
	   text.setAttributeNS(svgNS, "text-anchor", "middle");
	   text.textContent = buttons[i].text;
	   g.appendChild(text);
	}
}

function createInteractionButtons() {
	createButtons(interaction_buttons, 0);
}

function createMovieButtons() {
	createButtons(movie_buttons, 500);
}

function createMediaProperties() {
	var propertiesG = document.getElementById("properties_table");
	var title = document.createElementNS(svgNS, "text");
	title.setAttributeNS(svgNS, "font-weight", "bold");
	title.textContent = "Media Properties";
	propertiesG.appendChild(title);
	for (var i = 0; i < media_properties.length; i++) {
	   var g = document.createElementNS(svgNS, "g");
	   g.setAttributeNS(svgNS, "transform", "translate(0, "+((i+1)*21-10)+")");
	   propertiesG.appendChild(g);
	
	   var rect = document.createElementNS(svgNS, "rect");
	   rect.setAttributeNS(svgNS, "width", "120");
	   rect.setAttributeNS(svgNS, "height", "20");
	   rect.setAttributeNS(svgNS, "fill", "lightgrey");
	   g.appendChild(rect);

	   var label = document.createElementNS(svgNS, "text");
	   label.setAttributeNS(svgNS, "x", "60");
	   label.setAttributeNS(svgNS, "y", "15");
	   label.setAttributeNS(svgNS, "text-align", "middle");
	   label.setAttributeNS(svgNS, "text-anchor", "middle");
	   label.textContent = media_properties[i];
	   g.appendChild(label);
	   
	   var text = document.createElementNS(svgNS, "text");
	   text.setAttributeNS(svgNS, "x", "130");
	   text.setAttributeNS(svgNS, "y", "15");
	   text.textContent = "?";
	   g.appendChild(text);
	   media_properties_elts[i] = text;
	}
}

function createEventTableRow(event_name, yOffset) {
	var table = document.getElementById("event_table");
	var length = table.childNodes.length;

	var row = document.createElementNS(svgNS, "g");
	row.setAttributeNS(svgNS, "transform", "translate(0,"+(yOffset+length*21)+")");
	table.appendChild(row);

	var rect = document.createElementNS(svgNS, "rect");
	rect.setAttributeNS(svgNS, "width", "120");
	rect.setAttributeNS(svgNS, "height", "20");
	rect.setAttributeNS(svgNS, "fill", "lightgrey");
	row.appendChild(rect);

	var label = document.createElementNS(svgNS, "text");
	label.setAttributeNS(svgNS, "x", "60");
	label.setAttributeNS(svgNS, "y", "15");
	label.setAttributeNS(svgNS, "text-align", "middle");
	label.setAttributeNS(svgNS, "text-anchor", "middle");
	label.textContent = event_name;
	row.appendChild(label);

	var status = document.createElementNS(svgNS, "text");
	status.setAttributeNS(svgNS, "x", "130");
	status.setAttributeNS(svgNS, "y", "15");
	status.textContent = "no event yet";
	status.setAttributeNS(svgNS, "id", "e_"+event_name);
	row.appendChild(status);
}

function createEventTable() {
	var table = document.getElementById("event_table");
	var title = document.createElementNS(svgNS, "text");
	title.setAttributeNS(svgNS, "transform", "translate(0,0)");
	title.setAttributeNS(svgNS, "font-weight", "bold");
	title.textContent = "Media Events";
	table.appendChild(title);
	for (key in media_events) {	
		createEventTableRow(key, -10);
	}
}

function init_events() {
    for (key in media_events) {	
		document._video.addEventListener(key, capture, false);
		alert("Added event '"+key+"' to the video element");
    }
}

function remove_events() {
    for (key in media_events) {	
		document._video.removeEventListener(key, capture, false);
		alert("Removed event '"+key+"' to the video element");
    }
}

function init_properties() {
	update_properties();
}

function pad(n, width, z) {
	z = z || '0';
	n = n + '';
	return n.length >= width ? n : new Array(width - n.length + 1).join(z) + n;
}

function time() {
	var d = new Date();
	var t = (pad(d.getHours(), 2)+':'+pad(d.getMinutes(), 2)+':'+pad(d.getSeconds(),2)+'.'+pad(d.getMilliseconds(), 3))
	return t;
}

function readyStateString(i) {
	switch (i) {
		case 0: return "HAVE_NOTHING";
		case 1: return "HAVE_METADATA";
		case 2: return "HAVE_CURRENT_DATA";
		case 3: return "HAVE_FUTURE_DATA";
		case 4: return "HAVE_ENOUGH_DATA";
		default: return "PROBLEM";
	}
}

function networkStateString(i) {
	switch (i) {
		case 0: return "NETWORK_EMPTY";
		case 1: return "NETWORK_IDLE";
		case 2: return "NETWORK_LOADING";
		case 3: return "NETWORK_NO_SOURCE";
		default: return "PROBLEM";
	}
}

function reportVideoStates() {
	alert("Time "+time()+" networkState: "+networkStateString(document._video.networkState)+" readyState: "+readyStateString(document._video.readyState));
}

function capture(event) {
	alert("Time "+time()+" event "+event.type);
	reportVideoStates();
    media_events[event.type] ++;
    for (key in media_events) {	
		var e = document.getElementById("e_" + key);
		if (e) {
			e.textContent = ""+media_events[key];
			//if (media_events[key] > 0) e.className = "true";
		}
    }
    update_properties();
}

function update_properties() {
    var i = 0;
	var j;
    for (key in media_properties) {
		var prop = document._video[media_properties[key]];
		var val = prop;
		if (prop == "[object TimeRanges]") {
			val = "TimeRanges - length:"+prop.length;
			for (j = 0; j < prop.length; j++) {
			 val += ",("+prop.start(j)+";"+prop.end(j)+")";
			}
		}
		if (prop == "[object VideoTrackList]") {
			val = "VideoTrackList - length:"+prop.length;
		}
		if (prop == "[object AudioTrackList]") {
			val = "AudioTrackList - length:"+prop.length;
		}
		if (prop == "[object TextTrackList]") {
			val = "TextTrackList - length:"+prop.length;
		}
		media_properties_elts[i++].textContent = ""+val;
    }
    if (!!document._video.audioTracks) {
		var td = document.getElementById("m_audiotracks");
		td.textContent = ""+document._video.audioTracks.length;
    }
    if (!!document._video.videoTracks) {
		var td = document.getElementById("m_videotracks");
		td.textContent = ""+document._video.audioTracks.length;
    }
    if (!!document._video.textTracks) {
		var td = document.getElementById("m_texttracks");
		td.textContent = ""+document._video.audioTracks.length;
    }
}

var videos = new Array();

videos[0] = [
	     "http://media.w3.org/2010/05/sintel/poster.png",
	     "http://media.w3.org/2010/05/sintel/trailer.mp4",
	     "http://media.w3.org/2010/05/sintel/trailer.ogv",
	     "http://media.w3.org/2010/05/sintel/trailer.webm"
	     ];
videos[1] = [
	     "http://media.w3.org/2010/05/bunny/poster.png",
	     "http://media.w3.org/2010/05/bunny/trailer.mp4",
	     "http://media.w3.org/2010/05/bunny/trailer.ogv"
	     ];
videos[2] = [
	     "http://media.w3.org/2010/05/bunny/poster.png",
	     "http://media.w3.org/2010/05/bunny/movie.mp4",
	     "http://media.w3.org/2010/05/bunny/movie.ogv"
	     ];
videos[3] = [
	     "http://media.w3.org/2010/05/video/poster.png",
	     "http://media.w3.org/2010/05/video/movie_300.mp4",
	     "http://media.w3.org/2010/05/video/movie_300.ogv",
	     "http://media.w3.org/2010/05/video/movie_300.webm"
	     ];
videos[4] = [
	     "http://media.w3.org/2010/05/bunny/poster.png",
	     "http://127.0.0.1:8020/file.mp4",
	     ];

function switchVideo(n) {
	reportVideoStates();
	alert("Time "+time()+" switching video to "+videos[n][1]);

    if (n >= videos.length) n = 0;

    document._video.src = videos[n][1];
}

function load() {
    document._video.load();
	reportVideoStates();
}

init();
//document.addEventListener("DOMContentLoaded", init, false);