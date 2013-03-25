/*****************************************************************************************
 * Testing reuse of the same XHR object for multiple downloads with ArrayBuffer
 *****************************************************************************************/

alert("Script loaded");

var count = 0;

function getFile(xhr, url)
{
  xhr.url = url;
  xhr.open("GET", xhr.url);
  xhr.responseType = "arraybuffer";
  xhr.onreadystatechange = onDone;
  alert("Downloading: "+xhr.url +" waiting for ArrayBuffer response");
  xhr.send();
}

function onDone(e)
{ 
  if (this.readyState == this.DONE)
  {
	alert("Download done: "+this.url);
	alert("Response: "+this.response+", of size: "+this.response.byteLength);
	if (count == 0) {
	  count++;
	  getFile(this, "http://perso.telecom-paristech.fr/~concolat/html5_tests/video1.ogv");
	}
  }
}

function init()
{
  var xhr = new XMLHttpRequest();
  getFile(xhr, "http://perso.telecom-paristech.fr/~concolat/html5_tests/video1.mp4");
}
