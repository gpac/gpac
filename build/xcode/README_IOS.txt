How to build GPAC for iOS

Tested revision: 2264
Tested XCode (SDK): 3.2.4 (4.1)

You have to have GPAC extra_libs in your extra_lib/lib/gcc directory
Be careful when building SDL in Release, I never made it work

1) Open the XCode project.
2) Build libgpac_dynamic via XCode (should end on link error)
3) Execute "script_libgpac.sh Debug" (or Release)
4) Build libgpac_dynamic via XCode (should succeed)
5) Build all modules on one click via XCode (should end on link error)
6) Execute "script_libgpac.sh Debug" (or Release)
7) Build all modules via XCode (should succeed)
8) Build osmo4ios via XCode
