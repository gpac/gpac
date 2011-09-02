The validator allows recording user interactions and taking snapshots when playing back interactive content; and replaying the content simulating the interactions, generating and comparing snapshots. To activate it you need to modify the GPAC configuration as follows: 

Add a section:
[Validator]
Mode=Play

Mode can be: Play, Record, or Disable

The validator will try to load a single file to be tested or a playlist.
Single files are indicated with:
XVS=C:\Users\Cyril Concolato\code\tsi_svn\trunk\gpac_public\regression_tests\svg\file.xvs
XVS means XML Validation Sequence. An example of XVS is:

<TestValidationScript file="C:\Users\Cyril Concolato\standards\SVG_WG\SVG\profiles\1.1F2\test\svg\animate-elem-09-t.svg" >
<snapshot time="237" image="animate-elem-09-t-reference-000.png" />
<mousemove time="4749.000000" x="99" y="354" />
<mousemove time="4749.000000" x="103" y="343" />
<mousemove time="4749.000000" x="108" y="333" />
</TestValidationScript>


Playlist of XVS are givin with:
XVL=C:\Users\Cyril Concolato\code\tsi_svn\trunk\gpac_public\regression_tests\svg\svg1.1f2.xvl
XVL means XML Validation List. An example of XVL is:

<TestSuiteValidationScript content-base="C:\Users\Cyril Concolato\standards\SVG_WG\SVG\profiles\1.1F2\test\svg" >
<Test scenario="animate-elem-02-t.xvs" content="animate-elem-02-t.svg" />
<Test scenario="animate-elem-03-t.xvs" content="animate-elem-03-t.svg" />
<Test scenario="animate-dom-01-f.xvs" content="animate-dom-01-f.svg" />
<Test scenario="animate-dom-02-f.xvs" content="animate-dom-02-f.svg" />
</TestSuiteValidationScript>

When recording and playing back, the GPAC player rendering is switched to 5 FPS without antialiasing.

When recording, you can trigger some actions as follows:
CTRL+Insert: takes a snapshot and records it as a PNG. In replay mode, a PNG will be taken at the same scene time and the PNG will be compared.
CTRL+Fin: Quit
CTRL+F1: Takes snapshot at next frame change.
Page Down: Ends current XVS and switches to next in the XVL

Note: after recording, upon closing the player, the Validator mode is automatically switched to Play to avoid losing recorded interactions.