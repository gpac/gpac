import * as evg from 'evg'

export const description = "Screen clear";

export const help = `This scene clears the canvas area covered by the scene with a given color. 

The default clear color of the mixer is \`black\`.
`;


export const options = [
 {name:"color", value: "none", desc: "clear color", dirty: UPDATE_SIZE},
 {}
];


export function load() { return {

update: function() {
  if (!this.update_flag) return 1;

  this.clip = {};
  this.clip.x = this.x - this.width/2;
  this.clip.y = this.height/2 - this.y;
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
