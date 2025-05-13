#version 450

layout(location = 0) in vec2 in_pos;

layout(binding = 0) uniform ubo_reticle {
	vec2 reticle_pos;
	float reticle_scale_y;
} global;

void main() {
	vec2 scaled_in = vec2(in_pos.x, in_pos.y * global.reticle_scale_y);
	gl_Position = vec4(global.reticle_pos + scaled_in, 0.0, 1.0);
}

