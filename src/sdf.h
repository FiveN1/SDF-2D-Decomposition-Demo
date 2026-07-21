
#define MAX_SDF_PRIMITIVES 256

typedef enum sdf_shape_type {
    SDF_TYPE_NONE = 0,
    SDF_TYPE_CIRCLE,
    SDF_TYPE_BOX,
    SDF_TYPE_EQ_TRIANGLE,
    SDF_TYPE_STAR,
    SDF_TYPE_HORSESHOE,
    _SDF_TYPE_COUNT
} sdf_shape_type;

typedef struct sdf_shape_id { uint16_t id; } sdf_shape_id;

typedef struct sdf_shape {
    vec2_t position;
    float angle;
    sdf_shape_type type;
    vec2_t size;
    uint32_t _pad8[2]; // add extra padding, since glsl struct has to by 16 byte aligned
} sdf_shape;

typedef struct sdf_buffer { // arena-like buffer
    sdf_shape buffer[MAX_SDF_PRIMITIVES + 1]; // +1 for invalid shape at the very end of buffer, i.e at MAX_SDF_PRIMITIVES
    uint16_t free_index;
} sdf_buffer;

sdf_shape_id sdf_add_shape(sdf_buffer* buffer, sdf_shape* shape) {
    sdf_shape_id id = { .id = MAX_SDF_PRIMITIVES }; // initilize with invalid value
    if (buffer->free_index >= MAX_SDF_PRIMITIVES) {
        printf("sdf_add_shape(): SDF buffer full!\n"); // error log
        return id;
    }
    if (shape->size.x == 0.0f && shape->size.y == 0.0f) {
        shape->size = vec2f(0.3f);
    }
    buffer->buffer[buffer->free_index] = *shape;
    id.id = buffer->free_index++;
    return id;
}

void sdf_pop_shape(sdf_buffer* buffer) {
    if (buffer->free_index == 0) {
        printf("sdf_pop_shape(): SDF buffer empty!\n"); // error log
        return;
    }
    buffer->free_index--;
}

sdf_shape* sdf_get_shape(sdf_buffer* buffer, sdf_shape_id id) {
    if (id.id > buffer->free_index) return &buffer->buffer[MAX_SDF_PRIMITIVES];
    return &buffer->buffer[id.id];
}

//
// SDF TRANSFORMS
//

vec2_t sdf_rotate(vec2_t p, float angle) {
    float cosA = vecmath_cos(-angle);
    float sinA = vecmath_sin(-angle);
    return mat22_mul_vec2(mat22(vec2(cosA, -sinA), vec2(sinA, cosA)), p);
}

vec2_t sdf_transform(vec2_t p, vec2_t position, float angle) {
    return sdf_rotate(vec2_sub(p, position), angle);
}

//
// SDF SHAPE FUNCTIONS
//

float de_circle(vec2_t p, float r) {
    return vec2_length(p) - r;
}

float de_box(vec2_t p, vec2_t b) {
    vec2_t d = vec2_sub(vec2_abs(p), b);
    return vec2_length(vec2_max(d, vec2f(0.0))) + vecmath_min(vecmath_max(d.x, d.y), 0.0);
}

float de_eq_triangle(vec2_t p, float r) {
    const float k = vecmath_sqrt(3.0f);
    p.x = vecmath_abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0f) p = vec2_divf(vec2(p.x - k * p.y, -k * p.x - p.y), 2.0f);
    p.x -= vecmath_clamp(p.x, -2.0f * r, 0.0f);
    return -vec2_length(p) * vecmath_sign(p.y);
}

float de_pentagram(vec2_t p, float r) {
    const float k1x = 0.809016994;
    const float k2x = 0.309016994;
    const float k1y = 0.587785252;
    const float k2y = 0.951056516;
    const float k1z = 0.726542528;
    const vec2_t  v1 = vec2(k1x, -k1y);
    const vec2_t  v2 = vec2(-k1x, -k1y);
    const vec2_t  v3 = vec2(k2x, -k2y);
    p.x = vecmath_abs(p.x);
    p = vec2_sub(p, vec2_fmul(2.0f * vecmath_max(vec2_dot(v1, p), 0.0), v1));
    p = vec2_sub(p, vec2_fmul(2.0f * vecmath_max(vec2_dot(v2, p), 0.0), v2));
    p.x = vecmath_abs(p.x);
    p.y -= r;
    return vec2_length(vec2_sub(p, vec2_mulf(v3, vecmath_clamp(vec2_dot(p, v3), 0.0, k1z * r)))) * vecmath_sign(p.y * v3.x - p.x * v3.y);
}

float de_horseshoe(vec2_t p, vec2_t c, float r, vec2_t w) {
    p.x = vecmath_abs(p.x);
    float l = vec2_length(p);
    p = mat22_mul_vec2(mat22(vec2(-c.x, c.y), vec2(c.y, c.x)), p);
    p = vec2((p.y > 0.0 || p.x > 0.0) ? p.x : l * vecmath_sign(-c.x),
        (p.x > 0.0) ? p.y : l);
    p = vec2_sub(vec2(p.x, vecmath_abs(p.y - r)), w);
    return vec2_length(vec2_max(p, vec2f(0.0))) + vecmath_min(0.0, vecmath_max(p.x, p.y));
}

//
// SDF VALUE
//

float sdf_get_shape_dist(sdf_buffer* buffer, sdf_shape_id id, vec2_t p) {
    sdf_shape* shape = sdf_get_shape(buffer, id);
    p = sdf_transform(p, shape->position, shape->angle);
    float d = 256.0f;
    switch (shape->type) {
        case SDF_TYPE_NONE: 
            break;
        case SDF_TYPE_CIRCLE: 
            d = de_circle(p, shape->size.x);
            break;
        case SDF_TYPE_BOX: 
            d = de_box(p, shape->size);
            break;
        case SDF_TYPE_EQ_TRIANGLE: 
            d = de_eq_triangle(p, shape->size.x);
            break;
        case SDF_TYPE_STAR: 
            d = de_pentagram(p, shape->size.x);
            break;
        case SDF_TYPE_HORSESHOE:
            d = de_horseshoe(p, vec2(vecmath_sin(0.2f), vecmath_cos(0.2f)), 0.5f, vec2(0.3f, 0.1f));
            break;
    }
    return d;
}

float sdf_get_scene_dist(sdf_buffer* buffer, vec2_t p) {
    float d = 256.0f;
    for (uint16_t i = 0; i < buffer->free_index; i++) {
        float d_shape = sdf_get_shape_dist(buffer, (sdf_shape_id) { .id = i }, p);
        d = vecmath_min(d, d_shape);
    }
    return d;
}

//
// DECOMPOSITION
//

typedef enum gradient_descent_type {
    SGD = 0,
    GD_MOMENTUM,
    NELDER_MEAD
} gradient_descent_type;

typedef struct sdf_decomp_desc {
    int iteration_count;
    int grid_resolution;
    float grid_size;
    int gradient_descent_method;
} sdf_decomp_desc;

static struct sdf_decomposition_state {
    sdf_buffer* original_buffer;
    sdf_buffer* decomposed_buffer;
    bool running;
    struct {
        bool draw_grid;
        bool draw_grad_descnet;
    } debug_view;

} sdf_decomposition_state;

void sdf_decomposition_set_state(sdf_buffer* original_buffer, sdf_buffer* decomposed_buffer) {
    sdf_decomposition_state.original_buffer = original_buffer;
    sdf_decomposition_state.decomposed_buffer = decomposed_buffer;
    sdf_decomposition_state.running = true;
    sdf_decomposition_state.debug_view.draw_grid = false;
    sdf_decomposition_state.debug_view.draw_grad_descnet = false;
}

float sdf_decomposition_scene_dist(vec2_t p) {
    float d = sdf_get_scene_dist(sdf_decomposition_state.original_buffer, p);
    float d_subtracted = sdf_get_scene_dist(sdf_decomposition_state.decomposed_buffer, p);
    return vecmath_max(d, -d_subtracted);
}

// get gradient using central differences
vec2_t sdf_get_gradient(vec2_t p) {
    const float epsilon = 0.001f;
    float d = sdf_decomposition_scene_dist(p);
    vec2_t ep_x = vec2_add(p, vec2(epsilon, 0.0f));
    vec2_t ep_y = vec2_add(p, vec2(0.0f, epsilon));
    float x = d - sdf_decomposition_scene_dist(ep_x); // numerical differentiation along x
    float y = d - sdf_decomposition_scene_dist(ep_y); // numerical differentiation along y
    return vec2(x, y);
}

vec2_t sdf_descent_gradient_with_momentum(vec2_t p) {
    const int step_count = 32;
    float acceleration_magnitude = 0.001f;
    float damping = 0.8f;
    vec2_t velocity = { 0.0f, 0.0f };

    for (int i = 0; i < step_count; i++) {
        vec2_t gradient = vec2_normalize(sdf_get_gradient(p));
        vec2_t acceleration = vec2_mulf(gradient, acceleration_magnitude);
        velocity = vec2_add(velocity, acceleration);
        velocity = vec2_mulf(velocity, damping);
        p = vec2_add(velocity, p);
    }
    return p;
}

vec3_t sdf_get_global_minimum(int grid_resolution, float grid_size) {
    vec2_t lowest_point = { 0 };
    float lowest_value = 128.0f;

    for (int y = 0; y < grid_resolution; y++) {
        for (int x = 0; x < grid_resolution; x++) {
            // get point in grid
            vec2_t point = vec2((float)x, (float)y);
            point = vec2_divf(point, grid_resolution - 1);
            point = vec2_mulf(point, grid_size);
            point = vec2_subf(point, grid_size * 0.5f);
            // descent to local minimum
            point = sdf_descent_gradient_with_momentum(point);
            // compare
            float d = sdf_decomposition_scene_dist(point);
            if (d < lowest_value) {
                lowest_value = d;
                lowest_point = point;
            }
        }
    }
    return vec3(lowest_point.x, lowest_point.y, lowest_value);
}

void sdf_decompose(sdf_decomp_desc* desc) {
    // reset output buffer
    sdf_decomposition_state.decomposed_buffer->free_index = 0;
    
    for (int n = 0; n < desc->iteration_count; n++) {
        // get lowest point in SDF
        vec3_t global_minimum = sdf_get_global_minimum(desc->grid_resolution, desc->grid_size);
        // add n-sphere
        sdf_add_shape(sdf_decomposition_state.decomposed_buffer, &(sdf_shape){
            .type = SDF_TYPE_CIRCLE,
            .position = vec3_xy(global_minimum),
            .size = vec2f(vecmath_abs(global_minimum.z))
        });
    }
}

void sdf_decompose_step(sdf_decomp_desc* desc) {

    if (!sdf_decomposition_state.running) {
        return;
    }

    if (sdf_decomposition_state.decomposed_buffer->free_index >= desc->iteration_count) {
        sdf_decomposition_state.running = false; // reached end
        return;
    }

    static float time = 0.0f;
    time += (float)sapp_frame_duration();

    if (time > 0.12f) {
        // get lowest point in SDF
        vec3_t global_minimum = sdf_get_global_minimum(desc->grid_resolution, desc->grid_size);
        // add n-sphere
        sdf_add_shape(sdf_decomposition_state.decomposed_buffer, &(sdf_shape){
            .type = SDF_TYPE_CIRCLE,
            .position = vec3_xy(global_minimum),
            .size = vec2f(vecmath_abs(global_minimum.z))
        });
    }
}

//
// DEBUG VISUALS
//

vec2_t debug_draw_gradient_with_momentum(vec2_t p) {
    const int step_count = 32;
    float acceleration_magnitude = 0.001f;
    float damping = 0.8f;
    vec2_t velocity = { 0.0f, 0.0f };

    sgl_begin_lines();
    for (int i = 0; i < step_count; i++) {
        float c = 0.8f * ((float)(i) / (float)step_count);
        sgl_c3f(c, c, c);
        sgl_v2f(p.x, p.y);

        vec2_t gradient = vec2_normalize(sdf_get_gradient(p));
        vec2_t acceleration = vec2_mulf(gradient, acceleration_magnitude);
        velocity = vec2_add(velocity, acceleration);
        velocity = vec2_mulf(velocity, damping);
        p = vec2_add(velocity, p);

        sgl_v2f(p.x, p.y);
    }
    sgl_end();
    return p;
}

void debug_draw_grid(int grid_resolution, float grid_size) {

    if (!sdf_decomposition_state.debug_view.draw_grid) return;

    vec2_t lowest_point = { 0 };
    float lowest_value = 128.0f;

    for (int y = 0; y < grid_resolution; y++) {
        for (int x = 0; x < grid_resolution; x++) {
            // get point in grid
            vec2_t point = vec2((float)x, (float)y);
            point = vec2_divf(point, grid_resolution - 1);
            point = vec2_mulf(point, grid_size);
            point = vec2_subf(point, grid_size * 0.5f);

            sgl_begin_points();
            sgl_c3f(0.0f, 0.0f, 0.0f);
            sgl_point_size(10.0f);
            sgl_v2f(point.x, point.y);
            sgl_end();

            if (!sdf_decomposition_state.debug_view.draw_grad_descnet) continue;

            point = debug_draw_gradient_with_momentum(point);

            float d = sdf_decomposition_scene_dist(point);
            if (d < lowest_value) {
                lowest_value = d;
                lowest_point = point;
            }
        }
    }

    if (sdf_decomposition_state.debug_view.draw_grad_descnet) {
        sgl_begin_points();
        sgl_c3f(1.0f, 1.0f, 0.0f);
        sgl_point_size(10.0f);
        sgl_v2f(lowest_point.x, lowest_point.y);
        sgl_end();
    }
}

void draw_debug_visuals(sdf_decomp_desc* desc, vec2_t camera_position, float camera_scale) {
    sgl_push_matrix();
    sgl_scale(1.0f / camera_scale / (sapp_widthf() / sapp_heightf()), 1.0f / camera_scale, 1.0f);
    sgl_translate(camera_position.x, camera_position.y, 0.0f);

    debug_draw_grid(desc->grid_resolution, desc->grid_size);

    sgl_pop_matrix();
}