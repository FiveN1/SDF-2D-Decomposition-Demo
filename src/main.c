
/*
    SDF decomposition demo in 2D 
    by: Tomas Verheyen (2026)
*/

#include<stdio.h>

#define SOKOL_IMPL
#include<sokol/1ecbb1c/sokol_app.h>
#include<sokol/1ecbb1c/sokol_gfx.h>
#include<sokol/1ecbb1c/sokol_glue.h>
#include<sokol/1ecbb1c/util/sokol_gl.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include<cimgui/cimgui.h>
#include<sokol/1ecbb1c/util/sokol_imgui.h>
#include<vecmath/vecmath.h>

#define AL_IMPL
#include"al/230426/al_camera.h"

#include"sdf.h"
#include"shader.glsl.h"

static struct app_data {
    sg_pass_action pass_action;
    sg_bindings bindings;
    sg_pipeline pipeline;

    // uniforms
    vs_params_t vs_params;
    fs_params_t fs_params;

    // buffers on GPU
    sg_buffer original_sdf_ssbo;
    sg_buffer decomposed_sdf_ssbo;

    // buffers on CPU
    sdf_buffer original_sdf;
    sdf_buffer decomposed_sdf;

    al_camera_t camera;

    // decomposition description
    sdf_decomp_desc decomp_description;
} app_data;

#include"ig_inspector.h"

void init() {
    sg_setup(&(sg_desc) { .environment = sglue_environment() });
    sgl_setup(&(sgl_desc_t) { 0 });
    simgui_setup(&(simgui_desc_t) { 0 });
    ImGuiIO* ig_io = igGetIO_Nil();
    ig_io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    app_data.pass_action = (sg_pass_action){ .colors[0] = {
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = (sg_color){0.0f, 0.5f, 0.0f}
    } };
    float fsq_verts[] = { -1.0f, -3.0f, 3.0f, 1.0f, -1.0f, 1.0f };
    app_data.bindings = (sg_bindings){
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc) {
            .data = SG_RANGE(fsq_verts)
        }),
    };
    app_data.pipeline = sg_make_pipeline(&(sg_pipeline_desc) {
        .shader = sg_make_shader(shader_shader_desc(sg_query_backend())),
            .layout = (sg_vertex_layout_state){ .attrs[0].format = SG_VERTEXFORMAT_FLOAT2 }
    });
    app_data.original_sdf_ssbo = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .usage.stream_update = true,
        .size = sizeof(sdf_shape_t) * (MAX_SDF_PRIMITIVES + 1)
    });
    app_data.bindings.views[VIEW_original_sdf_buffer] = sg_make_view(&(sg_view_desc) {
        .storage_buffer = { .buffer = app_data.original_sdf_ssbo },
    });
    app_data.decomposed_sdf_ssbo = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .usage.stream_update = true,
        .size = sizeof(sdf_shape_t) * (MAX_SDF_PRIMITIVES + 1)
    });
    app_data.bindings.views[VIEW_decomposed_sdf_buffer] = sg_make_view(&(sg_view_desc) {
        .storage_buffer = { .buffer = app_data.decomposed_sdf_ssbo },
    });
    app_data.vs_params = (vs_params_t){
        .aspect = 1.0f
    };
    app_data.fs_params = (fs_params_t){
        .original_sdf_count = 0,
        .decomposed_sdf_count = 0
    };
    
    //
    // CAMERA
    //

    app_data.camera = al_camera_create(&(al_camera_desc_t) {
        .mode = AL_CAMERA_MODE_2D_GRABBER,
        .position = vec3(0.0f, 0.0f, 0.0f),
        .scale = vec3f(0.5f),
    });

    //
    // BASE SDF
    //

    sdf_add_shape(&app_data.original_sdf, &(sdf_shape){
        .type = SDF_TYPE_STAR,
        .position = vec2(0.0f, 0.0f),
        .size = vec2f(0.3f)
    });
    
    //
    // DECOMPOSITION
    //

    sdf_decomposition_set_state(&app_data.original_sdf, &app_data.decomposed_sdf);

    app_data.decomp_description = (sdf_decomp_desc){
        .iteration_count = 16,
        .grid_resolution = 16,
        .grid_size = 0.5f
    };
}

void delete() {
    simgui_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

void frame() {

    al_camera_update(&app_data.camera);

    sdf_decompose_step(&app_data.decomp_description);

    sg_begin_pass(&(sg_pass) { .action = app_data.pass_action, .swapchain = sglue_swapchain() });

    // uniform update
    app_data.vs_params.aspect = sapp_widthf() / sapp_heightf();
    app_data.fs_params.original_sdf_count = app_data.original_sdf.free_index;
    app_data.fs_params.decomposed_sdf_count = app_data.decomposed_sdf.free_index;
    app_data.fs_params.camera_position = vec3_xy(app_data.camera.position);
    app_data.fs_params.camera_zoom = app_data.camera.scale.x;

    // buffer update
    sg_update_buffer(app_data.original_sdf_ssbo, &SG_RANGE(app_data.original_sdf.buffer));
    sg_update_buffer(app_data.decomposed_sdf_ssbo, &SG_RANGE(app_data.decomposed_sdf.buffer));

    // draw
    sg_apply_pipeline(app_data.pipeline);
    sg_apply_bindings(&app_data.bindings);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(app_data.vs_params));
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(app_data.fs_params));
    sg_draw(0, 3, 1);

    // draw debug
    draw_debug_visuals(&app_data.decomp_description, vec3_xy(app_data.camera.position), app_data.camera.scale.x);
    sgl_draw();

    // draw imgui
    simgui_new_frame(&(simgui_frame_desc_t) {
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
    });
    ig_inspector_draw();
    simgui_render();

    sg_end_pass();
    sg_commit();
}

void on_event(const sapp_event* event) {
    if (simgui_handle_event(event)) return;
    al_camera_event(&app_data.camera, event);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc) {
        .width = 640,
        .height = 480,
        .window_title = "SDF Decomposition 2D",
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = delete,
        .event_cb = on_event,
        .high_dpi = true,
        .win32.console_utf8 = true,
        .win32.console_attach = true,
        .win32.console_create = true
    };
}