if (!Uint8Array) {
var Uint8Array = function () {};
Uint8Array.prototype = new ArrayBuffer();
}

(function () {
if (typeof(gpac) == "undefined") return;
console.log ("Using GPAC HTML Portability script");
var DEBUG = false;
document.createElementOrig = document.createElement;
document.createElement = function (str) { 
  var e;
  if (str == "div" ) { e = document.createElementOrig("g"); }
  else if (str == "table" ) { e = document.createElementOrig("textArea"); e.setAttribute("width", "500");  }
  else if (str == "tr" ) { e = document.createElementOrig("tspan"); e.break = true;}
  else if (str == "td" ) { e = document.createElementOrig("tspan"); }
  else if (str == "h1" || str == "h2" || str == "h3" || str == "h4" ) { e = document.createElementOrig("text"); }
  else if ( str == "a" ) { e = document.createElementOrig("tspan"); }
  else if ( str == "span" ) { e = document.createElementOrig("tspan"); }
  else if (str == "textarea") { e = document.createElementOrig("textArea"); }		
  else { e = document.createElementOrig(str); }
  e.formerElementName = str;
  if (DEBUG) 
    console.log("created SVG element '"+e.tagName+"' for html tag '"+str+"'");
  return e;
}		

document.body = document.documentElement;

Node.__proto__.focus = function () { 
  if (DEBUG)
    console.log("focus called on node '"+this.tagName+"'"); 
};	  

SVGElement.__proto__.__defineSetter__("id", function (s) { 
                                               if (DEBUG) 
											     console.log("setting id on "+this.tagName+" with '"+s+"'"); 
											   this.setAttribute("id", s); 
											 } ); 
SVGElement.__proto__.__defineGetter__("id", function () { 
                                               if (DEBUG) 
											     console.log("getting id on "+this.tagName); 
											   return this.getAttribute("id"); 
											 } );
SVGElement.__proto__.__defineSetter__("innerHTML", function (s) { 
												var t = s.replace(/&nbsp;/g, ' ');
												t = t.replace(/&amp;/g, '&');
                                                if (DEBUG) 
											      console.log("setting innerHTML on "+this.tagName+" "+this.id+" with '"+t+"'"); 
											    this.textContent = t;
											 } ); 
SVGElement.__proto__.__defineGetter__("innerHTML", function () { 
                                               if (DEBUG) 
											     console.log("getting innerHTML on "+this.tagName); 
											   return this.textContent; 
											 } );
SVGElement.__proto__.__defineSetter__("classList", function (s) { 
                                               if (DEBUG) 
											     console.log("setting classList on "+this.tagName+" with '"+s+"'"); 
											   this.setAttribute("classList", s); 
											 } ); 
SVGElement.__proto__.__defineGetter__("classList", function () { 
                                               if (DEBUG) 
											     console.log("getting classList on "+this.tagName); 
											   return this.getAttribute("classList"); 
											 } );
SVGElement.__proto__.__defineSetter__("onclick", function (callback) { 
                                             if (DEBUG) 
											   console.log("setting onclick on "+this.tagName+" with callback "+callback); 
											 var f = function (event) { 
											   if (DEBUG) 
												 console.log("Element "+(event.target.tagName)+" clicked, calling "+callback); 
											   callback(event); 
										     };
											 this.addEventListener("click", f, false); 
										    });	  
											
Node.__proto__.__defineSetter__("title", function (value) { 
                                             if (DEBUG) 
											   console.log("setting title on "+this.tagName+" with '"+value+"'"); 
											 this.setAttribute("title", value);
										    });	  											
Node.__proto__.__defineSetter__("href", function (value) { 
                                             if (DEBUG) 
											   console.log("setting href on "+this.tagName+" with '"+value+"'"); 
											 this.setAttributeNS("http://www.w3.org/1999/xlink", "href", value);
										    });	  																					

document.getElementByIdOrig = document.getElementById;
document.getElementById = function (id) { 
  var e = document.getElementByIdOrig(id);
  if (DEBUG) 
    console.log("getting element by id '"+id+"' found '"+e+"'"+(e ? " of type '"+e.tagName+"'" : ""));
  return e;
}
}) ();