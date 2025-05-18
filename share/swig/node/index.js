const gpac = require("./build/Release/gpac");
const package = require("./package.json");
const { version } = package;

const [major, minor, _] = version.split(".").map(Number);
const gpac_major = gpac.gf_gpac_abi_major();
const gpac_minor = gpac.gf_gpac_abi_minor();

if (major !== gpac_major) {
  throw new Error(
    `GPAC ABI major version mismatch: expected ${major}, got ${gpac_major}`
  );
}

if (minor > gpac_minor) {
  console.warn(
    `Warning: GPAC ABI minor version mismatch: expected ${minor}, got ${gpac_minor}. Some features may yield symbol resolution errors.`
  );
}

module.exports = gpac;
