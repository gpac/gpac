
export const help = `AVMix is an audio video mixer controlled by an updatable JSON playlist format. The filter can be used to:
- schedule video sequence(s) over time
- mix videos together
- layout of multiple videos
- overlay images, text and graphics over source videos

All input streams are decoded prior to entering the mixer.
- audio streams are mixed in software
- video streams are composed according to the \`gpu\` option
- other stream types are not yet supported

OpenGL hardware acceleration can be used, but the supported feature set is currently not the same with or without GPU.

In software mode, the mixer will detect whether any of the currently active video sources can be used as a base canvas for the output to save processing time.
The default behavior is to do this detection only at the first generated frame, use \`dynpfmt\` to modify this.

The filter can be extended through JavaScript modules. Currently only scenes and transition effects use this feature.


# Live vs offline

When operating offline, the mixer will wait for video frames to be ready for 10 times \`lwait\`. After this timeout, the filter will abort if no input is available.
This implies that there shall always be a media to compose, i.e. no "holes" in the timeline.
Note: The playlist is still refreshed in offline mode.


When operating live, the mixer will initially wait for video frames to be ready for \`lwait\` seconds. After this initial timeout, the output frames will indicate:
- 'No signal' if no input is available (no source frames) or no scene is defined
- 'Signal lost' if no new input data has been received for \`lwait\` on a source
`;

export const help_playlist = `
# Playlist Format
## Overview
The playlist describes:
- Media sequences: each sequence is a set of sources to be played continuously
- Transitions: sources in a sequence can be combined using transitions
- Scenes: a scene describes one graphical object to put on screen and if and how input video are mapped on objects
- Groups: a group is a hierarchy of scenes and groups with positioning properties, and can also be used to create offscreen images reused by other elements.
- Timers: a timer can be used to animate scene parameters in various fashions

The playlist content shall be either a single JSON object or an array of JSON objects, hereafter called root objects.
Root objects types can be indicated through a \`type\` property:
- seq: a \`sequence\` object
- url: a \`source\` object (if used as root, a default \`sequence\` object will be created)
- scene: a \`scene\` object
- group: a \`group\` object
- timer: a \`timer\` object
- script: a \`script\` object
- config: a \`config\` object

The \`type\` property of root objects is usually not needed as the parser guesses the object types from its properties.

A root object with a property \`skip\` set to anything but \`0\` or \`false\` is ignored.
Within a \`group\` hierarchy, any \`scene\` or \`group\` object with a property \`skip\` set to anything but \`0\` or \`false\` is ignored.

Any unrecognized property not starting with \`_\` will be reported as warning.

## Colors
Colors are handled as strings, formatted as:
- the DOM color name (see \`gpac -h colors\`)
- HTML codes \`$RRGGBB\` or \`#RRGGBB\`
- RGB hex vales \`0xRRGGBB\`
- RGBA hex values \`0xAARRGGBB\`
- the color \`none\` is \`0x00000000\`, its signification depends on the object using it.

If JS code needs to manipulate colors, use sys.color_lerp and sys.color_component functions.

## JS Hooks

Some object types allow for custom JS code to be executed. 
The script code can either be the value of the property, or located in a file indicated in the property. 
The code is turned into a function (i.e. \`new Function(args, js_code)\`) upon initial playlist parsing or reload, hereafter called \`JSFun\`.
The \`JSFun\` arguments and return value are dependent on the parent object type.
The parent object is exposed as \`this\` in \`JSFun\` and can be used to store context information for the JS code.

The code can use the global functions and modules defined, especially:
- sys: GPAC system module
- evg: GPAC EVG module
- os: QuickJS OS module 
- video_playing: video playing state
- audio_playing: audio playing state
- video_time: output video time
- video_timescale: output video timescale
- video_width: output video width
- video_height: output video height
- audio_time: output audio time
- audio_timescale: output audio timescale
- samplerate: output audio samplerate
- channels: output audio channels
- current_utc_clock: current UTC clock in ms
- get_media_time: gets media time of output (no argument) or of source with id matching the first argument. Return
  - -4: not found
  - -3: not playing
  - -2: in prefetch
  - -1: timing not yet known
  - value: media time in seconds (float)
- resolve_url: resolves URL given in first argument against media playlist URL and returns the resolved url (string)
- get_scene(id): gets scene with given ID
- get_group(id): gets group with given ID
- get_script(id): gets script with given ID

Scene and group options must be accessed through getters and setters:
- scene.get(prop_name): gets the scene option
- scene.set(prop_name, value): sets the scene option
- group.get(prop_name): gets the group option
- group.set(prop_name, value): sets the group option

Results are undefined if JS code modifies the scene/group objects in any other way.

Warning: there is no protection of global variables and state, write your script carefully!

## Sequences
### Properties for \`sequence\` objects:
 - id (null): sequence identifier
 - loop (0): number of loops for the sequence (0 means no loop, -1 will loop forever)
 - start (0): sequence start time:
   - positive number: offset in seconds from current clock
   - negative number: sequence is not active
   - otherwise: date or \`now\`
 - stop (0): sequence stop time:
   - positive number greater than \`start\`: offset in seconds from current clock
   - negative number or less than \`start\`: sequence will stop only when over
   - otherwise: date or \`now\`
 - transition (null): a \`transition\` object to apply between sources of the sequence
 - seq ([]): array of one or more \`source\` objects

### Notes
Media source timing does not depend on the media being used by a scene or not, it is only governed by the \`sequence\` parameters.
This means that a \`sequence\` not used by any active scene will not be rendered (video nor audio).

## Sources
### Properties for \`source\` objects
- id (null): source identifier, used when reloading the playlist
- src ([]): list of \`sourceURL\` describing the URLs to play. Multiple sources will be played in parallel
- start (0.0): media start time in source
- stop (0.0): media stop time in source, ignored if less than or equal to \`start\`
- mix (true): if true, apply sequence transition or mix effect ratio as audio volume. Otherwise volume is not modified by transitions.
- fade ('inout'): indicate how audio should be faded at stream start/end:
  - in: audio fade-in when playing first frame
  - out: audio fade-out when playing last frame
  - inout: both fade-in and fade-out are enabled
  - other: no audio fade
- keep_alive (false): if using dedicated gpac process for one or more input, relaunch process(es) at source end if exit code is greater than 2 or if not responding after \`rtimeout\`
- seek (false): if true and \`keep_alive\` is active, adjust \`start\` according to the time elapsed since source start when relaunching process(es)
- prefetch (500): prefetch duration in ms (play before start time of source), 0 for no prefetch

## Source Locations
### Properties for \`sourceURL\` objects
- id (null): source URL identifier, used when reloading the playlist
- in (null): input URL or filter chain to load as string. Words starting with \`-\` are ignored. The first entry must specify a source URL, and additional filters and links can be specified using \`@N[#LINKOPT]\` and \`@@N[#LINKOPT]\` syntax, as in gpac
- port (null): input port for source. Possible values are:
  - pipe: launch a gpac process to play the source using GSF format over pipe
  - tcp, tcpu: launch a gpac process to play the source using GSF format over TCP socket (\`tcp\`) or unix domain TCP socket (\`tcpu\`)
  - not specified or empty string: loads source using the current process
  - other: use value as input filter declaration and launch \`in \ as dedicated process (e.g., \`in="ffmpeg ..." port="pipe://..."\`)
- opts (null): options for the gpac process instance when using dedicated gpac process, ignored otherwise
- media ('all'): filter input media by type, \`a\` for audio, \`v\` for video, \`t\` for text (several characters allowed, e.g. \`av\` or \`va\`), \`all\` accept all input media
- raw (true): indicate if input port is decoded AV (true) or compressed AV (false) when using dedicated gpac process, ignored otherwise

### Notes
When launching child process, the input filter is created first and the child process launched afterwards.

Warning: when launching child process directly (e.g. \`in="ffmpeg ..."\`), any relative URL used in \`in\` must be relative to the current working directory.

## 2D transformation
### Common properties for \`group\` and \`scene\` objects
- active (true): indicate if the object is active or not. An inactive object will not be refreshed nor rendered
- x (0): horizontal translation
- y (0): vertical translation
- cx (0): horizontal coordinate of rotation center
- cy (0): vertical coordinate of rotation center
- units ('rel'): unit type for \`x\`, \`y\`, \`cx\`, \`cy\`, \`width\` and \`height\`. Possible values are:
  - rel: units are expressed in percent of current reference (see below)
  - pix: units are expressed in pixels
- rotation (0): rotation angle of the scene in degrees
- hscale (1): horizontal scaling factor to apply to the group
- vscale (1): vertical skewing factor to apply to the scene
- hskew (0): horizontal skewing factor to apply to the scene
- vskew (0): vertical skewing factor to apply to the scene
- zorder (0): display order of the scene or of the offscreen group (ignored for regular groups)
- untransform (false): if true, reset parent tree matrix to identity before computing matrix
- mxjs (null): JS code for matrix evaluation

### Coordinate System
Each group or scene is specified in a local coordinate system for which {0,0} represents the center.
The local transformation matrix is computed as \`rotate(cx, cy, rotation)\` * \`hskew\` * \`vskew\` * \`scale(hscale, vscale)\` * \`translate(x, y)\`.

The default unit system (\`rel\`) is relative to the current established reference space:
- by default, the reference space is \`{output_width, output_height}\`, the origin {0,0} being the center of the output frame 
- any group with \`reference=true\`, \`width>0\` and \`height>0\` establishes a new reference space \`{group.width, group.height}\`

Inside a reference space \`R\`, relative coordinates are interpreted as follows:
- For horizontal coordinates, 0 means center, -50 means left edge (\`-R.width/2\`), 50 means right edge (\`+R.width/2\`).
- For vertical coordinates, 0 means center, -50 means bottom edge (\`-R.height/2\`), 50 means top edge (\`+R.height/2\`).
- For \`width\`, 100 means \`R.width\`.
- For \`height\`, 100 means \`R.height\`.

If \`width=height\`, the width is set to the computed height of the object.
If \`height=width\`, the height is set to the computed width of the object.
For \`x\` property, the following special values are defined:
- \`y\` will set the value to the computed \`y\`  of the object.
- \`-y\` will set the value to the computed \`-y\` of the object.
For \`y\` property, the following special values are defined:
- \`x\` will set the value to the computed \`x\` of the object.
- \`-x\` will set the value to the computed \`-x\` of the object.


Changing reference is typically needed when creating offscreen groups, so that children relative coordinates are resolved against the offscreen canvas size.

### z-ordering
\`zorder\` specifies the display order of the element in the offscreen canvas of the enclosing offscreen group, or on the output frame if no offscreen group in parent tree.
This order is independent of the parent group z-ordering. This allows moving objects of a group up and down the display stack without modifying the groups.

### Coordinate modifications through JS
The \`JSFun\` specified in \`mxjs\` has a single parameter \`tr\`.

The \`tr\` parameter is an object containing the following variables that the code can modify:
- x, y, cx, cy, hscale, vscale, hskew, vskew, rotation, untransform: these values are initialized to the current group values in local coordinate system units
- mx: 2D matrix of the scene for custom config
- set: if set to true by the code, the other operations are ignored and the modified \`mx\` is used as is. Otherwise, other matrix operations are added after
- update: if set to true, the object matrix will be recomputed at each frame even if no change in the group or scene parameters (always enforced to true if \`use\` is set)
- depth: for groups with \`use\`, indicates the recursion level of the used element. A value of 0 indcates this is a direct render of the element, otherwise it is a render through \`use\`

The \`JSFun\` may return false to indicate that the scene should be considered as inactive. Any other return value (undefined or not false) will mark the scene as active.

EX: "mxjs": "tr.rotation = (get_media_time() % 8) * 360 / 8; tr.update=true;"



## Grouping
### Properties for \`group\` objects
- id (null): group identifier
- scenes ([]): zero or more \`group\` or \`scene\` objects, cannot be animated or updated
- opacity (1): group opacity
- offscreen ('none'): set group in offscreen mode, cannot be animated or updated. An offscreen mode is not directly visible but can be used in some texture operations. Possible values are:
  - none: regular group
  - mask: offscreen surface is alpha+grey
  - color: offscreen surface is alpha+colors or colors if \`back_color\` is set
  - dual: same as \`color\` but allows group to be displayed
- scaler (1): when opacity or offscreen rendering is used, offscreen canvas size is divided by this factor (>=1)
- back_color ('none'): when opacity or offscreen rendering is used, fill offscreen canvas with the given color.
- width (-1): when opacity or offscreen rendering is used, limit offscreen width to given value (see below)
- height (-1): when opacity or offscreen rendering is used, limit offscreen height to given value (see below)
- use (null): id of group or scene to re-use
- use_depth (-1): number of recursion allowed for the used element, negative means global max branch depth as indicated by \`maxdepth\`
- reverse (false): reverse scenes order before draw
- reference (false): group is a reference space for relative coordinate of children nodes 

### Notes
The maximum depth of a branch in the scene graph is \`maxdepth\` (traversing aborts after this limit).

In offscreen mode, the bounds of the enclosed objects are computed to allocate the offscreen surface, unless \`width\` and  \`height\` are both greater or equal to 0.
Enforcing offscreen size is useful when generating textures for later effects.

Offscreen rendering is always done in software.

When enforcing \`scaler>1\` on a group with \`opacity==1\`, offscreen rendering will be used and the scaler applied.

When enforcing \`width\` and \`height\` on a group with \`opacity<1\`, the display may be truncated if children objects are out of the offscreen canvas bounds.

## Scenes
### Properties for \`scene\` objects
- id (null): scene identifier
- js ('shape'): scene type, either builtin (see below) or path to a JS module, cannot be animated or updated
- sources ([]): list of identifiers of sequences or offscreen groups used by this scene
- width (-1): width of the scene, -1 means reference space width
- height (-1): height of the scene, -1 means reference space height
- mix (null): a \`transition\` object to apply if more than one source is set, ignored otherwise
- mix_ratio (-1): mix ratio for transition effect, <=0 means first source only, >=1 means second source only
- volume (1.0): audio volume (0: silence, 1: input volume), this value is not clamped by the mixer.
- fade ('inout'): indicate how audio should be faded at scene activate/deactivate:
  - in: audio fade-in when playing first frame after scene activation
  - out: audio fade-out when playing last frame at scene activation
  - inout: both fade-in and fade-out are enabled
  - other: no audio fade
- any other property exposed by the underlying scene JS module.

### Notes
Inputs to a scene, whether \`sequence\` or offscreen \`group\`, must be declared prior to the scene itself.

A default scene will be injected if none is found when initially loading the playlist. If you need to start with an empty output, use a scene with no sequence associated.

## Transitions and Mixing effects
### JSON syntax
Properties for \`transition\` objects:
- id (null): transition identifier
- type: transition type, either builtin (see below) or path to a JS module
- dur: transition duration (transitions always end at source stop time). Ignored if transition is specified for a scene \`mix\`.
- fun (null): JS code modifying the ratio effect
- any other property exposed by the underlying transition module.

### Notes
A \`sequence\` of two media with playback duration (as indicated in \`source\`) of D1 and D2 using a transition of duration DT will result in a sequence lasting \`D1 + D2 - DT\`.

The \`JSFun\` specified by \`fun\` takes one argument \`ratio\` and must return the recomputed ratio.

EX "fun": "return ratio*ratio;"


## Timers and animations
### Properties for \`timer\` objects
- id (null): id of the timer
- dur (0): duration of the timer in seconds
- loop (false): loops timer when \`stop\` is not set
- start (-1): start time, as offset in seconds from current video time (number) or as date (string) or \`now\`
- stop (-1): stop time, as offset in seconds from current video time (number) or as date (string) or \`now\`, ignored if less than \`start\`
- keys ([]): list of keys used for interpolation, ordered list between 0.0 and 1.0
- anims ([]): list of \`animation\` objects

### Properties for \`animation\` objects
- values ([]): list of values to interpolate, there must be as many values as there are keys
- color (false): indicate the values are color (as strings)
- angle (false): indicate the interpolation factor is an angle in degree, to convert to radians (interpolation ratio multiplied by PI and divided by 180) before interpolation
- mode ('linear') : interpolation mode:
  - linear: linear interpolation between the values
  - discrete: do not interpolate
  - other: JS code modifying the interpolation ratio
- postfun (null): JS code modifying the interpolation result
- end ('freeze'): behavior at end of animation:
  - freeze: keep last animated values
  - restore: restore targets to their initial values
- targets ([]): list of strings indicating targets properties to modify. Syntax is:
  - ID@option: modifies property \`option\` of object with given ID
  - ID@option[IDX]: modifies value at index \`IDX\` of array property \`option\` of object with given ID

### Notes
Currently, only \`scene\`, \`group\`, \`transition\` and \`script\` objects can be modified through timers (see playlist updates).

The \`JSFun\` specified by \`mode\` has one input parameter \`interp\` equal to the interpolation factor and must return the new interpolation factor.
EX "mode":"return interp*interp;" 

The \`JSFun\` specified \`postfun\` has two input parameters \`res\` (the current interplation result) and \`interp\` (the interpolation factor), and must return the new interpolated value.
EX "postfun": "if (interp<0.5) return res*res; return res;" 

## Scripts
### Properties for \`script\` objects
- id (null): id of the script
- script (null): JavaScript code or path to JavaScript file to execute, cannot be animated or updated
- active (true): indicate if script is active or not


### Notes

Script objects allow read and write access to the playlist from script. They currently can only be used to modify scenes and groups and to activate/deactivate other scripts.

The \`JSFun\` function specified by \`fun\` has no input parameter. The return value (default 0) is the number of seconds (float) to wait until next evaluation of the script.

EX: { "script": "let s=get_scene('s1'); let rot = s.get('rotation'); rot += 10; s.set('rotation', rot); return 2;" }
This will change scene \`s1\` rotation every 2 seconds 

## Filter configuration
The playlist may specify configuration options of the filter, using a root object of type \'config\':
- property names are the same as the filter options
- property values are given in the native type, or as strings for fractions (format \`N/D\`), vectors (format \`WxH\`) or enums
- each declared property overrides the filter option of the same name (whether default or set at filter creation)

A configuration object in the playlist is only parsed when initially loading the playlist, and ignored when reloading it.

The following additional properties are defined for testing:
- reload_tests([]): list of playlists to reload
- reload_timeout(1.0): timeout in seconds before playlist reload
- reload_loop (0): number of times to repeat the reload tests (not including original playlist which is not reloaded)


## Playlist modification
The playlist file can be modified at any time.
Objects are identified across playlist reloads through their \`id\` property.
Objects that are not present after reloading a playlist are removed from the mixer. This implies that reloading a playlist will recreate most objects with no ID associated.

A \`sequence\` object modified between two reloads is refreshed, except for its \`start\` field if sequence active.

A \`source\` object shall have the same parent sequence between two reloads. Any modification on the object will only be taken into consideration when (re)loading the source.

A \`sourceURL\` object is not tracked for modification, only evaluated when activating the parent \`source\` object.

A \`scene\` or \`group\` object modified between two reloads is notified of each changed value.

A \`timer\` object modified between two reloads is shut down and restarted. Consequently, \`animation\` objects are not tracked between reloads.

A \`transition\` object may change between two reloads, but any modification on the object will only be taken into consideration when restarting the effect.

## Playlist example

The following is an example playlist using a sequence of two videos with a mix transition and an animated video area:

EX [
EX  {"id": "seq1", "loop": -1, "start": 0,  "seq":
EX   [
EX    { "id": "V1", "src": [{"in": "s1.mp4"}], "start": 60, "stop": 80},
EX    { "id": "V2", "src": [{"in": "s2.mp4"}], "stop": 100}
EX   ],
EX   "transition": { "dur": 1, "type": "mix"}
EX  },
EX  {"id": "scene1", "sources": ["seq1"]},
EX  {"start": 0, "dur": 10, "keys": [0, 1], "anims":
EX   [
EX    {"values": [50, 0],  "targets": ["scene1@x", "scene1@y"]},
EX    {"values": [0, 100],  "targets": ["scene1@width", "scene1@height"]}
EX   ]
EX  }
EX ]


# Updates Format

Updates can be sent to modify the playlist, rather than reloading the entire playlist.
Updates are read from a separate file specified in \`updates\`, inactive by default.

Warning: The \`updates\` file is only read when modified __AFTER__ the initialization of the filter.

The \`updates\` file content shall be either a single JSON object or an array of JSON objects.
The properties of these objects are:
- skip: if true or 1, ignores the update, otherwise apply it
- replace: string identifying the target replacement. Syntax is:
  - ID@name: indicate property name of element with given ID to replace
  - ID@name[idx]: indicate the index in the property name of element with given ID to replace
- with: replacement value, must be of the same type as the target value.

An \`id\` property cannot be updated.

The following playlist elements of a playlist can be updated:
- scene: all properties except \`js\` and read-only module properties
- group: all properties except \`scenes\` and  \`offscreen\`
- sequence: \`start\`, \`stop\`, \`loop\` and \`transition\` properties
- timer: \`start\`, \`stop\`, \`loop\` and \`dur\` properties
- transition: all properties
  - for sequence transitions: most of these properties will only be updated at next reload
  - for active scene transitions: whether these changes are applied right away depend on the transition module

EX [
EX  {"replace": "scene1@x", "with": 20},
EX  {"replace": "seq1@start", "with": "now"}
EX  }
EX ]

`;

