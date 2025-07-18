/**
 * Constants
 */

const SIZE_I32 = Uint32Array.BYTES_PER_ELEMENT;
const DEFAULT_ARGS = [];

Module["SIZE_I32"] = SIZE_I32;
Module["DEFAULT_ARGS"] = DEFAULT_ARGS;
Module["logger"] = () => {};
Module["noInitialRun"] = true;
Module["noExitRuntime"] = true;

/**
 * Functions
 */

function stringToPtr(str) {
  const len = Module["lengthBytesUTF8"](str) + 1;
  const ptr = Module["_malloc"](len);
  Module["stringToUTF8"](str, ptr, len);

  return ptr;
}

function stringsToPtr(strs) {
  const len = strs.length;
  const ptr = Module["_malloc"](len * SIZE_I32);
  for (let i = 0; i < len; i++) {
    Module["setValue"](ptr + SIZE_I32 * i, stringToPtr(strs[i]), "i32");
  }

  return ptr;
}

function print(message) {
  Module["logger"]({ type: "stdout", message });
}

function printErr(message) {
  Module["logger"]({ type: "stderr", message });
}

function exec(tool, ..._args) {
  const args = [tool, ...Module["DEFAULT_ARGS"], ..._args];
  try {
    Module["_main"](args.length, stringsToPtr(args));
  } catch (e) {
    if (e !== "unwind") {
      throw e;
    }
  }
}

function _locateFile(path, prefix) {
  const hashedWasmBinaryFile = Module["hashedWasmBinaryFile"];
  if (hashedWasmBinaryFile && path.endsWith(".wasm")) {
    return hashedWasmBinaryFile;
  }
  return prefix + path;
}

function setLogger(logger) {
  Module["logger"] = logger;
}

Module["stringToPtr"] = stringToPtr;
Module["stringsToPtr"] = stringsToPtr;
Module["locateFile"] = _locateFile;
Module["exec"] = exec;

if (libgpac.use_logger) {
  Module["print"] = print;
  Module["printErr"] = printErr;
  Module["setLogger"] = setLogger;
}
