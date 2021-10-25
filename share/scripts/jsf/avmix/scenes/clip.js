import * as evg from 'evg'


export const description = "Screen clip";

export const help = `This scene resets the canvas clipper or sets the canvas clipper to the scene area.

The clipper is always axis-aligned in output frame, so when skew/rotation are present, the axis-aligned bounding box of the transformed clipper will be used.

Clippers are handled through a stack, resetting the clipper pops the stack and restores previous clipper.
If a clipper is already defined when setting the clipper, the clipper set is the intersection of the two clippers.
`;

export const options = [
 {name:"reset", value: false, desc: "if set, reset clipper otherwise set it to scene position and size", dirty: UPDATE_SIZE},
 {name:"stack", value: true, desc: "if false, clipper is set/reset independently of the clipper stack (no intersection, no push/pop of the stack)", dirty: UPDATE_SIZE},
 {}
];


export function load() { return {

update: function() {
  if (this.update_flag) {
    this.clip = {};
    this.clip.x = 0;
    this.clip.y = 0;
    this.clip.w = this.width;
    this.clip.h = this.height;
  }
  return this.reset ? 1 : 2; 
},
fullscreen: function() { return -1; },
identity: function() { return false; },

draw: function(canvas)
{
  canvas_set_clipper(this.reset ? null : this.clip, this.stack);
}


}; }
