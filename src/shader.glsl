
@ctype vec2 vec2_t
@ctype vec3 vec3_t

//
// VERTEX SHADER
//

@vs vs

layout(binding = 0) uniform vs_params{
    float aspect;
};

in vec4 position;
out vec2 pos;

void main() {
    gl_Position = position;
    pos.x = position.x * aspect;
    pos.y = position.y;
}
@end

//
// FRAGMENT SHADER
//

@fs fs

struct sdf_shape {
    vec2 position;
    float angle;
    int type;
    vec2 size;
    int _pad0;
    int _pad1;
};

layout(std140, binding = 0) readonly buffer original_sdf_buffer {
    sdf_shape original_sdf_shapes[];
};

layout(std140, binding = 1) readonly buffer decomposed_sdf_buffer {
    sdf_shape decomposed_sdf_shapes[];
};

layout(binding = 1) uniform fs_params{
    
    int original_sdf_count;
    int decomposed_sdf_count;

    vec2 camera_position;
    float camera_zoom;
};

in vec2 pos;
out vec4 frag_color;

//
// SDF TRANSFORM
//

vec2 sdf_rotate(in vec2 p, float angle) {
    float cosA = cos(angle);
    float sinA = sin(angle);
    return mat2(cosA, -sinA, sinA, cosA) * p; // Rotate `p` by `angle`
}

vec2 sdf_transform(in vec2 p, in vec2 position, in float angle) {
    return sdf_rotate((p * camera_zoom - position - camera_position), angle);
}

//
// SDF SHAPE FUNCTIONS
//

float de_circle(vec2 p, in float r) {
    return length(p) - r;
}

float de_box(in vec2 p, in vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float de_eq_triangle(in vec2 p, in float r) {
    const float k = sqrt(3.0);
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0) p = vec2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

float de_pentagram(in vec2 p, in float r) {
    const float k1x = 0.809016994;
    const float k2x = 0.309016994;
    const float k1y = 0.587785252;
    const float k2y = 0.951056516;
    const float k1z = 0.726542528;
    const vec2  v1 = vec2(k1x, -k1y);
    const vec2  v2 = vec2(-k1x, -k1y);
    const vec2  v3 = vec2(k2x, -k2y);
    p.x = abs(p.x);
    p -= 2.0 * max(dot(v1, p), 0.0) * v1;
    p -= 2.0 * max(dot(v2, p), 0.0) * v2;
    p.x = abs(p.x);
    p.y -= r;
    return length(p - v3 * clamp(dot(p, v3), 0.0, k1z * r))
        * sign(p.y * v3.x - p.x * v3.y);
}

float de_horseshoe(in vec2 p, in vec2 c, in float r, in vec2 w) {
    p.x = abs(p.x);
    float l = length(p);
    p = mat2(-c.x, c.y, c.y, c.x) * p;
    p = vec2((p.y > 0.0 || p.x > 0.0) ? p.x : l * sign(-c.x),
        (p.x > 0.0) ? p.y : l);
    p = vec2(p.x, abs(p.y - r)) - w;
    return length(max(p, 0.0)) + min(0.0, max(p.x, p.y));
}

//
// SDF 
//

float sdf_get_shape_dist(in vec2 p, in sdf_shape shape) {
    p = sdf_transform(p, shape.position, shape.angle);
    float d = 256.0f;
    switch (shape.type) {
    case 0:
        break;
    case 1:
        d = de_circle(p, shape.size.x);
        break;
    case 2:
        d = de_box(p, shape.size);
        break;
    case 3:
        d = de_eq_triangle(p, shape.size.x);
        break;
    case 4:
        d = de_pentagram(p, shape.size.x);
        break;
    case 5:
        d = de_horseshoe(p, vec2(sin(0.2f), cos(0.2f)), 0.5f, vec2(0.3f, 0.1f));
        break;
    }
    return d;
}

float de_scene(in vec2 p) {
    float d = 256.0f;
    for (int i = 0; i < original_sdf_count; i++) {
        sdf_shape shape = original_sdf_shapes[i];
        float d_shape = sdf_get_shape_dist(p, shape);
        d = min(d, d_shape);
    }

    float d_decomposed = 256.0f;
    for (int i = 0; i < decomposed_sdf_count; i++) {
        sdf_shape shape = decomposed_sdf_shapes[i];
        float d_shape = sdf_get_shape_dist(p, shape);
        d_decomposed = min(d_decomposed, d_shape);
    }

    return max(d, -d_decomposed);
}


/*
float smin(float a, float b, float k) {
    k *= 1.0;
    float r = exp2(-a / k) + exp2(-b / k);
    return -k * log2(r);
}
*/


// circular

/*
float smin(float a, float b, float k)
{
    k *= 1.0 / (1.0 - sqrt(0.5));
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - k * 0.5 * (1.0 + h - sqrt(1.0 - h * (h - 2.0)));
}


float opXor(float a, float b)
{
    return max(min(a, b), -max(a, b));
}
*/

/*
float de_scene(in vec2 p) {

    
    if (1 + original_shape_sdf_count + decomposition_sdf_count > shapes_count) return 0.0f; // overflow

    float d0 = 128.0f;
    for (int i = 1; i < 1 + original_shape_sdf_count; i++) {
        vec2 trans_p = rotate_sdf((p * cam_zoom - shapes[i].position - cam_position), shapes[i].angle);
        float ds = de_shape(trans_p, i);
        d0 = min(d0, ds);
    }
    float d1 = 128.0f;
    for (int i = 1 + original_shape_sdf_count; i < 1 + original_shape_sdf_count + decomposition_sdf_count; i++) {
        vec2 trans_p = rotate_sdf((p * cam_zoom - shapes[i].position - cam_position), shapes[i].angle);
        float ds = de_shape(trans_p, i);
        d1 = min(d1, ds);
        //float s = 1.0f / (float(decomposition_sdf_count) * 6.0f);
        //d1 = smin(d1, ds, s);
    }

    //float d = max(d0, -d1); // d1 is negative
    float d = opXor(d0, d1);
    //float d = d1;
    

    return 0.0f; // d1 is negative
}
*/


//
// SHADING
//

vec3 hex_to_rgb(int hex_value) {
    vec3 color;
    color.r = ((hex_value >> 16) & 0xFF) / 255.0;  // Extract the RR byte
    color.g = ((hex_value >> 8) & 0xFF) / 255.0;   // Extract the GG byte
    color.b = ((hex_value) & 0xFF) / 255.0;        // Extract the BB byte
    return color;
}

vec3 linear_color_gradient(float gradient_position) {

    // managua
    vec3 color_spectrum[10] = {
        hex_to_rgb(0xffcf67),
        hex_to_rgb(0xdd9954),
        hex_to_rgb(0xba6b44),
        hex_to_rgb(0x93453b),
        hex_to_rgb(0x68293c),
        hex_to_rgb(0x4f315d),
        hex_to_rgb(0x505693),
        hex_to_rgb(0x5d7fbd),
        hex_to_rgb(0x6fb0de),
        hex_to_rgb(0x91d8f0)
    };
    vec3 color_spectrum_[10] = {
        hex_to_rgb(0x65144b),
        hex_to_rgb(0xa03b85),
        hex_to_rgb(0xc76fac),
        hex_to_rgb(0xe3aed0),
        hex_to_rgb(0xf5e3ed),
        hex_to_rgb(0xf0f2e3),
        hex_to_rgb(0xc3daa0),
        hex_to_rgb(0x7ea854),
        hex_to_rgb(0x467a39),
        hex_to_rgb(0x244b23)
    };
    int color_count = 10;
    int color_segments = (color_count - 1);
    float segment_distance = 1.0f / color_segments;
    int base_segment = int(floor(gradient_position * color_segments));
    int next_segment = base_segment + 1;
    float base_position = float(base_segment) * segment_distance;
    float next_weight = (float(gradient_position) - base_position) * color_segments;
    float base_weight = 1.0f - next_weight;
    return color_spectrum[base_segment] * base_weight + color_spectrum[next_segment] * next_weight;
}

float get_shadow(float d, float shadow_max, float shadow_opacity) {
    float shadow = clamp(abs(d), 0.0f, shadow_max) * (1.0f / shadow_max) * shadow_opacity + (1.0f - shadow_opacity);
    return shadow;
}

void main() {
    float d = de_scene(pos);
    vec3 color = vec3(1.0f, 1.0f, 1.0f);
    float iso = 0.001f;
    float lind = (clamp(sqrt(abs(d)) * sign(d) + sin(d * 512.0f) * 0.1f, -1.0f, 1.0f) + 1.0f) / 2.0f;
    if (d > iso || d < -iso) {
        color = linear_color_gradient(lind);
        float wave = 1.0f - (sin(d * 512.0f) + 1.0f) * 0.5f * 0.2f;
        float shadow = get_shadow(d, 0.05f, 0.5f);
        if (d > iso) {
            float blend = get_shadow(d, 0.25f, 1.0f);
            vec3 bg = vec3(0.972, 0.976, 0.988);
            color = bg * wave * (1.0f - blend) + bg * blend;
            color *= shadow;
        }
        if (d < -iso) {
            color = vec3(1.0f, 0.49f, 0.59f) * wave * shadow;
        }
    }
    frag_color = vec4(color, 1.0);
}
@end

@program shader vs fs