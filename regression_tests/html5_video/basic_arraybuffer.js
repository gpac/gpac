/*****************************************************************************************
 * Testing basic support for the ArrayBuffer JavaScript object
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
	alert("Creating ArrayBuffer(100)");
	var ab = new ArrayBuffer(100);
	alert("Typeof(ArrayBuffer): "+typeof(ab));
	alert("ArrayBuffer: "+ab);
	enumerate(ab);
}