#include "../q3.c"

#define SOKOL_IMPL
#define SOKOL_GLCORE

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "sokol_shape.h"
#include "sokol_gl.h"
// #include "sokol_nuklear.h"

typedef uint32_t u32;
typedef float f32;

typedef struct gl_renderer_t gl_renderer_t;
struct gl_renderer_t {
    q3_render_t base;
    f32 x, y, z;
    f32 sx, sy, sz;
    f32 nx, ny, nz;
};

void glr_set_pen_color(q3_render_t *render, f32 r, f32 g, f32 b, f32 a) {
    Q3_UNUSED(render);
    Q3_UNUSED(a);
    // sgl_c3f(r, g, b);
}

void glr_set_pen_position(q3_render_t *render, f32 x, f32 y, f32 z) {
    gl_renderer_t *self = (gl_renderer_t *)render;
    self->x = x;
    self->y = y;
    self->z = z;
}

void glr_set_scale(q3_render_t *render, f32 sx, f32 sy, f32 sz) {
    gl_renderer_t *self = (gl_renderer_t *)render;
    self->sx = sx;
    self->sy = sy;
    self->sz = sz;
}

void glr_line(q3_render_t *render, f32 x, f32 y, f32 z) {
    gl_renderer_t *self = (gl_renderer_t *)render;

    sgl_begin_lines();
        sgl_v3f(self->x, self->y, self->z);
        sgl_v3f(x, y, z);
        glr_set_pen_position(render, x, y, z);
    sgl_end();
}

void glr_set_tri_normal(q3_render_t *render, f32 nx, f32 ny, f32 nz) {
    gl_renderer_t *self = (gl_renderer_t *)render;
    self->nx = nx * 0.5f + 0.5f;
    self->ny = ny * 0.5f + 0.5f;
    self->nz = nz * 0.5f + 0.5f;
}

void glr_triangle(q3_render_t *render, f32 x1, f32 y1, f32 z1, f32 x2, f32 y2, f32 z2, f32 x3, f32 y3, f32 z3) {
    Q3_UNUSED(render);
    gl_renderer_t *self = (gl_renderer_t *)render;

    sgl_begin_line_strip();
        sgl_v3f(x1, y1, z1);
        sgl_v3f(x2, y2, z2);
        sgl_v3f(x3, y3, z3);
    sgl_end();
}

void glr_point(q3_render_t *render) {
    Q3_UNUSED(render);
    gl_renderer_t *self = (gl_renderer_t *)render;
    
    sgl_begin_points();
        sgl_v3f(self->x, self->y, self->z);
    sgl_end();
}

gl_renderer_t gl_render_init(void) {
    return (gl_renderer_t) {
        .base = {
            .point            = glr_point,
            .line             = glr_line,
            .set_pen_color    = glr_set_pen_color,
            .set_pen_position = glr_set_pen_position,
            .set_tri_normal   = glr_set_tri_normal,
            .triangle         = glr_triangle,
            .set_scale        = glr_set_scale,
        },
    };
}

sg_pass_action pass_action;
sgl_pipeline pip_3d;
gl_renderer_t renderer;
f32 acc = 0.f;
f32 dt = 1.f / 60.f;
q3_scene_t scene = {0};

// camera
q3_vec3_t eye = { 0.f, 5.f, 20.f, };
q3_vec3_t target = { 0.f, 0.f, 0.f, };

void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
    });

    sgl_setup(&(sgl_desc_t){0});

    renderer = gl_render_init();

    pip_3d = sgl_make_pipeline(&(sg_pipeline_desc){
        .cull_mode = SG_CULLMODE_FRONT,
        .face_winding = SG_FACEWINDING_CCW,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
    });

    pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };

    q3_scene_init(&scene, &(q3_scene_desc_t){
        .dt = dt, 
    });

    q3_bodydef_t def = q3_bodydef_default();
    // def.body_type = Q3_BODY_DYNAMIC;

    q3_body_t *body = q3_scene_create_body(&scene, &def);

    q3_boxdef_t boxdef = q3_boxdef_init();
    boxdef.m_restitution = 0;

    q3_transform_t tx = {0};
    q3_tidentity(&tx);

    q3_boxdef_set(&boxdef, &tx, (q3_vec3_t){ 50.f, 1.f, 50.f });
    q3_body_add_box(body, &boxdef);
}

void frame(void) {
    static bool once = true;
    acc += dt;
    if (acc > 1.f) {
        once = false;
        acc = 0;
        
        q3_bodydef_t def = q3_bodydef_default();
        def.body_type = Q3_DYNAMIC_BODY;
        def.position = (q3_vec3_t){ 0, 10, 0 };
        def.axis = (q3_vec3_t){ 
            q3_random_float(-1.f, 1.f),
            q3_random_float(-1.f, 1.f),
            q3_random_float(-1.f, 1.f),
        };
        def.angle = Q3_PI * q3_random_float(-1.f, 1.f);
        def.angular_velocity = (q3_vec3_t){ 
            q3_random_float(1.f, 3.f),
            q3_random_float(1.f, 3.f),
            q3_random_float(1.f, 3.f),
        };
        def.angular_velocity = q3_v3mulf(
            def.angular_velocity, 
            q3_sign(q3_random_float(-1.f, 1.f))
        );
        def.linear_velocity = (q3_vec3_t){ 
            q3_random_float(1.f, 3.f),
            q3_random_float(1.f, 3.f),
            q3_random_float(1.f, 3.f),
        };
        def.linear_velocity = q3_v3mulf(
            def.linear_velocity, 
            q3_sign(q3_random_float(-1.f, 1.f))
        );

        q3_body_t *body = q3_scene_create_body(&scene, &def);

        q3_boxdef_t boxdef = q3_boxdef_init();
        q3_transform_t tx = {0};
        q3_tidentity(&tx);

        q3_boxdef_set(&boxdef, &tx, (q3_vec3_t){ 1.f, 1.f, 1.f });
        q3_body_add_box(body, &boxdef);
    }

    q3_scene_step(&scene);

    f32 dw = sapp_widthf();
    f32 dh = sapp_heightf();
    f32 aspect_ratio = dw / dh;

    sgl_viewportf(0.f, 0.f, dw, dh, false);

    sgl_matrix_mode_projection();
        sgl_load_identity();
        sgl_perspective(45.f, aspect_ratio, 0.1f, 10000.f);
    sgl_matrix_mode_modelview();
        sgl_load_identity();
        sgl_lookat(
            eye.x,    eye.y,    eye.z,
            target.x, target.y, target.z,
            0.f,      1.f,      0.f
        );


    q3_scene_render(&scene, (q3_render_t*)&renderer);

    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = sglue_swapchain() });
        sgl_draw();
    sg_end_pass();
    sg_commit();
}

void event(const sapp_event *e) {
    f32 increment = .8f;
    switch (e->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            switch (e->key_code) {
                case SAPP_KEYCODE_Z:
                    q3_scene_step(&scene);
                    break;

                case SAPP_KEYCODE_ESCAPE:
                    sapp_quit();
                    break;
                case SAPP_KEYCODE_W:
                    eye.z    -= increment;
                    target.z -= increment;
                    break;
                case SAPP_KEYCODE_A:
                    eye.x    -= increment;
                    target.x -= increment;
                    break;
                case SAPP_KEYCODE_S:
                    eye.z    += increment;
                    target.z += increment;
                    break;
                case SAPP_KEYCODE_D:
                    eye.x    += increment;
                    target.x += increment;
                    break;
                case SAPP_KEYCODE_Q:
                    eye.y    -= increment;
                    target.y -= increment;
                    break;
                case SAPP_KEYCODE_E:
                    eye.y    += increment;
                    target.y += increment;
                    break;
                default: break;
            }
            break;
        default: break;
    }
}

void cleanup(void) {
    q3_scene_remove_all_bodies(&scene);
    sgl_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = event,
        .cleanup_cb = cleanup,
        .width = 512,
        .height = 512,
        .sample_count = 4,
        .window_title = "cqu3e",
        .icon.sokol_default = true,
        .win32.console_attach = true,
    };
}
