import nodeGypBuild from "node-gyp-build";
import fs from "fs";
import { fileURLToPath } from "url";
import { dirname, resolve } from "path";

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// Check the package version
const packageJSONPath = resolve(__dirname, "package.json");
const packageJSON = JSON.parse(fs.readFileSync(packageJSONPath, "utf8"));
const { version } = packageJSON;
const [major, minor, _] = version.split(".").map(Number);

// Check the ABI version
const gpac = nodeGypBuild(__dirname);
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

export default gpac;
