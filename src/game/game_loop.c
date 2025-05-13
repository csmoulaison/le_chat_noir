#define MAX_NETWORK_LINES 4

// TODO - overview
// * Dynamic up vector. Always up based on current forward? Think about what
//   that means.
// * Aspect ratio correction on reticle.
// * Resolution independence of CAMERA_RETICLE_OFFSET_MOD, which scales the
//   reticle offset and is currently resolution dependent.
// * Raycast from camera to hitting cube.
// * Cube knockback - change rotation based on hit normal and current rotation,
//   and knockback in reverse direction of normal (offset by our current speed,
//   of course).
// * Visual effect for hitting cube. Maybe flashes white momentarily. That
//   effect could even be the same as the bullet disappearing when fired at the UI
//   level.
// * When camera is hit by cube:
//   - Pause logic update: cubes blink out of existence one at a time at random.
//     Directly after the last disappears, the game restarts, seamlessly
//     spawning new cubes that come towards you immediately.
// * Don't bother with collision between cubes, I think.

void game_loop(
    void*                mem,
    size_t               mem_bytes,
    float                dt,
    uint32_t             window_w,
    uint32_t             window_h,
    struct input_state*  input,
    struct render_group* render_group)
{
    struct game_memory* game = (struct game_memory*)mem;

#define CAM_LOOK_SENSITIVITY 0.15
#define CAM_LOOK_ACCELERATION 2
#define CAM_LOOK_DECCELERATION 20
#define CAM_LOOK_DECCELERATION_RANGE 0.1
#define CAM_LOOK_VELOCITY_MAX 2

	// Position rotation target (and therefore reticle)
    struct v2 mouse_delta =
    {{{
	     (float)input->mouse_delta_x,
		-(float)input->mouse_delta_y,
    }}};

    game->camera_rotation_target = v2_add(
	    game->camera_rotation_target,
	    v2_scale(mouse_delta, CAM_LOOK_SENSITIVITY));

	float friction_scale = 1;
	if(v2_dot(v2_sub(game->camera_rotation_actual, game->camera_rotation_target), game->camera_rotation_per_frame) > 0)
	{
		friction_scale = 16;
	}

	// "Friction"
	game->camera_rotation_per_frame = v2_add(
		game->camera_rotation_per_frame, 
		v2_scale(game->camera_rotation_per_frame, -friction_scale * dt));
		
	game->camera_rotation_per_frame = v2_add(
		game->camera_rotation_per_frame, 
		v2_scale(v2_normalize(v2_sub(game->camera_rotation_target, game->camera_rotation_actual)), CAM_LOOK_ACCELERATION * dt));

    game->camera_rotation_actual = v2_add(
	    game->camera_rotation_actual, 
	    game->camera_rotation_per_frame);

	// Push camera rotation velocity towards target position.
	if(v2_distance(game->camera_rotation_target, game->camera_rotation_actual) < CAM_LOOK_DECCELERATION_RANGE && 
		v2_magnitude(game->camera_rotation_per_frame) < 0.1)
	{
		game->camera_rotation_per_frame = v2_zero();
	}

	sync_camera_directions(
		&game->camera_forward,
		&game->camera_right,
		game->camera_rotation_actual);

#define CAM_ACCELERATION_PER_SECOND 0.33;

	game->camera_speed += dt * CAM_ACCELERATION_PER_SECOND;

	mat4 cube_transforms[CUBES_LEN];
	for(uint32_t i = 0; i < CUBES_LEN; i++)
	{
		game->cube_positions[i] = v3_add(
			game->cube_positions[i], 
			v3_scale(game->camera_forward, -game->camera_speed * dt));
		glm_rotate(
			game->cube_orientations[i], 
			dt * radians(180), 
			game->cube_rotations_per_frame[i].data);

		if(v3_dot(game->camera_forward, game->cube_positions[i]) < 0)
		{
			reposition_cube(
				&game->cube_positions[i], 
				game->cube_orientations[i], 
				game->camera_forward, 
				game->camera_right);
		}

		glm_mat4_identity(cube_transforms[i]);
		glm_translate(cube_transforms[i], game->cube_positions[i].data);
		glm_mat4_mul(cube_transforms[i], game->cube_orientations[i], cube_transforms[i]);
	}	

	render_group->clear_color = v3_new(.0, .0, .0);
	render_group->max_draw_distance_z = MAX_DRAW_DISTANCE_Z;

	render_group->camera_position = game->camera_position;
	render_group->camera_target = v3_add(game->camera_position, game->camera_forward);

#define CAMERA_RETICLE_OFFSET_MOD 0.035

	struct v2 window_bounds = {{{ (float)window_w, (float)window_h }}};
	struct v2 reticle_offset_mod = {{{ -CAMERA_RETICLE_OFFSET_MOD, CAMERA_RETICLE_OFFSET_MOD }}};
	render_group->reticle_offset = v2_div(v2_sub(game->camera_rotation_actual, game->camera_rotation_target), v2_mul(window_bounds, reticle_offset_mod));
	
	memcpy(render_group->cube_transforms, cube_transforms, sizeof(struct m4) * CUBES_LEN);
}
