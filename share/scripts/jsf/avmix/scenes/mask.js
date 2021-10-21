import * as evg from 'evg'


export const description = "Screen mask";

export const help = `This scene sets the canvas alpa mask mode.

The canvas alpha mask is always full screen.

In software mode, combining mask effect in record mode and reverse group drawing allows drawing front to back while writing pixels only once.
`;

export const options = [
 {name:"mode", value: "off", desc: `if set, reset clipper otherwise set it to scene position and size
  - off: mask is disabled
  - on: mask is enabled, further draw operations will take place on mask
  - use: mask is enabled, further draw operations will be filtered by mask
  - use_inv: mask is enabled, further draw operations will be filtered by 1-mask
  - rec: mask is in record mode, further draw operations will be drawn on output and will set mask avlue to 0 
 `, dirty: UPDATE_SIZE},
 {}
];


export function load() { return {

_mode: 0,
update: function() {

  if (this.update_flag) {
    if (this.mode=="off") this._mode = GF_EVGMASK_NONE;
    else if (this.mode=="on") this._mode = GF_EVGMASK_DRAW;
    else if (this.mode=="onkeep") this._mode = GF_EVGMASK_DRAW_NO_CLEAR;
    else if (this.mode=="use") this._mode = GF_EVGMASK_USE;
    else if (this.mode=="use_inv") this._mode = GF_EVGMASK_USE_INV;
    else if (this.mode=="rec") this._mode = GF_EVGMASK_RECORD;
    else this._mode = 0;

    this.update_flag = 0;
  }

  if (!this._mode) return 2;
  return 2; 
},
fullscreen: function() { return -1; },
identity: function() { return false; },
is_opaque: function() { return false; },
draw: function(canvas)
{
  canvas_set_mask_mode(this._mode);
}


}; }
