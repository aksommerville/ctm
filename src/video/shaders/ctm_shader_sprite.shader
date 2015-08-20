varying vec2 vtexoffset;
varying vec4 vprimary;
varying vec4 vtint;
varying float vflop;

BEGIN VERTEX SHADER

attribute vec2 position;
attribute float tile;
attribute vec4 primary;
attribute vec4 tint;
attribute float flop;

void main() {
  vec2 adjposition=(position*2.0)/screensize-1.0;
  adjposition.y=-adjposition.y;
  gl_Position=vec4(adjposition,0.0,1.0);
  vtexoffset=vec2(floor(mod(tile+0.5,16.0)),floor((tile+0.5)/16.0))/16.0;
  vprimary=primary;
  vtint=tint;
  vflop=flop;
  gl_PointSize=tilesize.x;
}

BEGIN FRAGMENT SHADER

uniform sampler2D sampler;
uniform float upsidedown;

void main() {

  vec2 texcoord=gl_PointCoord;
  if (vflop>0.5) texcoord.x=1.0-texcoord.x;
  if (upsidedown>0.5) texcoord.y=1.0-texcoord.y;
  
  texcoord=texcoord/16.0+vtexoffset;
  vec4 color=texture2D(sampler,texcoord);
  color.a*=vprimary.a;
  if (color.a<=0.0) discard;

  // Primary color.
  if ((color.r==color.g)&&(color.g==color.b)) {
    if (color.r<0.5) {
      color.rgb=mix(vec3(0.0,0.0,0.0),vprimary.rgb,color.r*2.0);
    } else {
      color.rgb=mix(vprimary.rgb,vec3(1.0,1.0,1.0),(color.r-0.5)*2.0);
    }
  }

  // Tint.
  if (vtint.a>0.0) {
    color.rgb=mix(color.rgb,vtint.rgb,vtint.a);
  }

  gl_FragColor=color;
}
