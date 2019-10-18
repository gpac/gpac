QuickJS in GPAC
- base version is quickjs-2019-09-18
- define CONFIG_VERSION in QuickJS.c
- patched for MSVC compil, based on https://github.com/horhof/quickjs/compare/master...samhocevar:task/msvc-support
- patched for no stack checking (breaks multithread support)
- patched for JS_GetOpaque_Nocheck (get opaque data without class_id check, needed in some places in gpac due to original SpiderMonkey design)
- patched for JS_EvalWithTarget (for SVG handler+observer, to cleanup)
- patched for exporting JS_AtomIsArrayIndex
- patched JS_CGETSET_DEF and JS_CGETSET_MAGIC_DEF for MSVC compil (designated initialzer with struct in union need { } to work properly)
