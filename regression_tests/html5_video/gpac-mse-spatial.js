function displaySpatialSets() {
	var v = document.getElementById('v_thumb');
	var vx = parseFloat(v.getAttribute("x"));
	var vy = parseFloat(v.getAttribute("y"));
	var vw = parseFloat(v.getAttribute("width"));
	var vh = parseFloat(v.getAttribute("height"));
	var g = document.getElementById('g_thumb');
	var r;
	var as;
	for (var spatialIndex in spatialInfo.sets) {
		as = spatialInfo.sets[spatialIndex];
		r = document.createElement('rect');
		g.appendChild(r);
		r.setAttribute("fill", "none");
		r.setAttribute("stroke", "red");
		r.setAttribute("pointer-events", "all");
		r.setAttribute("onclick", "changeVideo("+spatialIndex+")");
		r.setAttribute("stroke-width", 1);
		r.setAttribute("x", vx+vw*(as.x/spatialInfo.global.w));
		r.setAttribute("y", vy+vh*(as.y/spatialInfo.global.h));
		r.setAttribute("width", vw*(as.w/spatialInfo.global.w));
		r.setAttribute("height", vh*(as.h/spatialInfo.global.h));
	}		
}

function changeVideo(index) {
	alert("Request to change main video to spatial region #"+index);
	p1.changeSegmentList(spatialInfo.sets[index].representations);
}

// var spatialInfo = {
	// global: { x: 0, y: 0, w: 800, h: 600 },
	// sets: [
		// { x:   0, y:   0, w: 400, h: 300, representations: redBullVideoHTTPSegments },
		// { x: 400, y:   0, w: 400, h: 300, representations: redBullVideoHTTPSegments },
		// { x:   0, y: 300, w: 400, h: 300, representations: redBullVideoHTTPSegments },
		// { x: 400, y: 300, w: 400, h: 150, representations: redBullVideoHTTPSegments },
		// { x: 400, y: 450, w: 400, h: 150, representations: redBullVideoHTTPSegments },		
	// ]
// }
var spatialInfo = myanmarSpatialRepresentations;

var p1 = new Player('v_main', 'info_main', 'timer_anim', myanmarTile1VideoSegmentsLocal);
var p2 = new Player('v_thumb', 'info_thumb', 'timer_anim', myanmarFullVideoSegmentsLocal);	

function wheelHandler(evt) {
	alert("Wheel event: "+ evt.wheelDelta);
	(evt.wheelDelta > 0 ? p1.switchUp() : p1.switchDown());
}
document.documentElement.addEventListener("wheel", wheelHandler);
p1.play();
p2.play();

displaySpatialSets();

