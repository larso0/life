#version 450 core

layout (triangle_strip, max_vertices = 4) out;

layout (points) in;

layout (push_constant) uniform _matrices
{
	mat4 mvp;
} matrices;

void main()
{
	vec4 pos = gl_in[0].gl_Position;

	gl_Position = matrices.mvp * pos;
	EmitVertex();

	gl_Position = pos;
	gl_Position.y += 1.0;
	gl_Position = matrices.mvp * gl_Position;
	EmitVertex();

	gl_Position = pos;
        gl_Position.x += 1.0;
        gl_Position = matrices.mvp * gl_Position;
        EmitVertex();

	gl_Position = pos;
        gl_Position.x += 1.0;
        gl_Position.y += 1.0;
        gl_Position = matrices.mvp * gl_Position;
        EmitVertex();

        EndPrimitive();
}
