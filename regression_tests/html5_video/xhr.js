/*****************************************************************************************
 * Testing XHR objects 
 *****************************************************************************************/

/* TextArea element to report debug messages */
var tA = null;

function report(msg) {
	tA.appendChild(document.createElement('tbreak'));
	tA.appendChild(document.createTextNode(msg));
}

function getFile(url, responseType, mime, doneCallback, xhr2)
{
  var xhr;
  if (!xhr2) xhr = new XMLHttpRequest();
  else xhr = xhr2;  
  xhr.url = url;
  xhr.open("GET", xhr.url, false);
  xhr.responseType = responseType;
  if (mime !== "") {
	xhr.overrideMimeType(mime);
  }
  xhr.onreadystatechange = doneCallback;
  report("Downloading: " + xhr.url + ", expecting responseType \'" + responseType +"\'"+ ((mime !== "")?", with MIME: " + mime:""));
  xhr.send();
  return xhr;
}

function onDone2(e)
{ 
  if (this.readyState == this.DONE)
  {
	report("Download done: "+this.url);
	report("Response: "+this.response+", of size: "+this.response.byteLength);
	if (count == 0) {
	  count++;
	  getFile("http://perso.telecom-paristech.fr/~concolat/html5_tests/video1.ogv", "arraybuffer", "video/ogv", onDone2, this);
	}
  }
}

function onDone(e)
{ 
  if (this.readyState == this.DONE)
  {
	report("Download done: "+this.url);
	report("responseType: "+this.responseType);
	report("typeof(response): "+typeof(this.response));
	report("responseText: "+this.responseText);
	report("responseXML: "+this.responseXML);
	report("response: "+this.response);
	switch(this.responseType) {
		case "":
			break;
		case "text":
			break;
		case "arraybuffer":
			report("ArrayBuffer length:"+this.response.byteLength);
			break;
		case "json":
			break;
		case "document":
			break;
		case "blob":
			break;
		case "stream":
			break;
	}
	report("");
  }
}

function testAll(url)
{
  getFile(url, "",            "", onDone);
  getFile(url, "text",        "", onDone);
  getFile(url, "arraybuffer", "", onDone);
  getFile(url, "json",        "", onDone);
  getFile(url, "document",    "", onDone);
  getFile(url, "blob",        "", onDone);
  getFile(url, "stream",      "", onDone);

  getFile(url, "",            "text/plain", onDone);
  getFile(url, "text",        "text/plain", onDone);
  getFile(url, "arraybuffer", "text/plain", onDone);
  getFile(url, "json",        "text/plain", onDone);
  getFile(url, "document",    "text/plain", onDone);
  getFile(url, "blob",        "text/plain", onDone);
  getFile(url, "stream",      "text/plain", onDone);

  getFile(url, "",            "application/xml", onDone);
  getFile(url, "text",        "application/xml", onDone);
  getFile(url, "arraybuffer", "application/xml", onDone);
  getFile(url, "json",        "application/xml", onDone);
  getFile(url, "document",    "application/xml", onDone);
  getFile(url, "blob",        "application/xml", onDone);
  getFile(url, "stream",      "application/xml", onDone);

  getFile(url, "",            "application/octet-stream", onDone);
  getFile(url, "text",        "application/octet-stream", onDone);
  getFile(url, "arraybuffer", "application/octet-stream", onDone);
  getFile(url, "json",        "application/octet-stream", onDone);
  getFile(url, "document",    "application/octet-stream", onDone);
  getFile(url, "blob",        "application/octet-stream", onDone);
  getFile(url, "stream",      "application/octet-stream", onDone);
}

function testAllLocal(filename) {
	testAll(filename);
//	testAll("file://"+filename);
//	testAll("file://localhost/"+filename);
}

function init()
{ 
  tA = document.createElement('textArea');
  document.documentElement.appendChild(tA);

  report("Starting tests ...");
  testAll("http://perso.telecom-paristech.fr/~concolat/html5_tests/file.txt");
  testAll("http://perso.telecom-paristech.fr/~concolat/html5_tests/file.xml");
  testAll("http://perso.telecom-paristech.fr/~concolat/html5_tests/file.mp4");
  testAll("http://perso.telecom-paristech.fr/~concolat/html5_tests/file.json");
  testAllLocal("file.txt");
  testAllLocal("file.xml");
  testAllLocal("file.mp4");
  testAllLocal("file.json");
  report("done.");

}

alert("Script loaded"); 
init();

