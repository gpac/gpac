'use strict';
/*****************************************************************************************
 *
 * Example code using the Media Source Extension API with GPAC
 *
 *****************************************************************************************/
alert("Media Source Script loaded");

var baseURL = "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-main-multi/"
var medias = [
    { url: "mp4-main-multi-mpd-V-BS_init.mp4", offset: 0 },
    { url: "mp4-main-multi-h264bl_low-1.m4s", offset: 0 },
    { url: "mp4-main-multi-h264bl_low-1.m4s", offset: 10 },
    { url: "mp4-main-multi-h264bl_low-1.m4s", offset: 15 },
    { url: "mp4-main-multi-h264bl_low-1.m4s", offset: 20 },
    { url: "mp4-main-multi-h264bl_low-1.m4s", offset: 25 },
    { url: "mp4-main-multi-h264bl_low-1.m4s", offset: 30 }
];

function addMediaToTimelineAndPlay(sb, i)
{
    var xhr = new XMLHttpRequest();
    xhr.sb = sb;
    xhr.index = (i == undefined ? 0 : i);
    xhr.timeOffset = medias[xhr.index].offset;
    xhr.open("GET", baseURL+medias[xhr.index].url);
    xhr.responseType = "arraybuffer";
    xhr.onreadystatechange = function () {
        if (this.readyState == this.DONE) {
            if (this.sb.updating == false) {
                this.sb.timestampOffset = this.timeOffset;
                this.sb.appendBuffer(this.response);
                if (this.index < medias.length) {
                    addMediaToTimelineAndPlay(this.sb, this.index+1);
                } else {
                    this.sb.v.play();
                }
            } else {
                alert("two appends at the same time");
            }
        }
    };
    xhr.send(); 
}

function onSourceOpen(event) {
    var sb;
    alert("MediaSource opened");
    /* GPAC Hack: since the event is not targeted to the MediaSource, we need to get the MediaSource back */
    var v = event.target;
    var ms = v.ms;
    sb = ms.addSourceBuffer("video/mp4");
    sb.v = v;
    addMediaToTimelineAndPlay(sb);
}

function init() {

    var v = document.getElementById("v");
    /* GPAC Hack: the event should be dispatched to the MediaSource object not to the video */
    v.addEventListener("sourceopen", onSourceOpen);

    alert("Creating new MediaSource");
    var ms = new MediaSource();
    v.src = URL.createObjectURL(ms);

    /* GPAC hack to retrieve the MediaSource from the video when the sourceopen event is dispatched to the video */
    v.ms = ms;

}

