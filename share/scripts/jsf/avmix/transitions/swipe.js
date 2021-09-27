export const description = "Video swipe";

export const help = "This transition performs simple 2D affine transformations for source videos transitions, with configurable effect origin";


export const options = [
 {name:"from", value: "left", desc: `direction of video 2 entry. Possible values are:
  - left: from left to right edges
  - right: from right to left edges
  - top: from top to bottom edges
  - bottom: from bottom to top edges
  - topleft: from top-left to bottom-right corners
  - topright: from top-right to bottom-left corners
  - bottomleft: from bottom-left to top-right corners
  - bottomright: from bottom-right to top-left corners
`},
 {name:"mode", value: "slide", desc: `how video 2 entry impacts video 1. Possible values are:
  - slide: video 1 position is not modified
  - push: video 2 pushes video 1 away
  - squeeze: video 2 squeezes video 1 along opposite edge
  - grow: video 2 size increases, video 1 not modified
  - swap: video 2 size increases, video 1 size decreases
`},
 {}
];


export function load() { return {

h_mode: 0,
v_mode: 0,
mod: 0,

setup: function()
{

    if (this.mode == "push") this.mod = 1;
    else if (this.mode == "squeeze") this.mod = 2;
    else if (this.mode == "grow") this.mod = 3;
    else if (this.mode == "swap") this.mod = 4;
    else this.mod = 0;

    this.h_mode = 0;
    this.v_mode = 0;
    if (this.from == "left") this.h_mode = 1;
    else if (this.from == "right") this.h_mode = 2;
    else if (this.from == "top") this.v_mode = 1;
    else if (this.from == "bottom") this.v_mode = 2;
    else if (this.from == "topleft") {
      this.h_mode = 1;
      this.v_mode = 1;
    }
    else if (this.from == "topright") {
      this.h_mode = 2;
      this.v_mode = 1;
    }
    else if (this.from == "bottomleft") {
      this.h_mode = 1;
      this.v_mode = 2;
    }
    else if (this.from == "bottomright") {
      this.h_mode = 2;
      this.v_mode = 2;
    }
},

apply: function(canvas, ratio, path, matrix, pids)
{

  let rc = path.bounds;
  if (typeof rc == 'undefined') return;

  let new_w = rc.w;
  let new_h = rc.h;
  let new_x = 0;
  let new_y = 0;

  //left
  if (this.h_mode == 1) {
    new_x = -rc.w/2 + rc.w*ratio/2;
    new_w = rc.w*ratio;
  }
  //right
  else if (this.h_mode == 2) {
    new_x = rc.w/2 - rc.w*ratio/2;
    new_w = rc.w*ratio;
  }

  //top
  if (this.v_mode==1) {
    new_y = rc.h/2 - rc.h*ratio/2;
    new_h = rc.h*ratio;
  }
  //bottom
  else if (this.v_mode==1) {
    new_y = -rc.h/2 + rc.h*ratio/2;
    new_h = rc.h*ratio;
  }

  let new_path = path.clone();

  //add rectangle to path
  new_path.rectangle(new_x, new_y, new_w, new_h, true);
  //use zero-nonzero fill
  new_path.zero_fill = true;


  if (! this.mod) {
    canvas.path = new_path;
    canvas.matrix = matrix;
    canvas.fill(GF_EVG_OPERAND_ODD_FILL, ratio, pids[0].texture, pids[1].texture);
    return;

  }

  let mx1_bck = pids[0].texture.mx;
  let mx2_bck = pids[1].texture.mx;

  let mx1 = mx1_bck.copy();
  let mx2 = mx2_bck.copy();

  let t_x1 = 0;
  let t_y1 = 0;
  let t_x2 = 0;
  let t_y2 = 0;

  //left
  if (this.h_mode==1) {
    t_x1 = rc.w*ratio;
    t_x2 = rc.w*(ratio-1);
  }
  //right
  else if (this.h_mode==2) {
    t_x1 = -rc.w*ratio;
    t_x2 = -rc.w*(ratio-1);
  }

  //top
  if (this.v_mode==1) {
    t_y1 = -rc.h*ratio;
    t_y2 = -rc.h*(ratio-1);
  }
  //bottom
  else if (this.v_mode==2) {
    t_y1 = rc.h*ratio;
    t_y2 = rc.h*(ratio-1);
  }
  mx1.translate(t_x1, t_y1);
  mx2.translate(t_x2, t_y2);

  if (this.mod == 2) 
    mx1.scale(1-ratio, 1-ratio);
  else if (this.mod == 3) 
    mx2.scale(ratio, ratio);
  else if (this.mod == 4) {
    mx1.scale(1-ratio, 1-ratio);
    mx2.scale(ratio, ratio);
  }


  let pad1_bck = pids[0].texture.get_pad_color();
  let pad2_bck = pids[1].texture.get_pad_color();
  //pad with the same color !
  pids[0].texture.set_pad_color(pad1_bck ? pad1_bck : 'black');
  pids[1].texture.set_pad_color(pad1_bck ? pad1_bck : 'black');

  pids[0].texture.mx = mx1;
  pids[1].texture.mx = mx2;

  canvas.path = new_path;
  canvas.matrix = matrix;
  canvas.fill(GF_EVG_OPERAND_ODD_FILL, ratio, pids[0].texture, pids[1].texture);

  pids[0].texture.set_pad_color(pad1_bck);
  pids[1].texture.set_pad_color(pad2_bck);

  pids[0].texture.mx = mx1_bck;
  pids[1].texture.mx = mx2_bck;
}

}; }
