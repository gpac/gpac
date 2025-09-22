
export const description = "Color fading";
export const help = "This transition performs fade to/from color of source videos";

export const options = [
 {name:"color", value: "black", desc: "fade color"},
 {}
];


export function load() { return {

stencil: null,
uni_ratio: null,
uni_use_from: null,

setup: function()
{
  this.stencil = new evg.SolidBrush();
  this.stencil.set_color(this.color);
},

setup_gl: function(webgl, program, ratio_uniform, first_available_texture_unit)
{
  this.uni_ratio =webgl.getUniformLocation(program, 'ratio');
  this.uni_use_from =webgl.getUniformLocation(program, 'use_from');

  let col_uni = webgl.getUniformLocation(program, 'color');
  let a = sys.color_component(this.color, 0);
  let r = sys.color_component(this.color, 1);
  let g = sys.color_component(this.color, 2);
  let b = sys.color_component(this.color, 3);

  webgl.uniform4f(col_uni, r, g, b, a);

},

apply: function(canvas, ratio, path, pids)
{
  let tx;
  let use_from = true;
  ratio -= 0.5;
  if (ratio<0) {
    tx = pids[0].texture;
    ratio = 0.5 + ratio;
    ratio *= 2;
  } else {
    tx = pids[1].texture;
    ratio = 0.5 - ratio;
    ratio *= 2;
    use_from = false;
  }

  if (use_gpu) {
    canvas.uniform1f(this.uni_ratio, ratio);
    canvas.uniform1i(this.uni_use_from, use_from);

  } else {
    canvas.path = path;
    canvas.fill(GF_EVG_OPERAND_MIX, ratio, tx, this.stencil);
  }
},


get_shader_src: function()
{

  return `
  uniform vec4 color;
  uniform bool use_from;
  vec4 gf_apply_effect() {
      vec4 col_src;
      if (use_from) col_src = get_pixel_from(txcoord_from);
      else col_src = get_pixel_to(txcoord_to);

      return mix(col_src, color, ratio);
  }
  `;
}


}; }
