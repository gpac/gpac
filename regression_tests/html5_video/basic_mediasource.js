/*****************************************************************************************
 * Testing basic support for the MediaSource JavaScript object
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

function init()
{	
	alert("Creating new MediaSource");
	var ms = new MediaSource();			
	alert("MediaSource object: "+ms);
	enumerate(ms);
}

document.documentElement.addEventListener("load", init);