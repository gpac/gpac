# QuickJS in GPAC
Base version is quickjs-2025-10-15 :
	https://github.com/bellard/quickjs/commit/eb2c89087def1829ed99630cb14b549d7a98408c
	
## Modifications to QuickJS
- define CONFIG_VERSION in QuickJS.c
- patched for MSVC compil, based on https://github.com/horhof/quickjs/compare/master...samhocevar:task/msvc-support
- do not define CONFIG_STACK_CHECK (stack checking breaks multithread support)
- patched for exporting JS_AtomIsArrayIndex and JS_IsArrayBuffer
- patched for switch classID on object (for webGL named textures in GPAC)

## Modifications to QuickJS-libc ('os' and 'std' modules)
- moved from pthread to GF_Thread in 'os' module
- Worker support for windows (patched js_os_poll)
- cleanup worker thread exit to avoid leaks
- patch js_std_loop to not run forever for main thread
- added MSVC support for most 'os' functions (most importantly exec, waitpid and kill)
- use gpac module loader
