'use strict';
/*****************************************************************************************
 *
 * Example code using the Media Source Extension API with GPAC
 *
 *****************************************************************************************/
alert("Media Source Script loaded");

var DEBUG = true;
var UPDATE_TEXT_INFO = true;

function reportMessage(msg) {
	if (DEBUG) {
		alert(msg);
	}
}

function createLabel(x, y, fill, textContent)
{
    var label = document.createElement("text");
	label.setAttribute("x", x);
	label.setAttribute("y", y);
	label.setAttribute("fill", fill);
	label.textContent = textContent;
	return label;
}

function createBar(x, y, w, h, fill) {
    var bar = document.createElement("rect");
	bar.setAttribute("x", x);
	bar.setAttribute("y", y);
	bar.setAttribute("width", w);
	bar.setAttribute("height", h);
	bar.setAttribute("rx", h/2);
	bar.setAttribute("ry", h/2);
	bar.setAttribute("fill", fill);
	return bar;
}

Player.prototype.createInfoStructure = function(id) {
	this.info = {};
	var width = 330;
    var ta;
	var g = document.getElementById(id);
    ta = document.createElement("textArea");
    ta.setAttribute("x", 100);
    ta.setAttribute("y", -50);
	ta.setAttribute("width", width);
    g.appendChild(ta);
	/* status info */
    this.info.se = document.createElement("tspan");
    ta.appendChild(this.info.se);
    ta.appendChild(document.createElement("tbreak"));
	/* Quality info */
    this.info.qe = document.createElement("tspan");
    ta.appendChild(this.info.qe);
    ta.appendChild(document.createElement("tbreak"));
	/* Document time info */
    this.info.dte = document.createElement("tspan");
    //ta.appendChild(this.info.dte);
    //ta.appendChild(document.createElement("tbreak"));
	/* Video time info */
    this.info.vte = document.createElement("tspan");
    //ta.appendChild(this.info.vte);
    //ta.appendChild(document.createElement("tbreak"));
	/* Buffer info */
    this.info.be = document.createElement("tspan");
    //ta.appendChild(this.info.be);
    //ta.appendChild(document.createElement("tbreak"));
	/* URL info */
    this.info.ue = document.createElement("tspan");
    ta.appendChild(this.info.ue);

	this.ui = {};
    this.ui.playbackRect = createBar(10, 5, width-10, 5, "black");
	g.appendChild(this.ui.playbackRect);
	
    this.ui.bufferedRect = createBar(10, 5, 0, 5, "url(#backColor)");
	g.appendChild(this.ui.bufferedRect);
	this.ui.bufferedRect.max = width;
	
    this.ui.zeroLabel = createLabel(0, 10, "black", '0');
	g.appendChild(this.ui.zeroLabel);
	
    this.ui.playLabel = createLabel(0, 20, "black", '');
	g.appendChild(this.ui.playLabel);

    this.ui.playBar = createBar(0, 5, 1, 5, "black");
	g.appendChild(this.ui.playBar);
	
    this.ui.startLabel = createLabel(0, 0, "black", '');
	g.appendChild(this.ui.startLabel);

    this.ui.endLabel = createLabel(width+5, 10, "black", 'end');
	g.appendChild(this.ui.endLabel);
}

Player.prototype.updateInfo = function() {
	if (!UPDATE_TEXT_INFO) return;
	var start = 0;
	var end = 1;
	var nowVideo = this.v.currentTime.toFixed(3);
//    this.info.dte.textContent = "Document time:" + document.documentElement.getCurrentTime();
//    this.info.vte.textContent = "Video time: " + nowVideo;
	if (this.qualityChangeRequested) {
		this.info.qe.textContent = "Quality : " + (this.qualityIndex - this.qualityChangeRequested) + "/"+(this.segmentFiles.length-1)+", requested: "+this.qualityIndex;
	} else {
		this.info.qe.textContent = "Quality : " + this.qualityIndex + "/"+(this.segmentFiles.length-1);
	}
    if (this.sb != null && this.sb.buffered.length > 0) {
		start = this.sb.buffered.start(0).toFixed(3);
		end = this.sb.buffered.end(0).toFixed(3);
        //this.info.be.textContent = "Media Data Buffered: (" + start + "," + end + ")";
    } else {
        //this.info.be.textContent = "empty buffer";
    }
	this.ui.playLabel.textContent = nowVideo;
	this.ui.playLabel.setAttribute("x", nowVideo/end*this.ui.bufferedRect.max);
	this.ui.playBar.setAttribute("x", nowVideo/end*this.ui.bufferedRect.max);
	this.ui.startLabel.textContent = start;
	this.ui.endLabel.textContent = end;
	this.ui.startLabel.setAttribute("x", start/end*this.ui.bufferedRect.max);
	this.ui.bufferedRect.setAttribute("x", start/end*this.ui.bufferedRect.max);
	this.ui.bufferedRect.setAttribute("width", (end - start)/end*this.ui.bufferedRect.max);
}

Player.prototype.updateDownloadInfo = function(index, url, done) {
	if (!UPDATE_TEXT_INFO) return;
    this.info.ue.textContent = "Segment #" + index + " ..." + url.slice(-30);
	this.info.se.textContent = "Download Status:"+ (done ?" done" : " in progress");
}

Player.prototype.toggleQuality = function() {
    reportMessage("[video "+this.v.getAttribute("id")+"] Toggling quality");
	var newQuality = (this.qualityIndex + 1) % this.segmentFiles.length;
	this.qualityChangeRequested = (newQuality - this.qualityIndex);
    this.qualityIndex = newQuality;
    this.updateInfo();
}

Player.prototype.switchUp = function() {
    reportMessage("[video "+this.v.getAttribute("id")+"] Switching quality up");
	if (this.qualityIndex < this.segmentFiles.length - 1) {
		this.qualityIndex++;
		this.qualityChangeRequested++;
	}
    this.updateInfo();
}

Player.prototype.switchDown = function() {
    reportMessage("[video "+this.v.getAttribute("id")+"] Switching quality down");
	if (this.qualityIndex > 0) {
		this.qualityIndex--;
		this.qualityChangeRequested--;
	}
    this.updateInfo();
}

Player.prototype.getNextSegment = function () {
    /* apply the limit (if any) to the number of segments to download */
    if (this.maxSeg > 0 && this.fileIndex >= this.maxSeg && this.playing) return;

	if (!this.qualityChangeRequested) {
		/* we increment the segment index only when we need a media segment (not an init segment) */
		this.fileIndex++;
	} 

    /* apply the segment reordering pattern (if any) */
    if (this.appendOrder != null && this.fileIndex < this.appendOrder.length) {
        this.fileIndexReordered = this.appendOrder[this.fileIndex];
        reportMessage("[video "+this.v.getAttribute("id")+"] Changing file index from: " + this.fileIndex + "to : " + this.fileIndexReordered);
    } else {
        this.fileIndexReordered = this.fileIndex;
        reportMessage("[video "+this.v.getAttribute("id")+"] Using file index: " + this.fileIndex);
    }

    /* determine the url to use */
    if (this.qualityChangeRequested || this.fileIndexReordered < this.segmentFiles[this.qualityIndex].segStartIndex) {
        /* Setting the url with the initialization segment */
        this.url = this.segmentFiles[this.qualityIndex].baseURL + this.segmentFiles[this.qualityIndex].initName;
    }
    else if (this.fileIndexReordered >= this.segmentFiles[this.qualityIndex].segStartIndex 
	         && this.fileIndexReordered < this.segmentFiles[this.qualityIndex].segEndIndex) {
        /* Setting the url with a regular media segment */
        this.url = this.segmentFiles[this.qualityIndex].baseURL 
		         + this.segmentFiles[this.qualityIndex].segmentPrefix 
				 + this.fileIndexReordered 
				 + this.segmentFiles[this.qualityIndex].segmentSuffix;
    }

    reportMessage("[video "+this.v.getAttribute("id")+"] Downloading resource from url: " + this.url);
    /* starting the download */
    if (this.url) {
        this.updateDownloadInfo(this.fileIndexReordered, this.url);
        this.xhr.url = this.url;
		this.xhr.qualityChangeRequested = this.qualityChangeRequested;
        this.xhr.open("GET", this.url);
        this.xhr.responseType = "arraybuffer";
        this.xhr.onreadystatechange = onDone;
        this.xhr.send();
    }

    this.updateInfo();
}

function onDone(e) {
    if (this.readyState == this.DONE) {
        this.player.updateDownloadInfo(this.player.fileIndexReordered, this.url, true);
        var arraybuffer = this.response;
        reportMessage("[video "+this.player.v.getAttribute("id")+"] Received ArrayBuffer (size: " + arraybuffer.byteLength + ")");
        /* Appending the downloaded segment to the SourceBuffer */
        if (this.player.sb) {
            /* if there is a discontinuity in the file index, we need to inform the SourceBuffer */
            if (this.player.prevFileIndex != 0 && this.player.fileIndexReordered != this.player.prevFileIndex + 1) {
                this.player.sb.abort("continuation");
            }
            this.player.prevFileIndex = this.player.fileIndexReordered;
            /* we assume everything will be fine with this append */
            this.player.sb.appendBuffer(arraybuffer);
        }
        this.player.url = null;
		if (this.qualityChangeRequested != 0) {
			this.player.qualityChangeRequested = 0;
		}		
		if (!this.player.use_regulation) {
            this.player.getNextSegment();
        }
    }
}

Player.prototype.onSourceOpen = function (event) {
    reportMessage("[video "+this.v.getAttribute("id")+"] MediaSource opened");
    /* GPAC Hack: since the event is not targeted to the MediaSource, we need to get the MediaSource back */
    var ms = event.target.ms;

    reportMessage("[video "+this.v.getAttribute("id")+"] Adding Source Buffer of type video/mp4 to the MediaSource");
    ms.player.sb = ms.addSourceBuffer("video/mp4");
	ms.player.sb.id = ms.sourceBuffers.length;
    ms.player.getNextSegment();
}

Player.prototype.checkBufferLevel = function() {
    /* don't download a new segment if:
    - the SourceBuffer is not created
    - there is already a download going on */
    if (this.sb != null && this.url == null) {
        if (this.sb.buffered.length > 0) {
			if (this.v.currentTime >= this.sb.buffered.end(0)) {
				this.pause();
			} else {
				reportMessage("[video "+this.v.getAttribute("id")+"] Video time "+this.v.currentTime + ", Source Buffer #"+this.sb.id+" buffered data range: (" + this.sb.buffered.start(0) + "," + this.sb.buffered.end(0) + ")");
				if (this.sb.updating == false && this.sb.buffered.end(0) - this.v.currentTime < this.TIME_THRESOLD) {
					reportMessage("[video "+this.v.getAttribute("id")+"] buffered attribute has not enough data, downloading new segment");
					this.getNextSegment();
				}
			}
        }
        else {
            if (!this.sb.updating) {
                reportMessage("[video "+this.v.getAttribute("id")+"] buffered attribute has no data, downloading new segment");
                this.getNextSegment();
            }
        }
    }
}

/* Function called repeatedly to monitor the playback time versus download time difference 
and to trigger a new download if needed */
Player.prototype.repeatFunc = function() {
	if (!this.playing) return;
    this.updateInfo();
	this.checkBufferLevel();
}

Player.prototype.play = function() {
  reportMessage("[video "+this.v.getAttribute("id")+"] Starting video playback");
  this.playing = true;
  this.v.play();
}

Player.prototype.pause = function() {
  reportMessage("[video "+this.v.getAttribute("id")+"] Pausing video playback");
  this.playing = false;
  this.v.pause();
}

Player.prototype.changeSegmentList = function(newSegments) {
  this.segmentFiles = newSegments;
  this.qualityChangeRequested++;
}

/* 
   vId: id of the video element
   iId: id of a group where debug info will be displayed 
   aId: id of a animation used to refresh downloads
*/
function Player(vId, iId, aId, segmentFiles, segmentOrder) {
	reportMessage("[video "+vId+"] Creating Player");
	/* Boolean to define the download policy: 
		- depending on the buffer occupancy (true) (see TIME_THRESOLD below), 
		- as fast as possible (false) */
	this.use_regulation = true;
	/* difference in seconds between the latest media time downloaded and the media time being played, 
	used to trigger a new download when the regulation mode is on */
	this.TIME_THRESOLD = 2;
	/* Current file being downloaded */
	this.fileIndex = -1;
	/* URL of the segment being downloaded */
	this.url = null;
	/* Current quality being downloaded */
	this.qualityIndex = 0;
	this.qualityChangeRequested = 0;
	/* using 1 XHR for all downloads */
	this.xhr = new XMLHttpRequest();
	this.xhr.player = this;
	/* The source buffer used by the player (only one at the moment) */
	this.sb = null;
	/* maximum number of segments to download */
	this.maxSeg = Infinity;
	this.playing = false;
	/* Array representing the download order of each segments (for out-of-order append), 
	  segments not listed are downloaded in the right order */
	this.appendOrder = segmentOrder; 
	this.fileIndexReordered = 0;
	this.prevFileIndex =  0;
	/* references to objects used to display information regarding this player */
	this.info = null;

	this.segmentFiles = segmentFiles;
	
    if (this.use_regulation) {
        /* GPAC workaround: adding the event listener on the repeatEvent of a dummy animation to simulate window.setInterval */
		var animation = document.getElementById(aId);
        animation.addEventListener("repeatEvent", this.repeatFunc.bind(this));
        reportMessage("[video "+vId+"] Using buffer regulation to download segments ");
    } else {
        reportMessage("[video "+vId+"] Not using any regulation - downloading segments as fast as possible");
    }

    this.createInfoStructure(iId);

    reportMessage("[video "+vId+"] Creating new MediaSource");
    this.ms = new MediaSource();
	this.ms.player = this;

    var bloburl = URL.createObjectURL(this.ms);
    reportMessage("[video "+vId+"] Attaching Media Source " + bloburl + " to video element");

    this.v = document.getElementById(vId);
    /* GPAC Hack: the event should be dispatched to the MediaSource object not to the video */
    this.v.addEventListener("sourceopen", this.onSourceOpen.bind(this));
    this.v.src = bloburl;

    /* GPAC hack to retrieve the MediaSource from the video when the sourceopen event is dispatched to the video */
    this.v.ms = this.ms;

	return this;
}
