QuickJS in GPAC
- base version is quickjs-2021-03-27 :
	https://github.com/bellard/quickjs/commit/b5e62895c619d4ffc75c9d822c8d85f1ece77e5b
	
- define CONFIG_VERSION in QuickJS.c
- patched for MSVC compil, based on https://github.com/horhof/quickjs/compare/master...samhocevar:task/msvc-support
- do not define CONFIG_STACK_CHECK (stack checking breaks multithread support)
- patched for JS_GetOpaque_Nocheck (get opaque data without class_id check, needed in some places in gpac due to original SpiderMonkey design)
- patched for exporting JS_AtomIsArrayIndex and JS_IsArrayBuffer

