#version 450 core

layout (location = 0) in ivec2 v_pos;

void main()
{
	gl_Position = vec4(v_pos, 0.0, 1.0);
}