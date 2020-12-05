QuickJS in GPAC
- base version is quickjs-2020-11-08 rc2
- define CONFIG_VERSION in QuickJS.c
- patched for MSVC compil, based on https://github.com/horhof/quickjs/compare/master...samhocevar:task/msvc-support
- do not define CONFIG_STACK_CHECK (stack checking breaks multithread support)
- patched for JS_GetOpaque_Nocheck (get opaque data without class_id check, needed in some places in gpac due to original SpiderMonkey design)
- patched for exporting JS_AtomIsArrayIndex
