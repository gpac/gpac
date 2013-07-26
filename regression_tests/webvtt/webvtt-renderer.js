/* JavaScript Renderer for WebVTT tracks
   Generates GPAC Scene Graph (SVG, BIFS) to display WebVTT cues */
alert("WebVTT Rendered Loaded");

var DEBUG = false;

var rect = document.documentElement.getRectPresentationTrait("viewBox");
var width = rect.width;
var height = rect.height;
var xOffset = 5;
var yOffset = 5;
var fontSize = 20;
var textColor = "red";
var fontFamily = "SANS";
var lineIncrement = fontSize;
var nbCues = 0;

var cueArea = document.createElement("g");
document.documentElement.appendChild(cueArea);

function reportMessage(msg) {
	if (DEBUG) {
		alert(msg);
	}
}

function createTextArea(settingsObj) {
	reportMessage("Creating textArea with settings: "+settingsObj);
	var t = document.createElement("textArea");
	t.setAttribute("x", xOffset+settingsObj.xPosition);
	t.setAttribute("y", yOffset);
	if (settingsObj.fromTop) {
		t.setAttribute("display-align", "before");
		t.setAttribute("height", height-2*yOffset);
	} else {
		t.setAttribute("display-align", "after");
		t.setAttribute("height", height-2*yOffset-settingsObj.linePosition);
	}
	t.setAttribute("width", settingsObj.size-2*xOffset);
	t.setAttribute("fill", textColor);
	t.setAttribute("font-size", fontSize);
	t.setAttribute("font-family", fontFamily);
	t.setAttribute("text-align", settingsObj.align);
	t.setAttribute("line-increment", lineIncrement);
	cueArea.appendChild(t);
	reportMessage("textArea created: "+t);
	return t;
}

function printObj(o) {
  for (var p in o) {
	if (o.hasOwnProperty(p)) {
	  alert(""+p+": "+o[p]);
    }
  }
}

function addSpan(parent, cuePart) {
	reportMessage("adding tspan to "+parent+" with cuePart: "+ cuePart);
	var i;
	var span = document.createElement('tspan');
	//alert('adding span for '+parent+': '+cuePart+' token:'+cuePart.token);
	if (cuePart.token === undefined) {
		span.textContent = cuePart;
	} else {
		if (cuePart.token === 'i') {
			span.setAttribute("font-style", "italic");
		} else if (cuePart.token === 'u') {
			span.setAttribute("text-decoration", "underline");
		} else if (cuePart.token === 'b') {
			span.setAttribute("font-weight", "bold");
		}
		for (i in cuePart.children) {
			addSpan(span, cuePart.children[i]);
		}
	}
	parent.appendChild(span);
	reportMessage("tspan created "+span);
	return span;
}

function parseCueSettings(cueSettings){
	reportMessage("Parsing cue settings: "+cueSettings);
	var obj = {};
	var settingsArray = cueSettings.split(/\s+/g);
	//.filter(function(set) { return set && !!set.length; });

	// Convert the Array into an object 
	var compositeCueSettings = {};
	for (var settingsIndex in settingsArray) {
		var nvArray = settingsArray[settingsIndex].split(":");
		compositeCueSettings[nvArray[0]] = compositeCueSettings[nvArray[1]];
	}

	// Compute real values
	if (compositeCueSettings.line !== undefined) {
		if (compositeCueSettings.line.match(/\%/)) {
			obj.linePosition = parseFloat(compositeCueSettings.line.replace(/\%/ig,""));
			obj.linePosition *= height/100;
		} else {
			obj.linePosition = parseFloat(compositeCueSettings.line)*lineIncrement;
			if (obj.linePosition > 0) {
				obj.fromTop = true;
			} else {
				obj.fromTop = false;
				obj.linePosition = -obj.linePosition;
			}
		}
	} else {
		obj.linePosition = nbCues*lineIncrement;
	}
	if (compositeCueSettings.position !== undefined) {
		obj.xPosition = parseFloat(compositeCueSettings.position.replace(/\%/ig,""));
		obj.xPosition *= width/100;
	} else {
		obj.xPosition = 0;
	}
	if (compositeCueSettings.size !== undefined) {
		obj.size = parseFloat(compositeCueSettings.size.replace(/\%/ig,""));
		obj.size *= width/100;
	} else {
		obj.size = width;
	}
	if (compositeCueSettings.align !== undefined) {
		if (compositeCueSettings.align === "middle") {
			obj.align = "center";
		} else if (compositeCueSettings.align === "left") {
			obj.align = "start";
		} else if (compositeCueSettings.align === "right") {
			obj.align = "end";
		} else {
			obj.align = compositeCueSettings.align;
		}
	} else {
		obj.align = "center";
	}
	reportMessage("cue settings parsed: "+obj);
	return obj;
}

function removeCues() {
	cueArea.textContent = '';
	nbCues = 0;
}

function addCue(id, start, end, settings, payload) {
	reportMessage("adding cue: "+id+","+start+","+end+","+settings+","+payload);
	var lines = payload.split('\n');
	var settingObj = parseCueSettings(settings);
	var tA = createTextArea(settingObj);
	for (var lineIndex in lines) {
		var parsedCue = parseCue(lines[lineIndex]);
		for (var cuePartIndex in parsedCue) {
			addSpan(tA, parsedCue[cuePartIndex]);
		}
		tA.appendChild(document.createElement('tbreak'));
	}
	nbCues++;
	reportMessage("cue added: "+tA);
}

/* Code adapted from https://raw.github.com/cgiffard/Captionator/master/js/captionator.js */
var SRTChunkTimestampParser		= /(\d{2})?:?(\d{2}):(\d{2})[\.\,](\d+)/; 
function parseCue(cueText) {
	reportMessage("Parsing cue text: "+cueText);

	var cueStructure = new Array,
		cueSplit = [],
		splitIndex,
		currentToken,
		currentContext,
		stack = [],
		stackIndex = 0,
		chunkTimestamp,
		timeData,
		lastCueTime;
					
	cueSplit = cueText.split(/(<\/?[^>]+>)/ig);
				
	currentContext = cueStructure;
	for (splitIndex in cueSplit) {
		if (cueSplit.hasOwnProperty(splitIndex)) {
			currentToken = cueSplit[splitIndex];
			
			if (currentToken.substr(0,1) === "<") {
				if (currentToken.substr(1,1) === "/") {
					// Closing tag
					var TagName = currentToken.substr(2).split(/[\s>]+/g)[0];
					if (stack.length > 0) {
						// Scan backwards through the stack to determine whether we've got an open tag somewhere to close.
						var stackScanDepth = 0;
						for (stackIndex = stack.length-1; stackIndex >= 0; stackIndex --) {
							var parentContext = stack[stackIndex][stack[stackIndex].length-1];
							stackScanDepth = stackIndex;
							if (parentContext.token === TagName) { break; }
						}
					
						currentContext = stack[stackScanDepth];
						stack = stack.slice(0,stackScanDepth);
					} else {
						// Tag mismatch!
						alert("Tag mismatch when parsing WebVTT cue: "+cueText);
					}
				} else {
					// Opening Tag
					// Check whether the tag is valid according to the WebVTT specification
					// If not, don't allow it (unless the sanitiseCueHTML option is explicitly set to false)
				
					if ((	currentToken.substr(1).match(SRTChunkTimestampParser)	||
							currentToken.match(/^<v\s+[^>]+>/i)						||
							currentToken.match(/^<c[a-z0-9\-\_\.]+>/)				||
							currentToken.match(/^<(b|i|u|ruby|rt)>/))				) {
						
						var tmpObject = {
							"token":	currentToken.replace(/[<\/>]+/ig,"").split(/[\s\.]+/)[0],
							"rawToken":	currentToken,
							"children":	[]
						};
						
						if (tmpObject.token === "v") {
							tmpObject.voice = currentToken.match(/^<v\s*([^>]+)>/i)[1];
						} else if (tmpObject.token === "c") {
							tmpObject.classes = currentToken
													.replace(/[<\/>\s]+/ig,"")
													.split(/[\.]+/ig)
													.slice(1)
													.filter(hasRealTextContent);
						} else if (!!(chunkTimestamp = tmpObject.rawToken.match(SRTChunkTimestampParser))) {
							cueStructure.isTimeDependent = true;
							timeData = chunkTimestamp.slice(1);
							tmpObject.timeIn =	parseInt((timeData[0]||0) * 60 * 60,10) +	// Hours
												parseInt((timeData[1]||0) * 60,10) +		// Minutes
												parseInt((timeData[2]||0),10) +				// Seconds
												parseFloat("0." + (timeData[3]||0));		// MS
						}
					
						currentContext.push(tmpObject);
						stack.push(currentContext);
						currentContext = tmpObject.children;
					}
				}
			} else {
				// Text string
				currentToken = currentToken
								.replace(/</g,"&lt;")
								.replace(/>/g,"&gt;")
								.replace(/\&/g,"&amp;");
					
				currentToken = currentToken.replace(/\n+/g,"<br />");
			
				currentContext.push(currentToken);
			}
		}
	}
	reportMessage("cue text parsed: "+cueStructure);
	return cueStructure;
}
