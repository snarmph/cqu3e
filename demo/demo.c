#include "../q3.c"

#define SOKOL_IMPL
#ifdef _WIN32
#define SOKOL_D3D11
#else
#define SOKOL_GLCORE
#endif

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "sokol_gl.h"
#include "microui.c"
#include "microui_atlas.inl"

typedef uint32_t u32;
typedef float f32;

void drop_boxes_init(void);
void drop_boxes_update(void);
void drop_boxes_draw(q3_render_t *drawer);
void drop_boxes_cleanup(void);

// TODO there's still something wrong with the raycast!
#if 0
void ray_push_init(void);
void ray_push_update(void);
void ray_push_draw(q3_render_t *drawer);
void ray_push_cleanup(void);
#endif

void test_init(void);
void test_update(void);
void test_draw(q3_render_t *drawer);
void test_cleanup(void);

struct {
    char name[16];
    void (*init)(void);
    void (*update)(void);
    void (*draw)(q3_render_t*);
    void (*cleanup)(void);
} demos[] = {
    { "drop boxes", drop_boxes_init, drop_boxes_update, drop_boxes_draw, drop_boxes_cleanup },
#if 0
    { "ray push",   ray_push_init,   ray_push_update,   ray_push_draw,   ray_push_cleanup },
#endif
    { "test",       test_init,       test_update,       test_draw,       test_cleanup },
};

typedef struct gl_renderer_t gl_renderer_t;
struct gl_renderer_t {
    q3_render_t base;
    f32 x, y, z;
    f32 sx, sy, sz;
    f32 nx, ny, nz;
};

void glr_set_pen_color(q3_render_t *render, f32 r, f32 g, f32 b, f32 a) {
}

void glr_set_pen_position(q3_render_t *render, f32 x, f32 y, f32 z) {
}

void glr_set_scale(q3_render_t *render, f32 sx, f32 sy, f32 sz) {
}

void glr_line(q3_render_t *render, f32 x, f32 y, f32 z) {
}

void glr_set_tri_normal(q3_render_t *render, f32 nx, f32 ny, f32 nz) {
    gl_renderer_t *self = (gl_renderer_t *)render;
    self->nx = nx * 0.5f + 0.5f;
    self->ny = ny * 0.5f + 0.5f;
    self->nz = nz * 0.5f + 0.5f;
}

void glr_triangle(q3_render_t *render, f32 x1, f32 y1, f32 z1, f32 x2, f32 y2, f32 z2, f32 x3, f32 y3, f32 z3) {
    gl_renderer_t *self = (gl_renderer_t *)render;

    sgl_begin_triangles();
        sgl_c3f(self->nx, self->ny, self->nz);
        sgl_v3f(x1, y1, z1);
        sgl_v3f(x2, y2, z2);
        sgl_v3f(x3, y3, z3);
    sgl_end();
}

void glr_point(q3_render_t *render) {
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
uint64_t laptime = 0;
f32 dt = 1.f / 60.f;
q3_scene_t scene = {0};
int paused = false;
int single_step = false;
int enable_sleep = true;
int enable_friction = true;
int velocity_iterations = 20;
int demo_count = sizeof(demos) / sizeof(*demos);
int cur_demo = 0;
int last_demo = 0;

// microui state
mu_Context mu_ctx;
void r_init(void);
void r_begin(int disp_width, int disp_height);
void r_end(void);
void r_draw(void);
void r_push_quad(mu_Rect dst, mu_Rect src, mu_Color color);
void r_draw_rect(mu_Rect rect, mu_Color color);
void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color);
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
int r_get_text_width(const char* text, int len);
int r_get_text_height(void);
void r_set_clip_rect(mu_Rect rect);
int text_width_cb(mu_Font font, const char* text, int len) {
    (void)font;
    if (len == -1) {
        len = (int) strlen(text);
    }
    return r_get_text_width(text, len);
}

int text_height_cb(mu_Font font) {
    (void)font;
    return r_get_text_height();
}


// camera
q3_vec3_t eye = { 0.f, 5.f, 20.f, };
q3_vec3_t target = { 0.f, 0.f, 0.f, };

void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
    });

    sgl_setup(&(sgl_desc_t){0});
    stm_setup();

    renderer = gl_render_init();

    pip_3d = sgl_context_make_pipeline(sgl_default_context(), &(sg_pipeline_desc){
        .cull_mode = SG_CULLMODE_BACK,
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

    laptime = stm_now();

    q3_scene_init(&scene, &(q3_scene_desc_t){
        .dt = dt, 
    });

    demos[cur_demo].init();

    r_init();
    mu_init(&mu_ctx);
    mu_ctx.text_width = text_width_cb;
    mu_ctx.text_height = text_height_cb;
    mu_ctx.style->colors[MU_COLOR_WINDOWBG].a = 200;
}

void update_scene(float delta) {
	// The time accumulator is used to allow the application to render at
	// a frequency different from the constant frequency the physics 
    // simulation is running at (default 60Hz).
	static f32 accumulator = 0;
	accumulator += delta;

	accumulator = q3_clamp01(accumulator);
	while (accumulator >= dt) {
		if (!paused) {
            q3_scene_step(&scene);
            demos[cur_demo].update();
		}
		else if (single_step) {
            single_step = false;
            q3_scene_step(&scene);
            demos[cur_demo].update();
		}

		accumulator -= dt;
	}
}

void frame(void) {
    if (cur_demo != last_demo) {
        demos[last_demo].cleanup();
        demos[cur_demo].init();
        last_demo = cur_demo;
    }

    // ** UPDATE ***************************************

    float delta = (float)stm_sec(stm_laptime(&laptime));

    q3_scene_set_allow_sleep(&scene, enable_sleep);
    q3_scene_set_enable_friction(&scene, enable_friction);
    q3_scene_set_iterations(&scene, velocity_iterations);

    update_scene(delta);

    // ** RENDER ***************************************

    f32 dw = sapp_widthf();
    f32 dh = sapp_heightf();
    f32 aspect_ratio = dw / dh;

    mu_begin(&mu_ctx);
    {
        if (mu_begin_window_ex(
                &mu_ctx, 
                "q3_scene settings",
                mu_rect(dw - 330, 30, 300, 225),
                MU_OPT_NOCLOSE
            )
        ) {
            mu_layout_row(&mu_ctx, 3, NULL, 0);
            for (int i = 0; i < demo_count; ++i) {
                if (mu_button(&mu_ctx, demos[i].name)) {
                    cur_demo = i;
                }
            }
            mu_layout_row(&mu_ctx, 1, NULL, 0);
            
            mu_checkbox(&mu_ctx, "pause", &paused);
            if (paused) {
                single_step = mu_button(&mu_ctx, "single step");
            }
            mu_checkbox(&mu_ctx, "sleeping", &enable_sleep);
            mu_checkbox(&mu_ctx, "friction", &enable_friction);
            f32 vel_iterations_float = (f32)velocity_iterations;
            if (mu_slider_ex(&mu_ctx, &vel_iterations_float, 1.f, 50.f, 1.f, "%.0f", 0)) {
                velocity_iterations = (int)roundf(vel_iterations_float);
            }

            mu_end_window(&mu_ctx);
        }
    }
    mu_end(&mu_ctx);

    sgl_set_context(SGL_DEFAULT_CONTEXT);
    sgl_defaults();
    sgl_load_pipeline(pip_3d);

    sgl_viewportf(0.f, 0.f, dw, dh, false);

    sgl_matrix_mode_projection();
        sgl_load_identity();
        sgl_perspective(sgl_rad(45.f), aspect_ratio, 0.1f, 10000.f);
    sgl_matrix_mode_modelview();
        sgl_load_identity();
        sgl_lookat(
            eye.x,    eye.y,    eye.z,
            target.x, target.y, target.z,
            0.f,      1.f,      0.f
     );

    q3_scene_render(&scene, (q3_render_t*)&renderer);

    r_begin(dw, dh);
    mu_Command *cmd = NULL;
    while (mu_next_command(&mu_ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
        }
    }
    r_end();

    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = sglue_swapchain() });
        sgl_context_draw(SGL_DEFAULT_CONTEXT);
        r_draw();
    sg_end_pass();
    sg_commit();
}

const char key_map[512] = {
    [SAPP_KEYCODE_LEFT_SHIFT]       = MU_KEY_SHIFT,
    [SAPP_KEYCODE_RIGHT_SHIFT]      = MU_KEY_SHIFT,
    [SAPP_KEYCODE_LEFT_CONTROL]     = MU_KEY_CTRL,
    [SAPP_KEYCODE_RIGHT_CONTROL]    = MU_KEY_CTRL,
    [SAPP_KEYCODE_LEFT_ALT]         = MU_KEY_ALT,
    [SAPP_KEYCODE_RIGHT_ALT]        = MU_KEY_ALT,
    [SAPP_KEYCODE_ENTER]            = MU_KEY_RETURN,
    [SAPP_KEYCODE_BACKSPACE]        = MU_KEY_BACKSPACE,
};

void event(const sapp_event *ev) {
    f32 increment = .8f;
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            mu_input_mousedown(&mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            mu_input_mouseup(&mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            mu_input_mousemove(&mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            mu_input_scroll(&mu_ctx, 0, (int)ev->scroll_y);
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            mu_input_keydown(&mu_ctx, key_map[ev->key_code & 511]);
            switch (ev->key_code) {
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
        case SAPP_EVENTTYPE_KEY_UP:
            mu_input_keyup(&mu_ctx, key_map[ev->key_code & 511]);
            break;
        case SAPP_EVENTTYPE_CHAR:
        {
            // don't input Backspace as character (required to make Backspace work in text input fields)
            if (ev->char_code == 127) { break; }
            char txt[2] = { (char)(ev->char_code & 255), 0 };
            mu_input_text(&mu_ctx, txt);
            break;
        }
        default: break;
    }
}

void cleanup(void) {
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

// =========================================================
// TEST DROP BOXES
// =========================================================

struct {
    f32 acc;
} dp = {0};

void drop_boxes_init(void) {
    // floor body
    q3_bodydef_t def = q3_bodydef_default();
    q3_body_t *body = q3_scene_create_body(&scene, &def);
    q3_boxdef_t boxdef = q3_boxdef_init();
    boxdef.m_restitution = 0;
    q3_transform_t tx = {0};
    q3_tidentity(&tx);
    q3_boxdef_set(&boxdef, &tx, (q3_vec3_t){ 50.f, 1.f, 50.f });
    q3_body_add_box(body, &boxdef);
}

void drop_boxes_update(void) {
    dp.acc += dt;
    if (dp.acc > 1.f) {
        dp.acc = 0;
        
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
}

void drop_boxes_draw(q3_render_t *drawer) {
}

void drop_boxes_cleanup(void) {
    q3_scene_remove_all_bodies(&scene);
}

// =========================================================
// TEST RAY PUSH
// =========================================================

typedef struct raycast_t raycast_t;
struct raycast_t {
    q3_query_callback_i base;
    q3_raycast_data_t data;
    f32 tfinal;
    q3_vec3_t nfinal;
    q3_body_t *impact_body;
};

bool rc_report_shape(q3_query_callback_i *base, q3_box_t *shape) {
    raycast_t *self = (raycast_t *)base;
    if (self->data.toi < self->tfinal) {
        self->tfinal = self->data.toi;
        self->nfinal = self->data.normal;
        self->impact_body = shape->body;
    }

    self->data.toi = self->tfinal;
    return true;
}

void raycast_init(raycast_t *self, q3_vec3_t spot, q3_vec3_t dir) {
    self->data.start = spot;
    self->data.dir = q3_norm(dir);
    self->data.t = 10000.f;
    self->tfinal = FLT_MAX;
    self->data.toi = self->data.t;
    self->impact_body = NULL;
}

struct {
    raycast_t ray_cast;
    f32 acc;
} rp = {0};

void ray_push_init(void) {
    // floor body
    q3_bodydef_t def = q3_bodydef_default();
    q3_body_t *body = q3_scene_create_body(&scene, &def);
    q3_boxdef_t boxdef = q3_boxdef_init();
    boxdef.m_restitution = 0;
    q3_transform_t tx = {0};
    q3_tidentity(&tx);
    q3_boxdef_set(&boxdef, &tx, (q3_vec3_t){ 50.f, 1.f, 50.f });
    q3_body_add_box(body, &boxdef);

    rp.ray_cast.base.report_shape = rc_report_shape;
}

void ray_push_update(void) {
    rp.acc += dt;

    if (rp.acc > 1.0f) {
        rp.acc = 0;

        q3_bodydef_t bodydef;
        bodydef.position = (q3_vec3_t){ 0.0f, 3.0f, 0.0f };
        bodydef.axis = (q3_vec3_t){ q3_random_float(-1.0f, 1.0f), q3_random_float(-1.0f, 1.0f), q3_random_float(-1.0f, 1.0f) };
        bodydef.angle = Q3_PI * q3_random_float(-1.0f, 1.0f);
        bodydef.body_type = Q3_DYNAMIC_BODY;
        bodydef.angular_velocity = (q3_vec3_t){ q3_random_float(1.0f, 3.0f), q3_random_float(1.0f, 3.0f), q3_random_float(1.0f, 3.0f) };
        bodydef.angular_velocity = q3_v3mulf(bodydef.angular_velocity, q3_sign(q3_random_float(-1.0f, 1.0f)));
        bodydef.linear_velocity = (q3_vec3_t){ q3_random_float(1.0f, 3.0f), q3_random_float(1.0f, 3.0f), q3_random_float(1.0f, 3.0f) };
        bodydef.linear_velocity = q3_v3mulf(bodydef.linear_velocity, q3_sign(q3_random_float(-1.0f, 1.0f)));
        q3_body_t *body = q3_scene_create_body(&scene, &bodydef);

        q3_boxdef_t boxdef = q3_boxdef_init();
        q3_transform_t tx = {0};
        q3_tidentity(&tx);
        q3_boxdef_set(&boxdef, &tx, (q3_vec3_t){ 1.f, 1.f, 1.f });
        q3_body_add_box(body, &boxdef);
    }

    raycast_init(&rp.ray_cast, (q3_vec3_t){3,5,3}, (q3_vec3_t){-1,-1,-1});
    q3_scene_ray_cast(&scene, (q3_query_callback_i *)&rp.ray_cast, &rp.ray_cast.data);

    if (rp.ray_cast.impact_body) {
        q3_body_set_to_awake(rp.ray_cast.impact_body);
        q3_body_apply_force_at_world_point(
            rp.ray_cast.impact_body, 
            q3_v3mulf(rp.ray_cast.data.dir, 20.f), 
            q3_raycast_data_get_impact_point(&rp.ray_cast.data)
      );
    }
}

void ray_push_draw(q3_render_t *drawer) {
    drawer->set_scale(drawer, 1.f, 1.f, 1.f);
    drawer->set_pen_color(drawer, 0.2f, 0.5f, 1.f, 1.f);
    drawer->set_pen_position(
        drawer,
        rp.ray_cast.data.start.x,
        rp.ray_cast.data.start.y,
        rp.ray_cast.data.start.z
  );
    q3_vec3_t impact = q3_raycast_data_get_impact_point(&rp.ray_cast.data);
    drawer->line(drawer, impact.x, impact.y, impact.z);

    drawer->set_pen_position(drawer, impact.x, impact.y, impact.z);
    drawer->set_pen_color(drawer, 1.0f, 0.5f, 0.5f, 1.f);
    drawer->set_scale(drawer, 10.0f, 10.0f, 10.0f);
    drawer->point(drawer);

    drawer->set_pen_color(drawer, 1.0f, 0.5f, 0.2f, 1.f);
    drawer->set_scale(drawer, 1.0f, 1.0f, 1.0f);
    impact = q3_v3add(impact, q3_v3mulf(rp.ray_cast.nfinal, 2.f));
    drawer->line(drawer, impact.x, impact.y, impact.z);
}

void ray_push_cleanup(void) {
    q3_scene_remove_all_bodies(&scene);
}

// =========================================================
// TEST DEMO
// =========================================================

void test_init(void) {
    // floor body
    q3_bodydef_t def = q3_bodydef_default();
    q3_body_t *body = q3_scene_create_body(&scene, &def);
    q3_boxdef_t boxdef = q3_boxdef_init();
    boxdef.m_restitution = 0;
    q3_transform_t tx = {0};
    q3_tidentity(&tx);
    q3_boxdef_set(&boxdef, &tx, (q3_vec3_t){ 50.f, 1.f, 50.f });
    q3_body_add_box(body, &boxdef);

    def.body_type = Q3_DYNAMIC_BODY;
    def.position = (q3_vec3_t){ 0, 5, 0 };
    body = q3_scene_create_body(&scene, &def);
    for (int i = 0; i < 20; ++i) {
        tx.position = (q3_vec3_t){ 
            q3_random_float(1, 10),
            q3_random_float(1, 10),
            q3_random_float(1, 10),
        };
        q3_boxdef_set(&boxdef, &tx, (q3_vec3_t){ 1.f, 1.f, 1.f });
        q3_body_add_box(body, &boxdef);
    }
}

void test_update(void) {
}

void test_draw(q3_render_t *drawer) {
}

void test_cleanup(void) {
    q3_scene_remove_all_bodies(&scene);
}

// =========================================================
// MICROUI DRAWING CODE
// =========================================================

sg_image atlas_img;
sg_view atlas_view;
sg_sampler atlas_smp;
sgl_pipeline mu_pip;

void r_init(void) {
    // atlas image data is in atlas.inl file, this only contains alpha
    // values, need to expand this to RGBA8
    uint32_t rgba8_size = ATLAS_WIDTH * ATLAS_HEIGHT * 4;
    uint32_t* rgba8_pixels = (uint32_t*) malloc(rgba8_size);
    for (int y = 0; y < ATLAS_HEIGHT; y++) {
        for (int x = 0; x < ATLAS_WIDTH; x++) {
            int index = y*ATLAS_WIDTH + x;
            rgba8_pixels[index] = 0x00FFFFFF | ((uint32_t)atlas_texture[index]<<24);
        }
    }
    atlas_img = sg_make_image(&(sg_image_desc){
        .width = ATLAS_WIDTH,
        .height = ATLAS_HEIGHT,
        .data = {
            .mip_levels[0] = {
                .ptr = rgba8_pixels,
                .size = rgba8_size
            }
        },
        .label = "microui-atlas-image",
    });
    atlas_view = sg_make_view(&(sg_view_desc){
        .texture = { .image = atlas_img },
        .label = "microui-atlas-view",
    });
    atlas_smp = sg_make_sampler(&(sg_sampler_desc){
        // LINEAR would be better for text quality in HighDPI, but the
        // atlas texture is "leaking" from neighbouring pixels unfortunately
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "microui-atlas-sampler",
    });
    mu_pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
        },
        .label = "microui-pipeline",
    });

    free(rgba8_pixels);
}

void r_begin(int disp_width, int disp_height) {
    sgl_defaults();
    sgl_push_pipeline();
    sgl_load_pipeline(mu_pip);
    sgl_enable_texture();
    sgl_texture(atlas_view, atlas_smp);
    sgl_matrix_mode_projection();
    sgl_push_matrix();
    sgl_ortho(0.0f, (float) disp_width, (float) disp_height, 0.0f, -1.0f, +1.0f);
    sgl_begin_quads();
}

void r_end(void) {
    sgl_end();
    sgl_pop_matrix();
    sgl_pop_pipeline();
}

void r_draw(void) {
    sgl_draw();
}

void r_push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    float u0 = (float) src.x / (float) ATLAS_WIDTH;
    float v0 = (float) src.y / (float) ATLAS_HEIGHT;
    float u1 = (float) (src.x + src.w) / (float) ATLAS_WIDTH;
    float v1 = (float) (src.y + src.h) / (float) ATLAS_HEIGHT;

    float x0 = (float) dst.x;
    float y0 = (float) dst.y;
    float x1 = (float) (dst.x + dst.w);
    float y1 = (float) (dst.y + dst.h);

    sgl_c4b(color.r, color.g, color.b, color.a);
    sgl_v2f_t2f(x0, y0, u0, v0);
    sgl_v2f_t2f(x1, y0, u1, v0);
    sgl_v2f_t2f(x1, y1, u1, v1);
    sgl_v2f_t2f(x0, y1, u0, v1);
}

void r_draw_rect(mu_Rect rect, mu_Color color) {
    r_push_quad(rect, atlas[ATLAS_WHITE], color);
}

void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color) {
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (const char* p = text; *p; p++) {
        mu_Rect src = atlas[ATLAS_FONT + (unsigned char)*p];
        dst.w = src.w;
        dst.h = src.h;
        r_push_quad(dst, src, color);
        dst.x += dst.w;
    }
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    r_push_quad(mu_rect(x, y, src.w, src.h), src, color);
}

int r_get_text_width(const char* text, int len) {
    int res = 0;
    for (const char* p = text; *p && len--; p++) {
        res += atlas[ATLAS_FONT + (unsigned char)*p].w;
    }
    return res;
}

static int r_get_text_height(void) {
    return 18;
}

static void r_set_clip_rect(mu_Rect rect) {
    sgl_end();
    sgl_scissor_rect(rect.x, rect.y, rect.w, rect.h, true);
    sgl_begin_quads();
}
