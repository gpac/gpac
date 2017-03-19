var window = (function (window) {
if (!!window.setTimeout) return;
var DEBUG = false;
var svgNS = 'http://www.w3.org/2000/svg';
var xlinkNS = 'http://www.w3.org/1999/xlink';
var intervalIds=[];
var timeoutIds=[];

window.console = {};
console.log = function (str) { alert(str); };

function createAnimStruct(timeout, isIndefinite) {
  var defs = document.createElementNS(svgNS, 'defs');
  document.documentElement.appendChild(defs);
  var g = document.createElementNS(svgNS, 'g');
  defs.appendChild(g);
  g.setAttributeNS(svgNS, 'display', 'none');
  var anim = document.createElementNS(svgNS, 'animate');  
  anim.setAttributeNS(svgNS, 'attributeName', 'display');
  anim.setAttributeNS(svgNS, 'begin', 'indefinite');
  anim.setAttributeNS(svgNS, 'end', 'indefinite');
  anim.setAttributeNS(svgNS, 'dur', (+timeout)/1000);
  if (isIndefinite) {
    anim.setAttributeNS(svgNS, 'repeatCount', 'indefinite');
  }
  g.appendChild(anim);
  anim.setAttributeNS(svgNS, 'to', 'inline');
  return { root: defs, anim: anim};
}

var setTimer = function (useRepeatEvent, timers, callback, timeout) {
  var args = Array.prototype.slice(arguments, 4);
  var obj = createAnimStruct(timeout, useRepeatEvent);
  obj.id = timers.length;
  if (DEBUG) 
    console.log("Creating "+(useRepeatEvent ? "Interval ":"")+"Timer "+obj.id+" with arguments: "+Array.prototype.slice(arguments,0));
  obj.endEventCallback = function (event) { 
						  if (DEBUG) 
							window.console.log((useRepeatEvent ? "Interval ":"")+"Timeout "+obj.id+" reached");
						  if (!useRepeatEvent) {
						    document.documentElement.removeChild(obj.root);
						  }	
						  var newargs = [].concat(args);									
						  callback.apply(this, newargs);
						};
  obj.anim.addEventListener((useRepeatEvent ? 'repeatEvent' : 'endEvent'), obj.endEventCallback, false);
  obj.anim.beginElement();
  timers.push(obj);
  return obj.id;

}

window.setTimeout = function (callback, timeout) {
  return setTimer(false, timeoutIds, callback, timeout);
  // var args = Array.prototype.slice(arguments, 2);
  // var obj = createAnimStruct(timeout, false);
  // obj.id = timeoutIds.length;
  // if (DEBUG) 
    // console.log("Creating Timer "+obj.id+" with arguments: "+Array.prototype.slice(arguments,0));
  // obj.endEventCallback = function (event) { 
						  // if (DEBUG) 
							// window.console.log("Timeout "+obj.id+" reached");
						  // document.documentElement.removeChild(obj.root);
						  // var newargs = [].concat(args);									
						  // callback.apply(this, newargs);
						// };
  // obj.anim.addEventListener('endEvent', obj.endEventCallback, false);
  // obj.anim.beginElement();
  // timeoutIds.push(obj);
  // return obj.id;
}

window.setInterval = function (callback, timeout) {
  return setTimer(true, intervalIds, callback, timeout);
  // var args = Array.prototype.slice(arguments, 2);
  // var obj = createAnimStruct(timeout, true);
  // obj.id = intervalIds.length;
  // if (DEBUG) 
    // window.console.log("Creating Interval Timer "+obj.id+" with arguments: "+Array.prototype.slice(arguments,0));
  // obj.intervalCallback = function (event) { 
							 // if (DEBUG) 
							   // window.console.log("Interval "+obj.id+" reached");
							 // var newargs = [].concat(args);									
							 // callback.apply(this, newargs); 
						   // };
  // obj.anim.addEventListener('repeatEvent', obj.intervalCallback, false);
  // obj.anim.beginElement();
  // intervalIds.push(obj);
  // return obj.id;
}

var clearTimer = function(useRepeatEvent, timers, id) {
  if (DEBUG) 
    window.console.log("Clearing "+(useRepeatEvent ? "Interval ":"")+"Timer "+id+" with id: "+id);
  var obj = timers[id];
  if (obj) {
    if (obj.id != id) {
      window.console.log("Mismatch in ids while clearing timer: "+id+" vs. "+obj.id);
	}
    obj.anim.removeEventListener((useRepeatEvent ? 'repeatEvent' : 'endEvent'), obj.endEventCallback);
    document.documentElement.removeChild(obj.root);
	timers[id] = null;
  }
}

window.clearTimeout = function (id) {
  return clearTimer(false, timeoutIds, id);
  // if (DEBUG) 
    // window.console.log("Clearing Timer "+id+" with arguments: "+Array.prototype.slice(arguments,0));
  // var timerObj = timeoutIds[id];
  // if (timerObj) {
    // if (timerObj.id != id) {
      // window.console.log("Mismatch in ids while clearing Timer: "+id+" vs. "+timerObj.id);
	// }
    // timerObj.anim.removeEventListener('endEvent', timerObj.endEventCallback);
    // document.documentElement.removeChild(timerObj.root);
	// timeoutIds[id] = null;
  // }
}

window.clearInterval = function (id) {
  return clearTimer(true, intervalIds, id);
  // if (DEBUG) 
    // window.console.log("Clearing Interval Timer with arguments: "+Array.prototype.slice(arguments,0));
  // var intervalObj = intervalIds[id];
  // if (intervalObj) {
    // if (timerObj.id != id) {
      // window.console.log("Mismatch in ids while clearing Interval Timer: "+id+" vs. "+intervalObj.id);
	// }
    // intervalObj.anim.removeEventListener('repeatEvent', intervalObj.intervalCallback);
    // document.documentElement.removeChild(intervalObj.root);
  // }
}

window.printObject = function (obj) {
  window.console.log("Enumerating object: "+obj);
  for (var key in obj) {
    window.console.log("Property "+key+": "+obj[key]);
  }
  window.console.log("done.");
}

return window;
})(window ? window : this);
