import gpac from "./dist/index.mjs";
import assert from "assert";

process.stdout.write("[test] gpac loads...");
gpac.gf_sys_init(gpac.GF_MemTrackerNone, "gpac");
const fs = gpac.GF_FilterSession.new_defaults(0);
assert(fs !== null, "Failed to create default filter session");
gpac.gf_sys_close();
process.stdout.write(" OK\n");