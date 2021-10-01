
export const help = `AVMix is a simple audio video mixer controlled by an updatable JSON playlist format. The filter can be used to:
- schedule video sequence(s) over time
- mix videos together
- layout of multiple videos
- overlay images, text and graphics over source videos

All input streams are decoded prior to entering the mixer.
- audio streams are mixed in software
- video streams are composed according to the \`gpu\` option
- other stream types are not yet supported

OpenGL hardware acceleration can be used, but the supported feature set is currently not the same with or without GPU.

In software mode, the mixer will detect wether any of the currently active video sources can be used as a base canvas for the output to save processing time.
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

The playlist describes:
- Media sequences: each sequence is a set of sources to be played continuously
- Transitions: sources in a sequence can be combined using transitions
- Scenes: a scene describes one graphical object to put on screen and if and how input video are mapped on objects
- Timers: a timer can be used to animate scene parameters in various fashions

The playlist content shall be either a single JSON object or an array of JSON objects, hereafter called root objects.
Root objects types can be indicated through a \`type\` property:
- seq: a \`sequence\` object
- url: a \`source\` object (if used as root, a default \`sequence\` object will be created)
- scene: a \`scene\` object
- timer: a \`timer\` object
- config: a \`config\` object

The \`type\` property of root objects is usually not needed as the parser guesses the object types from its properties.

A root object with a property \`skip\` set to anything but \`0\` or \`false\` is ignored.
Any unrecognized property not starting with \`_\` will be reported as warning.

A default scene will be injected if none is found when initially loading the playlist. If you need to start with an empty output, use a scene with no sequence associated.

Media source timing does not depend on the media being used by a scene or not, it is only governed by the \`sequence\` parameters.
This means that a \`sequence\` not used by any active scene will not be rendered (video nor audio).


## JSON syntax

Properties for \`sequence\` objects:
 - id (null): sequence identifier
 - loop (0): number of loops for the sequence (0 means no loop, -1 will loop forever)
 - start (0): sequence start time:
   - if positive number, offset in seconds from current clock
   - if negative number, sequence is not active
   - otherwise date or \`now\`.
 - stop (0): sequence stop time:
   - if positive number greater than \`start\`, offset in seconds from current clock
   - if negative number or less than \`start\`, sequence will stop only when over
   - otherwise, date or \`now\`.
 - transition (null): a \`transition\` object to apply between sources of the sequence
 - seq ([]): array of one or more \`source\` objects

Properties for \`source\` objects:
- id (null): source identifier, used when reloading the playlist
- src ([]): list of \`sourceURL\` describing the URLs to play. Multiple sources will be played in parallel
- start (0.0): media start time in source
- stop (0.0): media stop time in source, <=0 means until the end. Ignored if less than equal to \`start\`
- mix (true): if true, apply sequence transition or mix effect ratio as audio volume. Otherwise volume is not modified by transitions.
- fade ('inout'): indicate how audio should be faded at stream start/end:
  - in: audio fade-in when playing first frame
  - out: audio fade-out when playing last frame
  - inout: both fade-in and fade-out are enabled
  - other: no audio fade
- keep_alive (false): if using dedicated gpac process for one or more input, relaunch process(es) at source end if exit code is greater than 2 or if not responding after \`rtimeout\`
- seek (false): if true and \`keep_alive\` is active, adjust \`start\` according to the time elapsed since source start when relaunching process(es)
- prefetch (500): prefetch duration in ms (play before start time of source), 0 for no prefetch

Properties for \`sourceURL\` objects:
- id (null): source URL identifier, used when reloading the playlist
- in (null): input URL or filter chain to load as string. Words starting with \`-\` are ignored. The first entry must specify a source URL, and additional filters and links can be specified using \`@N[#LINKOPT]\` and \`@@N[#LINKOPT]\` syntax, as in gpac
- port (null): input port for source. Possible values are:
  - pipe: launch a gpac process to play the source using GSF format over pipe
  - tcp, tcpu: launch a gpac process to play the source using GSF format over TCP socket (\`tcp\`) or unix domain TCP socket (\`tcpu\`)
  - not specified or empty string: loads source using the current process
  - other: use value as input filter declaration and launch \'in\' as dedicated process (e.g., in="ffmpeg ..." port="pipe://...")
- opts (null): options for the gpac process instance when using dedicated gpac process, ignored otherwise
- media ('all'): filter input media by type, \`a\` for audio, \`v\` for video, \`t\` for text (several characters allowed, e.g. \`av\` or \`va\`), \`all\` accept all input media
- raw (true): indicate if input port is decoded AV (true)) or compressed AV (false) when using dedicated gpac process, ignored otherwise

Note: when launching child process, the input filter is created first and the child process launched afterwards.

Warning: when launching child process directly (e.g. \`in="ffmpeg ..."\`), any relative URL used in \`in\` must be relative to the current working directory.

Properties for \`scene\` objects:
- id (null): scene identifier
- js ('shape'): scene type, either builtin (see below) or path to a JS module
- sources ([]): list of identifiers of sequences used by this scene
- x (0): horizontal coordinate of the scene top-left corner, in percent of the output width (0 means left edge, 100 means right edge)
  - special value \`y\` indicates scene \`scene.y\`
  - special value \`-y\` indicates \`output_height - scene.y - scene.height\`
- y (0): vertical coordinate of the scene top-left corner, in percent of the output height (0 means top edge, 100 means bottom edge)
  - special value \`x\` indicates scene \`scene.x\`
  - special value \`-x\` indicates \`output_width - scene.w - scene.width\`
- width (100): width of the scene, in percent of the output width.
  - special value \`height\` indicates to use scene height
- height (100): height of the scene, in percent of the output height
  - special value \`width\` indicates to use scene width
- zorder (0): display order of the scene
- active (true): indicate if the scene is active or not. An inactive scene will not be refreshed nor rendered
- rotation (0): rotation angle of the scene in degrees (the rotation is counter-clockwise, around the scene center)
- hskew (0): horizontal skewing factor to apply to the scene
- vskew (0): vertical skewing factor to apply to the scene
- mix (null): a \`transition\` object to apply if more than one source is set, ignored otherwise
- mix_ratio (-1): mix ratio for transition effect, <=0 means first source only, >=1 means second source only
- volume (1.0): audio volume (0: silence, 1: input volume), this value is not clamped.
- fade ('inout'): indicate how audio should be faded at scene activate/deactivate:
  - in: audio fade-in when playing first frame after scene activation
  - out: audio fade-out when playing last frame at scene activation
  - inout: both fade-in and fade-out are enabled
  - other: no audio fade

Properties for \`transition\` objects:
- id (null): transition identifier
- type: transition type, either builtin (see below) or path to a JS module
- dur: transition duration (transitions always end at source stop time). Ignored if transition is specified for a scene \`mix\`.
- fun (null): JS code modifying the ratio effect called \`ratio\`, eg \`fun="ratio = ratio*ratio;"\`

Properties for \`timer\` objects:
- id (null): id of the timer
- dur (0): duration of the timer in seconds
- loop (false): loops timer when \`stop\` is not set
- start (-1): start time, as offset in seconds from current video time (number) or as date (string) or \`now\`
- stop (-1): stop time, as offset in seconds from current video time (number) or as date (string) or \`now\`, ignored if less than \`start\`
- keys ([]): list of keys used for interpolation, ordered list between 0.0 and 1.0
- anims ([]): list of \`animation\` objects

Properties for \`animation\` objects:
- values ([]): list of values to interpolate, there must be as many values as there are keys
- color (false): indicate the values are color (as strings)
- angle (false): indicate the interpolation factor is an angle in degree, to convert to radians (interpolation ratio multiplied by PI and divided by 180) before interpolation
- mode ('linear') : interpolation mode:
  - linear: linear interpolation between the values
  - discrete: do not interpolate
  - other: JS code modifying the interpolation ratio called \`interp\`, eg \`"interp = interp*interp;"\`
- postfun (null): JS code modifying the interpolation result \`res\`, eg \`"res = res*res;"\`
- end ('freeze'): behavior at end of animation:
  - freeze: keep last animated values
  - restore: restore targets to their initial values
- targets ([]): list of strings indicating targets properties to modify. Syntax is:
  - ID@option: modifies property \`option\` of object with given ID
  - ID@option[IDX]: modifies value at index \`IDX\` of array property \`option\` of object with given ID

Currently, only \`scene\` and \`transition\` objects can be modified through timers.

__Note on colors__
Colors are handled as strings, formatted as:
- the DOM color name (see gpac -h colors)
- HTML codes \`$RRGGBB\` or \`#RRGGBB\`
- RGB hex vales \`0xRRGGBB\`
- RGBA hex values \`0xAARRGGBB\`

## Filter configuration
The playlist may specify configuration options of the filter, using a root object of type \'config\':
- property names are the same as the filter options
- property values are given in the native type, or as strings (fractions, vectors, enums)
- each declared property overrides the filter option of the same name (whether default or set at filter creation)

A configuration object in the playlist is only parsed when initially loading the playlist, and ignored when reloading it.

The following additional properties are defined for testing:
- reload_tests([]): list of playlists to reload
- reload_timeout(1.0): timeout in seconds before playlist reload
- reload_loop (0): number of times to repeat the reload tests (not including orignal playlist which is not reloaded)


## Playlist modification

The playlist file can be modified at any time.
Objects are identified across playlist reloads through their \`id\` property.
Root objects that are not present after reloading a playlist are removed from the mixer.

A \`sequence\` object modified between two reloads is refreshed, except for its \`start\` field if sequence active.

A \`source\` object shall have the same parent sequence between two reloads. Any modification on the object will only be taken into consideration when (re)loading the source.

A \`sourceURL\` object is not tracked for modification, only evaluated when activating the parent \`source\` object.

A \`scene\` object modified between two reloads is notified of each changed value.

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


The following playlist elements of a playlist can be updated:
- scene: all properties except \`js\` and \`sources\`
- sequence: \`start\`, \`stop\`, \`loop\` and \`transition\` properties
- timer: \`start\`, \`stop\`, \`loop\` and \`dur\` properties
- transition: all properties
  - for sequence transitions, most of these properties will only be updated at next reload
  - for active scene transitions, whether these changes are applied right away depend on the transition module


IDs cannot be updated.


EX [
EX  {"replace": "scene1@x", "with": 20},
EX  {"replace": "seq1@start", "with": "now"}
EX  }
EX ]

`;

