varying vec2 vtexcoord;

BEGIN VERTEX SHADER

attribute vec2 position;
attribute vec2 texcoord;

void main() {
  vec2 adjposition=(position*2.0)/screensize-1.0;
  adjposition.y=-adjposition.y;
  gl_Position=vec4(adjposition,0.0,1.0);
  vtexcoord=texcoord;
}

BEGIN FRAGMENT SHADER

uniform float alpha;
uniform sampler2D sampler;

void main() {
  gl_FragColor=texture2D(sampler,vtexcoord);
  gl_FragColor.a*=alpha;
}
