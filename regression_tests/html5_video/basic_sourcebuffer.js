/*****************************************************************************************
 * Testing basic support for the MSE SourceBuffer JavaScript object
 *****************************************************************************************/

alert("Script loaded");

function enumerate(e, s)
{
	for(var propertyName in e) {
		alert((s==null?"":s)+" "+propertyName+": "+e[propertyName]);
		if (typeof(e[propertyName]) == "object") {
			enumerate(e[propertyName], " ");
		}
	}		
}

function onSourceOpen(event)
{
	alert("MediaSource opened");
	/* GPAC Hack: since the event is not targeted to the MediaSource, we need to get the MediaSource back */
	var ms = event.target.ms;
	enumerate(ms);
	
	alert("Checking if type video/mp4 is supported: "+MediaSource.isTypeSupported("video/mp4"));
	alert("Checking if type video/ogg is supported: "+MediaSource.isTypeSupported("video/ogg"));
	alert("Checking if type text/plain is supported: "+MediaSource.isTypeSupported("text/plain"));
	alert("Checking if type application/octet-stream is supported: "+MediaSource.isTypeSupported("application/octet-stream"));

	alert("Adding Source Buffer of type video/mp4 to the MediaSource");
	var sb = ms.addSourceBuffer("video/mp4");
	alert("SourceBuffer "+sb);
	enumerate(sb);
	enumerate(sb.buffered);
}

function init()
{	
	var v = document.getElementById("v");
	
	alert("Creating new MediaSource");
	var ms = new MediaSource();			

	/* GPAC Hack: the event should be dispatched to the MediaSource object */
	v.addEventListener("sourceopen", onSourceOpen);
				
	var url = URL.createObjectURL(ms);
	alert("Attaching Media Source "+url+" to Video");
	v.src = url;
	
	/* GPAC hack to retrieve the MediaSource from the video when the sourceopen event is dispatched */
	v.ms = ms;
	
}