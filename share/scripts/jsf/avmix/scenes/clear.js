import * as evg from 'evg'

export const description = "Screen clear";

export const help = `This scene clears the canvas area covered by the scene with a given color. 

The default clear color of the mixer is \`black\`.

The clear area is always axis-aligned in output frame, so when skew/rotation are present, the axis-aligned bounding box of the transformed scene area will be cleared.
`;


export const options = [
 {name:"color", value: "none", desc: "clear color", dirty: UPDATE_SIZE},
 {}
];


export function load() { return {

update: function() {
  if (!this.update_flag) return 1;

  this.clip = {};
  this.clip.x = 0;
  this.clip.y = 0;
  this.clip.w = this.width;
  this.clip.h = this.height;

  this.update_flag = 0;
  return 1;
},
fullscreen: function() { return -1; },
identity: function() { return false; },
is_opaque: function() { return true; },

draw: function()
{
  canvas_clear(this.color, this.clip);      
}

}; }
