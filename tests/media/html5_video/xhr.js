/*****************************************************************************************
 * Testing XHR objects 
 *****************************************************************************************/

/* TextArea element to report debug messages */
var tA = null;

function report(msg) {
	tA.appendChild(document.createElement('tbreak'));
	tA.appendChild(document.createTextNode(msg));
}

function getResource(url, async, responseType, mime, doneCallback, xhr_reused)
{
  var xhr;
  if (!xhr_reused) xhr = new XMLHttpRequest();
  else xhr = xhr_reused;  
  xhr.url = url;
  xhr.open("GET", xhr.url, async);
  xhr.responseType = responseType;
  if (mime !== "") {
	xhr.overrideMimeType(mime);
  }
  if (async) xhr.onreadystatechange = doneCallback;
  report((async ? "Asynchronous" : "Synchronous" )+ " download of " + xhr.url + "," +
         " expecting responseType \'" + responseType +"\'"+ ((mime !== "")?", with forced MIME type: '" + mime +"'":""));
  xhr.send();
  if (!async) doneCallback.call(xhr);
  return xhr;
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

function getResourceSimple(url, async, responseType, mime)
{
	getResource(url, async, (responseType == undefined ? "" : responseType), (mime == undefined ? "" : mime), onDone);
}

function getResourceAllWays(url, async)
{
  getResource(url, async, "",            "", onDone);
  getResource(url, async, "text",        "", onDone);
  getResource(url, async, "arraybuffer", "", onDone);
  getResource(url, async, "json",        "", onDone);
  getResource(url, async, "document",    "", onDone);
  getResource(url, async, "blob",        "", onDone);
  getResource(url, async, "stream",      "", onDone);

  getResource(url, async, "",            "text/plain", onDone);
  getResource(url, async, "text",        "text/plain", onDone);
  getResource(url, async, "arraybuffer", "text/plain", onDone);
  getResource(url, async, "json",        "text/plain", onDone);
  getResource(url, async, "document",    "text/plain", onDone);
  getResource(url, async, "blob",        "text/plain", onDone);
  getResource(url, async, "stream",      "text/plain", onDone);

  getResource(url, async, "",            "application/xml", onDone);
  getResource(url, async, "text",        "application/xml", onDone);
  getResource(url, async, "arraybuffer", "application/xml", onDone);
  getResource(url, async, "json",        "application/xml", onDone);
  getResource(url, async, "document",    "application/xml", onDone);
  getResource(url, async, "blob",        "application/xml", onDone);
  getResource(url, async, "stream",      "application/xml", onDone);

  getResource(url, async, "",            "application/octet-stream", onDone);
  getResource(url, async, "text",        "application/octet-stream", onDone);
  getResource(url, async, "arraybuffer", "application/octet-stream", onDone);
  getResource(url, async, "json",        "application/octet-stream", onDone);
  getResource(url, async, "document",    "application/octet-stream", onDone);
  getResource(url, async, "blob",        "application/octet-stream", onDone);
  getResource(url, async, "stream",      "application/octet-stream", onDone);
}

function testAllLocal(filename, asyncDownload, getMethod, responseType, mime) {
	getMethod(filename, asyncDownload, responseType, mime);
//	getMethod("file://"+filename, asyncDownload, responseType, mime);
//	getMethod("file://localhost/"+filename, asyncDownload, responseType, mime);
}

function test1() 
{
  var baseURL = "";"http://perso.telecom-paristech.fr/~concolat/html5_tests/";
  var asyncDownload = false;
  var testFunc = getResourceSimple; //getResourceAllWays;
  report("Starting tests 1...");
  testFunc(baseURL+"file.txt", asyncDownload);
  testFunc(baseURL+"file.xml", asyncDownload, "document");
  testFunc(baseURL+"file.mp4", asyncDownload, "arraybuffer");
  testFunc(baseURL+"file.json", asyncDownload);
/*  testAllLocal("file.txt", asyncDownload, testFunc);
  testAllLocal("file.xml", asyncDownload, testFunc, "document");
  testAllLocal("file.mp4", asyncDownload, testFunc, "arraybuffer");
  testAllLocal("file.json", asyncDownload, testFunc);*/
  report("end of tests 1.");
}

function test2() 
{
  var res;

  report("Starting tests 2...");
  var xhr = getResource("file.mp4", false, "arraybuffer", "application/octet-stream", onDone);
  var res = xhr.response;
  alert("Resource: "+res);
  getResource("file.json", false, "arraybuffer", "application/octet-stream", onDone, xhr);
  report("end of tests 2");
}

function init()
{ 
  tA = document.createElement('textArea');
  document.documentElement.appendChild(tA);
}

alert("Script loaded"); 
init();
test1();

