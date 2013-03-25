'use strict';
/*****************************************************************************************
 *
 * Example code using the Media Source Extension API with GPAC
 *
 *****************************************************************************************/
alert("Media Source Script loaded");

var v;
var r;
var t;
var qe;
var dte;
var vte;
var be;
var ue;

var use_regulation = true;

/* difference in seconds between the latest media time downloaded and the media time being played
used to trigger a new download when the regulation mode is on */
var TIME_THRESOLD = 2;

/* Current file being downloaded */
var fileIndex = -1;

/* Current quality being downloaded */
var qualityIndex = 0;

/* using multiple XHR objects (1 per file, because of GPAC bug in the allocation of arraybuffer) */
var xhrs = new Array();
var xhr = new XMLHttpRequest();

/* Using only one SourceBuffer in this example */
var sb = null;

/* URL of the segment being downloaded */
var url = null;

/* maximum number of segments to download */
var maxSeg = Infinity;

/* Array representing the download pattern */
/* download order of each segments (for out-of-order append), segments not listed are downloaded in the right order */
//var appendOrder = null;
var appendOrder = [0, 5, 4, 3, 2, 1];
var fileIndexReordered;
var prevFileIndex = 0;

function createInfoStructure(g) {
    var ta;
    ta = document.createElement("textArea");
    ta.setAttribute("width", "800");
    g.appendChild(ta);

    qe = document.createElement("tspan");
    ta.appendChild(qe);
    ta.appendChild(document.createElement("tbreak"));
    dte = document.createElement("tspan");
    ta.appendChild(dte);
    ta.appendChild(document.createElement("tbreak"));
    vte = document.createElement("tspan");
    ta.appendChild(vte);
    ta.appendChild(document.createElement("tbreak"));
    be = document.createElement("tspan");
    ta.appendChild(be);
    ta.appendChild(document.createElement("tbreak"));
    ue = document.createElement("tspan");
    ta.appendChild(ue);
}

function updateInfo() {
    dte.textContent = "Document time        :" + document.documentElement.getCurrentTime();
    vte.textContent = "Video time           :" + v.currentTime;
    qe.textContent = "Quality Requested:" + qualityIndex;
    if (sb != null && sb.buffered.length > 0) {
     be.textContent = "Media Data Buffered: (" + sb.buffered.start(0) + "," + sb.buffered.end(0) + ")";
    } else {
        be.textContent = "empty buffer";
    }
}

function updateDownloadInfo(s) {
    ue.textContent = s;
}

function switchQuality() {
    alert('Switching quality');
    qualityIndex = (qualityIndex + 1) % segmentFiles.length;
    updateInfo();
}

function getNextSegment() {

    /* apply the limit (if any) to the number of segments to download */
    if (maxSeg > 0 && fileIndex >= maxSeg) return;

    fileIndex++;

    /* apply the segment reordering pattern (if any) */
    if (appendOrder != null && fileIndex < appendOrder.length) {
        fileIndexReordered = appendOrder[fileIndex];
        alert("Changing file index from: " + fileIndex + "to : " + fileIndexReordered);
    } else {
        fileIndexReordered = fileIndex;
        alert("Using file index: " + fileIndex);
    }

    /* determine the url to use */
    if (fileIndexReordered < segmentFiles[qualityIndex].segStartIndex) {
        /* Setting the url with the initialization segment */
        url = segmentFiles[qualityIndex].baseURL + segmentFiles[qualityIndex].initName;
    }
    else if (fileIndexReordered >= segmentFiles[qualityIndex].segStartIndex && fileIndexReordered < segmentFiles[qualityIndex].segEndIndex) {
        /* Setting the url with a regular media segment */
        url = segmentFiles[qualityIndex].baseURL + segmentFiles[qualityIndex].segmentPrefix + fileIndexReordered + segmentFiles[qualityIndex].segmentSuffix;
    }

    alert("url: " + url);
    /* starting the download */
    if (url) {
        updateDownloadInfo("Downloading segment #" + fileIndexReordered + " " + url);
        xhrs[fileIndex] = xhr;
        xhrs[fileIndex].url = url;
        xhrs[fileIndex].open("GET", url);
        xhrs[fileIndex].responseType = "arraybuffer";
        xhrs[fileIndex].onreadystatechange = onDone;
        xhrs[fileIndex].send();
    }

    updateInfo();
}

function onDone(e) {
    if (this.readyState == this.DONE) {
        updateDownloadInfo("Downloading segment #" + fileIndexReordered + " " + this.url + " - done");
        var arraybuffer = this.response;
        alert("Received ArrayBuffer (size: " + arraybuffer.byteLength + ")");

        /* Appending the downloaded segment to the SourceBuffer */
        if (sb) {
            /* if there is a discontinuity in the file index, we need to inform the SourceBuffer */
            if (prevFileIndex != 0 && fileIndexReordered != prevFileIndex + 1) {
                sb.abort("continuation");
            }
            prevFileIndex = fileIndexReordered;
            /* we assume everything will be fine with this append */
            sb.appendBuffer(arraybuffer);
        }
        url = null;

        if (!use_regulation) {
            getNextSegment();
        }
    }
}

function onSourceOpen(event) {
    alert("MediaSource opened");
    /* GPAC Hack: since the event is not targeted to the MediaSource, we need to get the MediaSource back */
    var ms = event.target.ms;

    alert("Adding Source Buffer of type video/mp4 to the MediaSource");
    sb = ms.addSourceBuffer("video/mp4");

    getNextSegment();
}

/* Function called repeatedly to monitor the playback time versus download time difference 
and to trigger a new download if needed */
function repeatFunc() {
    updateInfo();

    /* don't download a new segment if:
    - the SourceBuffer is not created
    - there is already a download going on */
    if (sb != null && url == null) {
        if (sb.buffered.length > 0) {
            alert("Source buffered range: (" + sb.buffered.start(0) + "," + sb.buffered.end(0) + ")");
            if (sb.updating == false && sb.buffered.end(0) - v.currentTime < TIME_THRESOLD) {
                alert("buffered attribute has too few data, downloading new segment");
                getNextSegment();
            }
        }
        else {
            if (!sb.updating) {
                alert("buffered attribute has no data, downloading new segment");
                getNextSegment();
            }
        }
    }
}

function init() {

    v = document.getElementById("v");
    /* GPAC Hack: the event should be dispatched to the MediaSource object not to the video */
    v.addEventListener("sourceopen", onSourceOpen);

    if (use_regulation) {
        /* GPAC workaround: adding the event listener on the repeatEvent of a dummy animation to simulate window.setInterval */
        var a = document.getElementById("a");
        a.addEventListener("repeatEvent", repeatFunc);
        alert("Using regulation - downloading segments as fast as possible");
    } else {
        alert("Not using any regulation - downloading segments as fast as possible");
    }

    /* adding listener to toggle video quality */
    r = document.getElementById("r");
    r.addEventListener("click", switchQuality);

    t = document.getElementById("t");
    createInfoStructure(t);

    alert("Creating new MediaSource");
    var ms = new MediaSource();

    var bloburl = URL.createObjectURL(ms);
    alert("Attaching Media Source " + bloburl + " to Video");
    v.src = bloburl;

    /* GPAC hack to retrieve the MediaSource from the video when the sourceopen event is dispatched to the video */
    v.ms = ms;

    alert("Starting video playback");
    v.play();
}
