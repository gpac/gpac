var fs = require('fs')
var http = require('http')
var url_parser = require('url');

var BUFFER_LENGTH = 2;
var paused = true;
var pausedAfterSend = false;

function sendBytes(file) {
	var length = BUFFER_LENGTH;
	var buffer = new Buffer(length);
	var offset = 0;
	var bytesRead;

	if (!paused) {
		bytesRead = fs.readSync(file.fd, buffer, offset, length, file.position);
		file.position += bytesRead;
		
		if (bytesRead == 0) {
			file.res.end();
			clearInterval(file.id);
			console.log("Ending request "+file.position+"/"+file.size);
		} else {
			console.log("Sending byte "+file.position+"/"+file.size);
			file.res.write(buffer);
			if (pausedAfterSend) paused = true;
		}
	}
}

var app = function(req,res)
{
	console.log("Received request for file "+req.url);
	var parsed_url = url_parser.parse(req.url, true);
	var filename = '.'+parsed_url.pathname;
	if (fs.existsSync(filename)) {	
		var head = {};
		var file = {};
		file.position = 0;
		file.fd = fs.openSync(filename, 'r');
		file.res = res;
		file.size = fs.statSync(filename).size;
		if (filename.slice(-3) === "mp4") {
			head['Content-Type'] = 'video/mp4';
		}
		head['Content-Length'] = file.size;
		res.writeHead(200, head);
		file.id = setInterval(sendBytes, 300, file);
	}
}

http.createServer(app).listen(8020,"127.0.0.1")

process.stdin.on('data', function(chunk) {
	if (chunk.toString() == "b\r\n") {
		BUFFER_LENGTH = 2;
		console.log("New buffer size "+BUFFER_LENGTH);
	} else if (chunk.toString() == "k\r\n") {
		BUFFER_LENGTH = 1000;	
		console.log("New buffer size "+BUFFER_LENGTH);
	} else if (chunk.toString() == "M\r\n") {
		BUFFER_LENGTH = 1000000;	
		console.log("New buffer size "+BUFFER_LENGTH);
	} else if (chunk.toString() == "p\r\n") {
		paused = !paused;	
	} else if (chunk.toString() == "P\r\n") {
		pausedAfterSend = !pausedAfterSend;	
	} else if (chunk.toString() == "q\r\n") {
		process.exit();
	}
});