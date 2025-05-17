const gpac = require("./build/Release/gpac");

gpac.gf_sys_init(gpac.GF_MemTrackerNone, "");

const fs = gpac.GF_FilterSession.new_defaults(0);

fs.load_filter("avgen:v:dur=2");
fs.load_filter("vout");

try {
  fs.run();
} catch (e) {
  console.log("Error stopping filter session:", e.message);
}

fs.stop();
delete fs;

console.log("Filter session stopped and deleted");
