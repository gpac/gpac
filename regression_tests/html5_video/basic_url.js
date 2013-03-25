/*****************************************************************************************
 * Testing basic support for the HTML5 URL object
 *****************************************************************************************/

alert("Script loaded");

function enumerate(e)
{
	for(var propertyName in e) {
		alert(" "+propertyName+": "+e[propertyName]);
	}		
}

function init()
{			
	alert("Creating URL from nothing");
	alert("URL: "+URL.createObjectURL());
	alert("Creating URL from null");
	alert("URL: "+URL.createObjectURL(null));
	alert("Creating URL from dummy value");
	alert("URL: "+URL.createObjectURL(1));
	alert("Creating URL from Media Source");
	var ms = new MediaSource();			
	var url = URL.createObjectURL(ms);
	alert("URL: "+ url);
}