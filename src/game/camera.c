void sync_camera_directions(struct v3* forward, struct v3* right, struct v2 camera_rotation)
{
	*forward = v3_new(
    	cos(radians(camera_rotation.x)) * cos(radians(camera_rotation.y)),
    	sin(radians(camera_rotation.y)),
    	sin(radians(camera_rotation.x)) * cos(radians(camera_rotation.y)));
	*forward = v3_normalize(*forward);

	struct v3 cam_up = {{{0, 1, 0}}};
	*right = v3_normalize(v3_cross(*forward, cam_up));
}
