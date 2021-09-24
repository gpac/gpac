export const description = "Video cross-fade";

export const help = "This transition performs cross-fade of source videos";


export const options = [
];


export function load() { return {

funct: null,
gl_ratio: null,

setup: function()
{
},

setup_gl: function(webgl, program, first_available_texture_unit)
{
},

apply: function(canvas, ratio, path, matrix, pids)
{
    if (use_gpu) {
      //nothing to do
    } else {
      canvas.path = path;
      canvas.matrix = matrix;
      canvas.fill(GF_EVG_OPERAND_MIX, ratio, pids[0].texture, pids[1].texture);
    }
},

get_shader_src: function()
{

  return `void main() {
      vec4 col_from = get_pixel_from(txcoord_from);
      vec4 col_to = get_pixel_to(txcoord_to);
      gl_FragColor = mix(col_from, col_to, ratio);
  }
  `;
}

}; }
