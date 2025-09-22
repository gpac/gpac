export const description = "Wrapper for gl-transitions";


function load_fx(fx_name, local_dist)
{
  let fx_obj = {uniforms: [], frag_shader: null, author: null, license: null};


  let fx_file;
  if (local_dist)
    fx_file = filter.jspath + 'transitions/gl-transitions/' + fx_name + '.glsl';
  else
    fx_file = fx_name;

  let source = sys.load_file(fx_file, true);
  if (!source || !source.length) return null;

  let lines = source.split('\n');

  lines.forEach( line => {
    let idx;
    while (line.charAt(0) ==' ') {
      line = line.slice(1);
    }
    if (!line.length) return;

    if (!fx_obj.author) {
      idx = line.indexOf('uthor');
      if (idx>0) {
        fx_obj.author = line.slice(idx+6);
        while (fx_obj.author.charAt(0) == ' ') {
          fx_obj.author = fx_obj.author.slice(1);
        } 
        return;
      }
    }
    if (!fx_obj.license) {
      idx = line.indexOf('icense');
      if (idx>0) {
        fx_obj.license = line.slice(idx+7);
        while (fx_obj.license.charAt(0) == ' ') {
          fx_obj.license = fx_obj.license.slice(1);
        } 
        return;
      }
    }

    if (! line.startsWith('uniform')) return;

    let code = line.split(' ');
    let uni = {};
    uni.type = code[1];
    uni.name = code[2].split(';');
    uni.name = uni.name[0];
    
    code = line.split('=');
    let value = code[1];
    if (typeof value != 'string') {
      uni.value = null;
      if (uni.type != 'sampler2D') {
        print(GF_LOG_ERROR, 'Uniform with no default value: ' + line);
      }
    } else {
      while (value.charAt(0) ==' ') {
        value = value.slice(1);
      }

      if (uni.type == 'bool') {
        if (value=='true') uni.value = true;
        else uni.value = false;
      } else if (uni.type == 'int') {
        uni.value = Number.parseInt(value);
      } else if (uni.type == 'float') {
        uni.value = Number.parseFloat(value);
      } else if ((uni.type == 'vec2') || (uni.type == 'vec3') || (uni.type == 'vec4')
        || (uni.type == 'ivec2') || (uni.type == 'ivec3') || (uni.type == 'ivec4')
      ) {
        idx = value.indexOf(uni.type);
        let type_len = uni.type.length;
        let oval = value;
        let nb_vals = 2;
        if (uni.type.indexOf('3')>0) nb_vals = 3;
        else if (uni.type.indexOf('4')>0) nb_vals = 4;

        if (idx>=0) {
          value = value.slice(idx+type_len+1);
          value = value.split(')');
          value = value[0].split(',');
          uni.value = [];
          value.forEach( v => {
            let f = Number.parseFloat(v);
            uni.value.push(f);
          });

          while (uni.value.length < nb_vals) {
            if (value.length==1) {
              uni.value.push(uni.value[0]);
            } else {
              print(GF_LOG_WARNING, 'Not enough scalars for uniform type ' + uni.type + ' - value ' + value + ', padding with 0');
              uni.value.push(0);
            }
          }
          uni.value.length = nb_vals;
          //safety check
          for (idx=0; idx<nb_vals; idx++) {
            if (uni.value[idx] != uni.value[idx]) {
              print(GF_LOG_ERROR, 'Error parsing value ' + oval + ' for type ' + uni.type + ': got NaN, forcing to 0');
              uni.value[idx] = 0;
            }
          }
        } else {
          print(GF_LOG_ERROR, 'Wrong format for uniform type ' + uni.type + ' - value ' + value);
        }
      } else {
        print(GF_LOG_ERROR, 'Uniform type not handled: ' + uni.type + ' - value ' + value);
      }
    }
    fx_obj.uniforms.push(uni);
  });

  source = source.replaceAll('getFromColor', 'get_pixel_from');
  source = source.replaceAll('getToColor', 'get_pixel_to');
  source = source.replaceAll(/\bratio\b/g, 'video_ar');
  source = source.replaceAll(/\bprogress\b/g, 'ratio');

  source += `

vec4 gf_apply_effect() {
  return transition(txcoord_from);
}
`;

  fx_obj.frag_shader = source;
  return fx_obj;
}


export let help='';

let myhelp = globalThis.help_mod || null;
let full_help=false;
if (myhelp == 'gltrans') {
  full_help=true;
} 
//don't print in md or man for the time being
else if (sys.get_opt("temp", "gendoc") == "yes") {
//  full_help=true;
}


if (full_help) {

help = `This transition module wraps gl-transitions, see https://gl-transitions.com/

## Available effects
`;

//load all modules
let effects = sys.enum_directory(filter.jspath+'transitions/gl-transitions/', "*.glsl");
effects.forEach( fx_file => {
  let name = fx_file.name.split('.')[0];
  let fx;
  try {
    fx = load_fx(name, true);
  } catch (e) {
    print(GF_LOG_ERROR, 'Failed to load gl transition ' + name + ': ' + e);
    return;
  }

  if (!fx) return;

  help += name;
  if (fx.author || fx.license) {
    help += ' (';
    if (fx.author) help += 'author: ' + fx.author;
    if (fx.license) help += ', license: ' + fx.license;
    help += ')';
  }
  help += '\n';

  fx.uniforms.forEach(uni =>{
    help += ' - ' + uni.name + ' (' + uni.type + '): ' + uni.value + '\n';
  });
  help += '\n';

});

} else {

help = "This transition module wraps gl-transitions, see https://gl-transitions.com/ and `gpac -h avmix:gltrans` for builtin transitions";

}

export const options = [
 {name:"fx", value: "", desc: "effect name for built-in effects, or path to gl-transition GLSL file"},
 {}
];



export function load()
{
  let fx;
  if (arguments.length) {
    let local_dist = true;
    let args = arguments[0];
    let fx_name = args.fx || null;
    if (!fx_name) return null;
    if (fx_name.indexOf('.') >= 0) {
      fx_name = resolve_url(fx_name);
      local_dist = false;
    }
    fx = load_fx(fx_name, false);
    if (!fx) return null;
 } else {
    fx = {uniforms: [], frag_shader: null, author: null, license: null};
 }

 return {

setup_gl: function(webgl, program, first_available_texture_unit)
{

  this.options.forEach( uni => {
    uni.loc = webgl.getUniformLocation(program, uni.name);
    if (uni.loc==null) {
      print(GF_LOG_ERROR, 'No uniform for ' + uni.name);
      return;
    }
    this.update_flag = UPDATE_SIZE;
  });
},

apply: function(webgl, ratio, path, pids)
{
  if (!this.update_flag) return;
  this.update_flag = 0;

  this.options.forEach( uni => {
    let value = this[uni.name] || uni.value;
    if (!uni.loc) return;

    if (uni.type=='bool') {
        webgl.uniform1i(uni.loc, value);
    } else if (uni.type=='int') {
        webgl.uniform1i(uni.loc, value);
    } else if (uni.type=='float') {
        webgl.uniform1f(uni.loc, value);
    } else if (uni.type=='vec2') {
        webgl.uniform2f(uni.loc, value[0], value[1]);
    } else if (uni.type=='vec3') {
        webgl.uniform3f(uni.loc, value[0], value[1], value[2]);
    } else if (uni.type=='vec4') {
        webgl.uniform4f(uni.loc, value[0], value[1], value[2], value[3]);
    } else if (uni.type=='ivec2') {
        webgl.uniform2i(uni.loc, value[0], value[1]);
    } else if (uni.type=='ivec3') {
        webgl.uniform3i(uni.loc, value[0], value[1], value[2]);
    } else if (uni.type=='ivec4') {
        webgl.uniform4i(uni.loc, value[0], value[1], value[2], value[3]);
    } else {
      print(GF_LOG_WARNING, 'uniform type ' + uni.type + ' updating not supported yet');
      return;
    }
  });
},

get_shader_src: function()
{
  return this.frag_shader;
},

frag_shader: fx.frag_shader,

options: fx.uniforms
}; 
}
