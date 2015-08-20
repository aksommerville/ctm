varying vec2 vtexoffset;
varying vec4 vcolor;
uniform float size;

BEGIN VERTEX SHADER

attribute vec2 position;
attribute float tile;
attribute vec4 color;

void main() {
  vec2 adjposition=(position*2.0)/screensize-1.0;
  adjposition.y=-adjposition.y;
  gl_Position=vec4(adjposition,0.0,1.0);
  vtexoffset=vec2(floor(mod(tile+0.5,16.0)),floor((tile+0.5)/16.0))/vec2(16.0,8.0);
  vcolor=color;
  gl_PointSize=size;
}

BEGIN FRAGMENT SHADER

uniform sampler2D sampler;
uniform float upsidedown;

void main() {
  vec2 texcoord=gl_PointCoord;
  if ((texcoord.x<=0.25)||(texcoord.x>=0.75)) discard;

  if (upsidedown>0.5) texcoord.y=1.0-texcoord.y;
  
  texcoord.x-=0.25;
  texcoord.x*=2.0;
  texcoord/=vec2(16.0,8.0);
  texcoord+=vtexoffset;
  float alpha=texture2D(sampler,texcoord).a;
  alpha*=vcolor.a;
  gl_FragColor=vec4(vcolor.rgb,alpha);
}
