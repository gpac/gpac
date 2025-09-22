The validator allows recording user interactions and taking snapshots when playing back interactive content; and replaying the content simulating the interactions, generating and comparing snapshots. To activate it you need to modify the GPAC configuration as follows: 

Add a section:
[Validator]
Mode=Play

Mode can be: Play, Record, or Disable

The validator will try to load a trace file, a single file or a playlist.

** Trace files are indicated with
Trace=SomeTrace

A trace file does not contain any association with the source file nor screenshots, only mouse and keyboard events are used. 
In trace mode the renderer is not reconfigured.

** Single files are indicated with:
XVS=/PATH/TOfile.xvs
XVS means XML Validation Sequence. An example of XVS is:

<TestValidationScript file="/PATH/TO/animate-elem-09-t.svg" >
<snapshot time="237" image="animate-elem-09-t-reference-000.png" />
<mousemove time="4749.000000" x="99" y="354" />
<mousemove time="4749.000000" x="103" y="343" />
<mousemove time="4749.000000" x="108" y="333" />
</TestValidationScript>


** Playlist of XVS are givin with:

XVL=/PATH/TO/svg1.1f2.xvl
XVL means XML Validation List. An example of XVL is:

<TestSuiteValidationScript content-base="/PATH/TO/svg" >
<Test scenario="animate-elem-02-t.xvs" content="animate-elem-02-t.svg" />
<Test scenario="animate-elem-03-t.xvs" content="animate-elem-03-t.svg" />
<Test scenario="animate-dom-01-f.xvs" content="animate-dom-01-f.svg" />
<Test scenario="animate-dom-02-f.xvs" content="animate-dom-02-f.svg" />
</TestSuiteValidationScript>

When recording and playing back, the GPAC player rendering is switched to 5 FPS without antialiasing, except for Trace mode.

When recording, you can trigger some actions as follows (except for trace mode):
CTRL+Insert: takes a snapshot and records it as a PNG. In replay mode, a PNG will be taken at the same scene time and the PNG will be compared.
CTRL+Fin: Quit
CTRL+F1: Takes snapshot at next frame change.
Page Down: Ends current XVS and switches to next in the XVL

Note: after recording, upon closing the player, the Validator mode is automatically switched to Play to avoid losing recorded interactions, except in trace mode.
