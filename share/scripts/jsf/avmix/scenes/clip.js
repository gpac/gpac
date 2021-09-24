import * as evg from 'evg'


export const description = "Screen clip";

export const help = "This scene sets the clipper or reset the clipper on the canvas.";

export const options = [
 {name:"reset", value: false, desc: "if set, reset clipper otherwise set it to scene position and size", dirty: UPDATE_SIZE},
 {}
];


export function load() { return {

update: function() {
  if (this.update_flag) {
    this.clip = {};
    this.clip.x = this.x - this.width/2;
    this.clip.y = this.height/2 - this.y;
    this.clip.w = this.width;
    this.clip.h = this.height;
  }
  return this.reset ? 1 : 2; 
},
fullscreen: function() { return -1; },
identity: function() { return false; },

draw: function(canvas)
{
  canvas_set_clipper(this.reset ? null : this.clip);      
}


}; }
