#version 120
#pragma debug(on)

uniform sampler3D Texture0;
uniform float bias;
uniform float scale;

void main(void)
{
	gl_FragColor = (texture3D(Texture0, gl_TexCoord[0].stp) + bias) * scale;
}
