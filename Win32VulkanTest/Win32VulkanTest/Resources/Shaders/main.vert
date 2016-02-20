#version 450

precision mediump float;

layout (binding = 0) uniform UBO {
	mat4 uModelView;
	mat4 uProjection;
} ubo;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec3 color;

void main() {
	color = inColor;
	gl_Position = ubo.uProjection * ubo.uModelView * vec4(inPosition, 1);
}
