#version 120
#pragma debug(on)

uniform sampler3D Texture0;

void main(void)
{
	gl_FragColor = texture3D(Texture0, gl_TexCoord[0].stp) * 25.0;
}
