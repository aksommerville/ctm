varying vec4 vcolor;

BEGIN VERTEX SHADER

attribute vec2 position;
attribute vec4 color;

void main() {
  vec2 adjposition=(position*2.0)/screensize-1.0;
  adjposition.y=-adjposition.y;
  gl_Position=vec4(adjposition,0.0,1.0);
  vcolor=color;
}

BEGIN FRAGMENT SHADER

void main() {
  gl_FragColor=vcolor;
}
