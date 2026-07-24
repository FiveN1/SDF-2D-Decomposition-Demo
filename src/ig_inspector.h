static void ig_inspector_draw() {

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    window_flags |= ImGuiWindowFlags_NoMove;

    static int location = 0;
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = igGetMainViewport();
    ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 window_pos, window_pos_pivot;
    window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
    window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
    window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
    window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
    igSetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    igSetNextWindowViewport(viewport->ID);
    window_flags |= ImGuiWindowFlags_NoMove;
    igSetNextWindowBgAlpha(0.35f); // Transparent background

    igBegin("SDF 2D Decomposition", NULL, window_flags);
    igText("SDF 2D Decomposition demo");
    igText("ms: %.3f, fps: %.1f", sapp_frame_duration() * 1000.0f, 1.0f / sapp_frame_duration());
    igSeparator();
    igText("Use left mouse button to move and scroll to zoom");

    igSeparator();
    igText("Settings");

    sdf_decomp_desc* desc = &app_data.decomp_description;
    
    igSliderInt("Iteration count", &desc->iteration_count, 1, 64, NULL, 0);

    igText("Grid:");
    igSliderInt("Grid resolution", &desc->grid_resolution, 2, 64, NULL, 0);
    igSliderFloat("Grid size", &desc->grid_size, 0.0f, 2.0f, NULL, 0);

    if (igButton("Run decomposition", (ImVec2_c) { 0 })) {
        app_data.decomposed_sdf.free_index = 0;
        sdf_decomposition_state.running = true;
    }

    if (igButton("Clear", (ImVec2_c) { 0 })) {
        app_data.decomposed_sdf.free_index = 0;
    }

    igSeparator();
    igText("Debug view");
    igCheckbox("Draw grid", &sdf_decomposition_state.debug_view.draw_grid);
    igCheckbox("Draw gradient descent (performence intensive)", &sdf_decomposition_state.debug_view.draw_grad_descnet);

    igSeparator();
    igText("Shape editor");

    sdf_buffer* shape_buff = &app_data.original_sdf;

    for (int i = 0; i < shape_buff->free_index; i++) {
        char buff[32];
        snprintf(buff, 32, "sdf_shape_%i", i);
        if (igTreeNode_Str(buff)) {
            sdf_shape* shape = sdf_get_shape(shape_buff, (sdf_shape_id) { .id = i });

            float temp_arr2[2] = { shape->position.x, shape->position.y };
            igDragFloat2("position", temp_arr2, 0.01f, 0.0f, 0.0f, NULL, 0);
            shape->position.x = temp_arr2[0];
            shape->position.y = temp_arr2[1];
            
            igSliderAngle("angle", &shape->angle, 0.0f, 360.0f, NULL, 0);

            temp_arr2[0] = shape->size.x, temp_arr2[1] = shape->size.y;
            igDragFloat2("size", temp_arr2, 0.01f, 0.0f, 16.0f, NULL, 0);
            shape->size.x = temp_arr2[0];
            shape->size.y = temp_arr2[1];

            const char* items[_SDF_TYPE_COUNT] = {
                "SDF_TYPE_NONE",
                "SDF_TYPE_CIRCLE",
                "SDF_TYPE_BOX",
                "SDF_TYPE_EQ_TRIANGLE",
                "SDF_TYPE_STAR",
                "SDF_TYPE_HORSESHOE"
            };
            igCombo_Str_arr("shape type", (int*)&shape->type, items, _SDF_TYPE_COUNT, _SDF_TYPE_COUNT);
            
            igTreePop();
        }
    }
    if (igButton("Add sdf_shape", (ImVec2_c) { 0 })) {
        sdf_add_shape(shape_buff, &(sdf_shape){
            .type = SDF_TYPE_CIRCLE
        });
    }
    if (igButton("Pop sdf_shape", (ImVec2_c) { 0 })) {
        sdf_pop_shape(shape_buff);
    }

    igEnd();
}