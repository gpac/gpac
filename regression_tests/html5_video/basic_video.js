/*****************************************************************************************
 * Testing support for the basic HTML5 Video object
 *****************************************************************************************/

alert("Script loaded");

function enumerate(e)
{
	for(var propertyName in e) {
		alert(" "+propertyName+": "+e[propertyName]);
	}
}

function print_ranges(r) {
    alert(" time ranges length:" + r.length);
    for (var i = 0; i < r.length; i++) {
        alert(" (" + r.start(i) + "," + r.end(i) + ")");
    }
}

function print_tracks(t) {
}

function print_media_info()
{
    var v = document.getElementById("v");
	alert("");
	alert("Element: "+v);
	alert("HTMLMediaElement Properties: ");
	alert("src:          "+v.src);
	alert("error & code: "+v.error+":"+v.error.code);
	alert("currentSrc:   "+v.currentSrc);
	alert("crossOrigin:  "+v.crossOrigin);
	alert("networkState: "+v.networkState);
	alert("preload:      "+v.preload);
	alert("buffered:     "+v.buffered);
	print_ranges(v.buffered);
	alert("readyState:   "+v.readyState);
	alert("seeking:      "+v.seeking);
	alert("currentTime:  "+v.currentTime);
	alert("duration:     "+v.duration);
	alert("startDate:    "+v.startDate);
	alert("paused:       "+v.paused);
	alert("def playrate: "+v.defaultPlaybackRate);
	alert("play rate:    "+v.playbackRate);
	alert("played:       "+v.played);
	print_ranges(v.played);
	alert("seekable:     "+v.seekable);
	print_ranges(v.seekable);
	alert("ended:        "+v.ended);
	alert("autoplay:     "+v.autoplay);
	alert("loop:         "+v.loop);
	alert("mediaGroup:   "+v.mediaGroup);
	alert("controller:   "+v.controller);
	alert("controls:     "+v.controls);
	alert("volume:       "+v.volume);
	alert("muted:        "+v.muted);
	alert("defaultMuted: "+v.defaultMuted);
	alert("audioTracks:  "+v.audioTracks+ " length ("+v.audioTracks.length+")");
	print_tracks(v.audioTracks);
	alert("videoTracks:  "+v.videoTracks+ " length ("+v.videoTracks.length+")");
	print_tracks(v.videoTracks);
	alert("textTracks:   "+v.textTracks+ " length ("+v.textTracks.length+")");
	print_tracks(v.textTracks);
	alert("");
	alert("HTMLVideoElement specific properties: ");
	alert("width:        "+v.width);
	alert("height:       "+v.height);
	alert("wideoWidth:   "+v.videoWidth);
	alert("videoHeight:  "+v.videoHeight);
	alert("poster:       "+v.poster);
	alert("");
}

function init()
{	
	var v = document.getElementById("v");
	v.src = "media/counter-10mn_I25_640x360_160kbps_openGOP_dash.mp4";
	v.addEventListener("click", print_media_info);
	enumerate(v);
}