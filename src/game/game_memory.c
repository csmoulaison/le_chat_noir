struct game_memory
{
    float t;

	float camera_speed;

	struct v3 camera_position;
	struct v2 camera_rotation_actual;
	struct v2 camera_rotation_target;
	struct v2 camera_rotation_per_frame;

	// Cached values that must be upated based on yaw and pitch
	struct v3 camera_forward;
	struct v3 camera_right;


	struct v3 cube_positions[CUBES_LEN];
	mat4 cube_orientations[CUBES_LEN];
	struct v3 cube_rotations_per_frame[CUBES_LEN];
};
