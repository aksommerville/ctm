varying vec2 vtexoffset;

BEGIN VERTEX SHADER

attribute vec2 position;
attribute float tile;

void main() {
  vec2 adjposition=(position*2.0)/screensize-1.0;
  adjposition.y=-adjposition.y;
  gl_Position=vec4(adjposition,0.0,1.0);
  vtexoffset=vec2(floor(mod(tile+0.5,16.0)),floor((tile+0.5)/16.0))/16.0;
  gl_PointSize=tilesize.x;
}

BEGIN FRAGMENT SHADER

uniform sampler2D sampler;
uniform float upsidedown;

void main() {
  vec2 texcoord=gl_PointCoord;
  if (upsidedown>0.5) texcoord.y=1.0-texcoord.y;
  texcoord=texcoord/16.0+vtexoffset;
  gl_FragColor=texture2D(sampler,texcoord);
}
