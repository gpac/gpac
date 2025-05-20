import nodeGypBuild from "node-gyp-build";
import fs from "fs";
import url from "url";
import path from "path";

const scriptDir =
  typeof __dirname !== "undefined"
    ? __dirname
    : path.dirname(
        (() => {
          try {
            return url.fileURLToPath(import.meta.url);
          } catch {
            return __filename;
          }
        })()
      );
const packageDir = path.resolve(scriptDir, "..");

// Check the package version
const packageJSONPath = path.resolve(packageDir, "package.json");
const packageJSON = JSON.parse(fs.readFileSync(packageJSONPath, "utf8"));
const { version } = packageJSON;
const [major, minor, _] = version.split(".").map(Number);

// Check the ABI version
const gpac = nodeGypBuild(packageDir);
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
