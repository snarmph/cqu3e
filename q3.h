#pragma once

#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef float          q3_r32;
typedef double         q3_r64;
typedef float          q3_f32;
typedef double         q3_f64;
typedef signed char	   q3_i8;
typedef signed short   q3_i16;
typedef signed int     q3_i32;
typedef unsigned char  q3_u8;
typedef unsigned short q3_u16;
typedef unsigned int   q3_u32;

#define Q3_UNUSED(A) (void)A

typedef struct q3_body_t         q3_body_t;
typedef struct q3_raycast_data_t q3_raycast_data_t;
typedef struct q3_aabb_t         q3_aabb_t;
typedef struct q3_render_t       q3_render_t;
typedef struct q3_manifold_t     q3_manifold_t;
typedef struct q3_scene_t        q3_scene_t;
typedef struct q3_contact_edge_t q3_contact_edge_t;

//--------------------------------------------------------------------------------------------------
// Internal Implementation Constants (do not change unless you know what you're doing)
//--------------------------------------------------------------------------------------------------
#define Q3_SLEEP_LINEAR     (0.01f)
#define Q3_SLEEP_ANGULAR    ((3.0f / 180.0f) * Q3_PI)
#define Q3_SLEEP_TIME       (0.5f)
#define Q3_BAUMGARTE        (0.2f)
#define Q3_PENETRATION_SLOP (0.05f)

//--------------------------------------------------------------------------------------------------
// Memory Macros
//--------------------------------------------------------------------------------------------------
static inline void* q3_alloc(q3_i32 bytes) {
	return calloc(1, bytes);
	// return malloc(bytes);
}

static inline void q3_free(void* memory) {
	free(memory);
}

#define Q3_PTR_ADD(P, BYTES) (((q3_u8 *)P) + (BYTES))

//--------------------------------------------------------------------------------------------------
// q3Stack
//--------------------------------------------------------------------------------------------------

typedef struct q3_stack_entry_t q3_stack_entry_t;
struct q3_stack_entry_t {
    q3_u8 *data;
    q3_i32 size;
};

typedef struct q3_stack_t q3_stack_t;
struct q3_stack_t {
	q3_u8* m_memory;
	q3_stack_entry_t* m_entries;

	q3_u32 m_index;

	q3_i32 m_allocation;
	q3_i32 m_entry_count;
	q3_i32 m_entry_capacity;
	q3_u32 m_stack_size;
};

q3_stack_t q3_stack_init(void);
void q3_stack_destroy(q3_stack_t *self);

void  q3_stack_reserve(q3_stack_t *self, q3_u32 size);
void *q3_stack_allocate(q3_stack_t *self, q3_i32 size);
void  q3_stack_free(q3_stack_t *self, void *data);

//--------------------------------------------------------------------------------------------------
// q3Heap
//--------------------------------------------------------------------------------------------------
// 20 MB heap size, change as necessary
const q3_i32 Q3K_HEAP_SIZE = 1024 * 1024 * 20;
const q3_i32 Q3K_HEAP_INITIAL_CAPACITY = 1024;

// Operates on first fit basis in attempt to improve cache coherency

typedef struct q3_heap_header_t q3_heap_header_t;
struct q3_heap_header_t {
    q3_heap_header_t* next;
    q3_heap_header_t* prev;
    q3_i32 size;
};
typedef struct q3_heap_free_block_t q3_heap_free_block_t;
struct q3_heap_free_block_t {
    q3_heap_header_t* header;
    q3_i32 size;
};

typedef struct q3_heap_t q3_heap_t;
struct q3_heap_t {
	q3_heap_header_t* m_memory;

	q3_heap_free_block_t* m_free_blocks;
	q3_i32 m_free_block_count;
	q3_i32 m_free_block_capacity;
};

q3_heap_t q3_heap_init(void);
void q3_heap_destroy(q3_heap_t *self);

void *q3_heap_allocate(q3_heap_t *self, q3_i32 size);
void  q3_heap_free(q3_heap_t *self, void *memory);

//--------------------------------------------------------------------------------------------------
// q3Paged_allocator
//--------------------------------------------------------------------------------------------------

typedef struct q3_block_t q3_block_t;
struct q3_block_t
{
    q3_block_t* next;
};

typedef struct q3_page_t q3_page_t;
struct q3_page_t
{
    q3_page_t* next;
    q3_block_t* data;
};

typedef struct q3_paged_allocator_t q3_paged_allocator_t;
struct q3_paged_allocator_t
{
	q3_i32 m_block_size;
	q3_i32 m_blocks_per_page;

	q3_page_t *m_pages;
	q3_i32 m_page_count;

	q3_block_t *m_freelist;
};

q3_paged_allocator_t q3_paged_allocator_init(q3_i32 element_size, q3_i32 elements_per_page);
void q3_paged_allocator_destroy(q3_paged_allocator_t *self);

void* q3_paged_allocator_allocate(q3_paged_allocator_t *self);
void  q3_paged_allocator_free(q3_paged_allocator_t *self, void* data);
void  q3_paged_allocator_clear(q3_paged_allocator_t *self);

//--------------------------------------------------------------------------------------------------
// Math Utils
//--------------------------------------------------------------------------------------------------

static inline void   q3_swap(q3_r32 *a, q3_r32 *b);
static inline void   q3_uswap(q3_u8 *a, q3_u8 *b);
static inline q3_r32 q3_invert(q3_r32 a);
static inline q3_r32 q3_sign(q3_r32 a);
static inline q3_r32 q3_abs(q3_r32 a);
static inline q3_i32 q3_imin(q3_i32 a, q3_i32 b);
static inline q3_r32 q3_rmin(q3_r32 a, q3_r32 b);
static inline q3_i32 q3_imax(q3_i32 a, q3_i32 b);
static inline q3_r32 q3_rmax(q3_r32 a, q3_r32 b);
static inline q3_u8  q3_umax(q3_u8 a, q3_u8 b);
static inline q3_r32 q3_clamp01(q3_r32 val);
static inline q3_r32 q3_clamp(q3_r32 min, q3_r32 max, q3_r32 a);
static inline q3_r32 q3_lerp(q3_r32 a, q3_r32 b, q3_r32 t);
static inline q3_r32 q3_random_float(q3_r32 l, q3_r32 h);
static inline q3_i32 q3_random_int(q3_i32 low, q3_i32 high);

//--------------------------------------------------------------------------------------------------
// q3Vec3
//--------------------------------------------------------------------------------------------------

#define Q3_R32_MAX FLT_MAX

const q3_r32 Q3_PI = 3.14159265f;

typedef struct q3_vec3_t q3_vec3_t;
struct q3_vec3_t {
	union {
		q3_r32 v[3];

		struct {
			q3_r32 x;
			q3_r32 y;
			q3_r32 z;
		};
	};
};

void q3_vec3_setall(q3_vec3_t *self, q3_r32 a);

q3_vec3_t q3_v3add(q3_vec3_t left, q3_vec3_t right);
q3_vec3_t q3_v3addf(q3_vec3_t vec, q3_r32 f);
q3_vec3_t q3_v3sub(q3_vec3_t left, q3_vec3_t right);
q3_vec3_t q3_v3subf(q3_vec3_t vec, q3_r32 f);
q3_vec3_t q3_v3mulf(q3_vec3_t vec, q3_r32 f);
q3_vec3_t q3_v3divf(q3_vec3_t vec, q3_r32 f);
q3_vec3_t q3_v3neg(q3_vec3_t vec);

static inline q3_vec3_t q3_v3identity(void);
static inline q3_vec3_t q3_v3mul(q3_vec3_t left, q3_vec3_t right);
static inline q3_vec3_t q3_v3div(q3_vec3_t left, q3_vec3_t right);
static inline q3_r32 q3_dot(q3_vec3_t a, q3_vec3_t b);
static inline q3_vec3_t q3_cross(q3_vec3_t a, q3_vec3_t b);
static inline q3_r32 q3_length(q3_vec3_t v);
static inline q3_r32 q3_lengthsq(q3_vec3_t v);
static inline q3_vec3_t q3_norm(q3_vec3_t v);
static inline q3_r32 q3_distance(q3_vec3_t a, q3_vec3_t b);
static inline q3_r32 q3_distancesq(q3_vec3_t a, q3_vec3_t b);
static inline q3_vec3_t q3_v3abs(q3_vec3_t v);
static inline q3_vec3_t q3_v3min(q3_vec3_t a, q3_vec3_t b);
static inline q3_vec3_t q3_v3max(q3_vec3_t a, q3_vec3_t b);
static inline q3_r32 q3_min_per_elem(q3_vec3_t a);
static inline q3_r32 q3_max_per_elem(q3_vec3_t a);
static inline q3_vec3_t q3_v3lerp(q3_vec3_t a, q3_vec3_t b, q3_r32 t);

//--------------------------------------------------------------------------------------------------
// q3Mat3
//--------------------------------------------------------------------------------------------------

typedef struct q3_mat3_t q3_mat3_t;
struct q3_mat3_t {
	union {
		q3_vec3_t v[3];

		struct {
            q3_vec3_t ex;
            q3_vec3_t ey;
            q3_vec3_t ez;
		};
	};
};

q3_mat3_t q3_mat3_init(q3_r32 a, q3_r32 b, q3_r32 c, q3_r32 d, q3_r32 e, q3_r32 f, q3_r32 g, q3_r32 h, q3_r32 i);
q3_mat3_t q3_mat3_from_rows(q3_vec3_t x, q3_vec3_t y, q3_vec3_t z);

void q3_mat3_set(q3_mat3_t *self, q3_r32 a, q3_r32 b, q3_r32 c, q3_r32 d, q3_r32 e, q3_r32 f, q3_r32 g, q3_r32 h, q3_r32 i);
void q3_mat3_set_from_angle(q3_mat3_t *self, q3_vec3_t axis, q3_r32 angle);
void q3_mat3_setrows(q3_mat3_t *self, q3_vec3_t x, q3_vec3_t y, q3_vec3_t z);

q3_mat3_t q3_m3mul(q3_mat3_t *left, q3_mat3_t *right);
q3_mat3_t q3_m3mulf(q3_mat3_t *self, q3_r32 f);
q3_vec3_t q3_m3mulv3(q3_mat3_t *self, q3_vec3_t v);
q3_mat3_t q3_m3add(q3_mat3_t *left, q3_mat3_t *right);
q3_mat3_t q3_m3sub(q3_mat3_t *left, q3_mat3_t *right);

q3_vec3_t q3_mat3_get(q3_mat3_t *self, q3_u32 index);
q3_vec3_t q3_mat3_column0(q3_mat3_t *self);
q3_vec3_t q3_mat3_column1(q3_mat3_t *self);
q3_vec3_t q3_mat3_column2(q3_mat3_t *self);

static inline q3_mat3_t q3_m3identity(void);
static inline q3_mat3_t q3_rotate(q3_vec3_t x, q3_vec3_t y, q3_vec3_t z);
static inline q3_mat3_t q3_transpose(q3_mat3_t *m);
static inline void q3_mat3_zero(q3_mat3_t *m);
static inline q3_mat3_t q3_diagonalf(q3_r32 a);
static inline q3_mat3_t q3_diagonal(q3_r32 a, q3_r32 b, q3_r32 c);
static inline q3_mat3_t q3_outer_product(q3_vec3_t u, q3_vec3_t v);
static inline q3_mat3_t q3_covariance(q3_vec3_t *points, q3_u32 num_points);
static inline q3_mat3_t q3_inverse(q3_mat3_t *m);

//--------------------------------------------------------------------------------------------------
// q3Quaterion
//--------------------------------------------------------------------------------------------------

typedef struct q3_quat_t q3_quat_t;
struct q3_quat_t {
	union {
		q3_r32 v[4];

		struct {
			q3_r32 x;
			q3_r32 y;
			q3_r32 z;

			q3_r32 w;
		};
	};
};

q3_quat_t q3_quat_from_angle(q3_vec3_t axis, q3_r32 radians);
void q3_quat_set(q3_quat_t *self, q3_vec3_t axis, q3_r32 radians);
void q3_quat_to_axis_angle(q3_quat_t self, q3_vec3_t* axis, q3_r32* angle);
void q3_quat_integrate(q3_quat_t *self, q3_vec3_t dv, q3_r32 dt);
q3_quat_t q3_qmul(q3_quat_t left, q3_quat_t right);
q3_mat3_t q3_quat_to_mat3(q3_quat_t quat);

static inline q3_quat_t q3_qnorm(q3_quat_t q);

//--------------------------------------------------------------------------------------------------
// q3Transform
//--------------------------------------------------------------------------------------------------

typedef struct q3_transform_t q3_transform_t;
struct q3_transform_t {
	q3_vec3_t position;
	q3_mat3_t rotation;
};

static inline q3_vec3_t q3_tmulv3(q3_transform_t *tx, q3_vec3_t v);
static inline q3_vec3_t q3_tmul_scale_v3(q3_transform_t *tx, q3_vec3_t scale, q3_vec3_t v);
static inline q3_transform_t q3_tmul(q3_transform_t *t, q3_transform_t *u);
static inline q3_vec3_t q3_tmulv3_trans(q3_transform_t *tx, q3_vec3_t v);
static inline q3_vec3_t q3_m3mulv3_trans(q3_mat3_t *r, q3_vec3_t v);
static inline q3_mat3_t q3_m3mul_trans(q3_mat3_t *r, q3_mat3_t *q);
static inline q3_transform_t q3_tmul_trans(q3_transform_t *t, q3_transform_t *u);
static inline void q3_tidentity(q3_transform_t *tx);
static inline q3_transform_t q3_tinverse(q3_transform_t *tx);

//--------------------------------------------------------------------------------------------------
// q3Mass_data
//--------------------------------------------------------------------------------------------------
typedef struct q3_mass_data_t  q3_mass_data_t;
struct q3_mass_data_t {
	q3_mat3_t inertia;
	q3_vec3_t center;
	q3_r32 mass;
};

//--------------------------------------------------------------------------------------------------
// q3Box
//--------------------------------------------------------------------------------------------------

typedef struct q3_box_t q3_box_t;
struct q3_box_t {
	q3_transform_t local;
	q3_vec3_t e; // extent, as in the extent of each OBB axis

	q3_box_t* next;
	q3_body_t* body;
	q3_r32 friction;
	q3_r32 restitution;
	q3_r32 density;
	q3_i32 broad_phase_index;
	void* user_data;
	bool sensor;
};

bool  q3_box_test_point(q3_box_t *self, q3_transform_t *tx, q3_vec3_t p);
bool  q3_box_raycast(q3_box_t *self, q3_transform_t *tx, q3_raycast_data_t* raycast);
void  q3_box_compute_aabb(q3_box_t *self, q3_transform_t *tx, q3_aabb_t* aabb);
void  q3_box_compute_mass(q3_box_t *self, q3_mass_data_t* md);
void  q3_box_render(q3_box_t *self, q3_transform_t *tx, bool awake, q3_render_t* render);

//--------------------------------------------------------------------------------------------------
// q3Box_def
//--------------------------------------------------------------------------------------------------
typedef struct q3_boxdef_t q3_boxdef_t;
struct q3_boxdef_t {
	q3_transform_t m_tx;
	q3_vec3_t m_e;

	q3_r32 m_friction;
	q3_r32 m_restitution;
	q3_r32 m_density;
	bool m_sensor;
};

q3_boxdef_t q3_boxdef_init(void);
void q3_boxdef_set(q3_boxdef_t *self, q3_transform_t *tx, q3_vec3_t extents);

//--------------------------------------------------------------------------------------------------
// q3Contact
//--------------------------------------------------------------------------------------------------
typedef struct q3_contact_constraint_t q3_contact_constraint_t;

// in stands for "incoming"
// out stands for "outgoing"
// I stands for "incident"
// R stands for "reference"
// See D. Gregorius GDC 2015 on creating contacts for more details
// Each feature pair is used to cache solutions from one physics tick to another. This is
// called warmstarting, and lets boxes stack and stay stable. Feature pairs identify points
// of contact over multiple physics ticks. Each feature pair is the junction of an incoming
// feature and an outgoing feature, usually a result of clipping routines. The exact info
// stored in the feature pair can be arbitrary as long as the result is a unique ID for a
// given intersecting configuration.
typedef union q3_feature_pair_t q3_feature_pair_t;
union q3_feature_pair_t {
	struct {
		q3_u8 in_r;
		q3_u8 out_r;
		q3_u8 in_i;
		q3_u8 out_i;
	};

	q3_i32 key;
};

typedef struct q3_contact_t q3_contact_t;
struct q3_contact_t {
	q3_vec3_t position;			// World coordinate of contact
	q3_r32 penetration;			// Depth of penetration from collision
	q3_r32 normal_impulse;			// Accumulated normal impulse
	q3_r32 tangent_impulse[2];	// Accumulated friction impulse
	q3_r32 bias;					// Restitution + baumgarte
	q3_r32 normal_mass;				// Normal constraint mass
	q3_r32 tangent_mass[2];		// Tangent constraint mass
	q3_feature_pair_t fp;			// Features on A and B for this contact
	q3_u8 warm_started;				// Used for debug rendering
};

typedef struct q3_manifold_t q3_manifold_t;
struct q3_manifold_t {
	q3_box_t *A;
	q3_box_t *B;

	q3_vec3_t normal;				// From A to B
	q3_vec3_t tangent_vectors[2];	// Tangent vectors
	q3_contact_t contacts[8];
	q3_i32 contact_count;

	q3_manifold_t* next;
	q3_manifold_t* prev;

	bool sensor;
};

void q3_manifold_set_pair(q3_manifold_t *self, q3_box_t *a, q3_box_t *b);

typedef struct q3_contact_edge_t q3_contact_edge_t;
struct q3_contact_edge_t
{
	q3_body_t *other;
	q3_contact_constraint_t *constraint;
	q3_contact_edge_t* next;
	q3_contact_edge_t* prev;
};

struct q3_contact_constraint_t {
	q3_box_t *A, *B;
	q3_body_t *body_a, *body_b;

	q3_contact_edge_t edge_a;
	q3_contact_edge_t edge_b;
	q3_contact_constraint_t* next;
	q3_contact_constraint_t* prev;

	q3_r32 friction;
	q3_r32 restitution;

	q3_manifold_t manifold;

	q3_i32 m_flags;
};

typedef enum {
    Q3_COLLIDING     = 0x00000001, // Set when contact collides during a step
    Q3_WAS_COLLIDING = 0x00000002, // Set when two objects stop colliding
    Q3_ISLAND        = 0x00000004, // For internal marking during island forming
} q3_contact_constraint_flags_e;

void q3_contact_constraint_solve_collision(q3_contact_constraint_t *self);

static inline q3_r32 q3_mix_restitution(q3_box_t *a, q3_box_t *b);
static inline q3_r32 q3_mix_friction(q3_box_t *a, q3_box_t *b);

//--------------------------------------------------------------------------------------------------
// q3Collide
//--------------------------------------------------------------------------------------------------

void q3_box_to_box(q3_manifold_t* m, q3_box_t* a, q3_box_t* b);

typedef enum {
	Q3_STATIC_BODY,
	Q3_DYNAMIC_BODY,
	Q3_KINEMATIC_BODY
} q3_body_type_e;

struct q3_body_t {
	q3_mat3_t m_inv_inertia_model;
	q3_mat3_t m_inv_inertia_world;
	q3_r32 m_mass;
	q3_r32 m_inv_mass;
	q3_vec3_t m_linear_velocity;
	q3_vec3_t m_angular_velocity;
	q3_vec3_t m_force;
	q3_vec3_t m_torque;
	q3_transform_t m_tx;
	q3_quat_t m_q;
	q3_vec3_t m_local_center;
	q3_vec3_t m_world_center;
	q3_r32 m_sleep_time;
	q3_r32 m_gravity_scale;
	q3_i32 m_layers;
	q3_i32 m_flags;

	q3_box_t* m_boxes;
	void *m_user_data;
	q3_scene_t* m_scene;
	q3_body_t* m_next;
	q3_body_t* m_prev;
	q3_i32 m_island_index;

	q3_r32 m_linear_damping;
	q3_r32 m_angular_damping;

	q3_contact_edge_t* m_contact_list;
};

// Adds a box to this body. Boxes are all defined in local space
// of their owning body. Boxes cannot be defined relative to one
// another. The body will recalculate its mass values. No contacts
// will be created until the next q3Scene::Step() call.
q3_box_t* q3_body_add_box(q3_body_t *self, q3_boxdef_t *def);
// Removes this box from the body and broadphase. Forces the body
// to recompute its mass if the body is dynamic. Frees the memory
// pointed to by the box pointer.
void q3_body_remove_box(q3_body_t *self, q3_box_t *box);
// Removes all boxes from this body and the broadphase.
void q3_body_remove_all_boxes(q3_body_t *self);
void q3_body_apply_linear_force(q3_body_t *self, q3_vec3_t force);
void q3_body_apply_force_at_world_point(q3_body_t *self, q3_vec3_t force, q3_vec3_t point);
void q3_body_apply_linear_impulse(q3_body_t *self, q3_vec3_t impulse);
void q3_body_apply_linear_impulse_at_world_point(q3_body_t *self, q3_vec3_t impulse, q3_vec3_t point);
void q3_body_apply_torque(q3_body_t *self, q3_vec3_t torque);
void q3_body_set_to_awake(q3_body_t *self);
void q3_body_set_to_sleep(q3_body_t *self);
bool q3_body_is_awake(q3_body_t *self);
q3_vec3_t q3_body_get_local_point(q3_body_t *self, q3_vec3_t p);
q3_vec3_t q3_body_get_local_vector(q3_body_t *self, q3_vec3_t v);
q3_vec3_t q3_body_get_world_point(q3_body_t *self, q3_vec3_t p);
q3_vec3_t q3_body_get_world_vector(q3_body_t *self, q3_vec3_t v);
q3_vec3_t q3_body_get_velocity_at_world_point(q3_body_t *self, q3_vec3_t p);
void q3_body_set_linear_velocity(q3_body_t *self, q3_vec3_t v);
void q3_body_set_angular_velocity(q3_body_t *self, q3_vec3_t v);
bool q3_body_can_collide(q3_body_t *self, q3_body_t *other);
// Manipulating the transformation of a body manually will result in
// non-physical behavior. Contacts are updated upon the next call to
// q3Scene::Step(). Parameters are in world space. All body types
// can be updated.
void q3_body_set_transform(q3_body_t *self, q3_vec3_t position);
void q3_body_set_transform_angle(q3_body_t *self, q3_vec3_t position, q3_vec3_t axis, q3_r32 angle);
// Used for debug rendering lines, triangles and basic lighting
void q3_body_render(q3_body_t *self, q3_render_t *render);
// Dump this rigid body and its shapes into a log file. The log can be
// used as C++ code to re-create an initial scene setup.
void q3_body_dump(q3_body_t *self, FILE *file, q3_i32 index);

//--------------------------------------------------------------------------------------------------
// q3Body_def
//--------------------------------------------------------------------------------------------------
typedef struct q3_bodydef_t q3_bodydef_t;
struct q3_bodydef_t {
	q3_vec3_t axis;			// Initial world transformation.
	q3_r32 angle;				// Initial world transformation. Radians.
	q3_vec3_t position;		// Initial world transformation.
	q3_vec3_t linear_velocity;	// Initial linear velocity in world space.
	q3_vec3_t angular_velocity;	// Initial angular velocity in world space.
	q3_r32 gravity_scale;		// Convenient scale values for gravity x, y and z directions.
	q3_i32 layers;				// Bitmask of collision layers. Bodies matching at least one layer can collide.
	void* user_data;			// Use to store application specific data.

	q3_r32 linear_damping;
	q3_r32 angular_damping;

	// Static, dynamic or kinematic. Dynamic bodies with zero mass are defaulted
	// to a mass of 1. Static bodies never move or integrate, and are very CPU
	// efficient. Static bodies have infinite mass. Kinematic bodies have
	// infinite mass, but *do* integrate and move around. Kinematic bodies do not
	// resolve any collisions.
	q3_body_type_e body_type;

	bool allow_sleep;	// Sleeping lets a body assume a non-moving state. Greatly reduces CPU usage.
	bool awake;			// Initial sleep state. True means awake.
	bool active;		// A body can start out inactive and just sits in memory.
	bool lock_axis_x;		// Locked rotation on the x axis.
	bool lock_axis_y;		// Locked rotation on the y axis.
	bool lock_axis_z;		// Locked rotation on the z axis.
};

q3_bodydef_t q3_bodydef_default(void);

//--------------------------------------------------------------------------------------------------
// q3AABB
//--------------------------------------------------------------------------------------------------
struct q3_aabb_t {
	q3_vec3_t min;
	q3_vec3_t max;
};

// http://box2d.org/2014/02/computing-a-basis/
static inline void q3_compute_basis(q3_vec3_t a, q3_vec3_t *__restrict b, q3_vec3_t *__restrict c);
static inline bool q3_aabb_to_aabb(q3_aabb_t *a, q3_aabb_t *b);
static inline bool q3_aabb_contains(q3_aabb_t *self, q3_aabb_t *other);
static inline bool q3_aabb_contains_point(q3_aabb_t *self, q3_vec3_t point);
static inline q3_r32 q3_aabb_surface_area(q3_aabb_t *self);
static inline q3_aabb_t q3_aabb_combine(q3_aabb_t *a, q3_aabb_t *b);

//--------------------------------------------------------------------------------------------------
// q3Half_space
//--------------------------------------------------------------------------------------------------
typedef struct q3_half_space_t q3_half_space_t;
struct q3_half_space_t {
	q3_vec3_t normal;
	q3_r32 distance;
};

void      q3_half_space_set3(q3_half_space_t *self, q3_vec3_t a, q3_vec3_t b, q3_vec3_t c);
void      q3_half_space_set2(q3_half_space_t *self, q3_vec3_t n, q3_vec3_t p);
q3_vec3_t q3_half_space_origin(q3_half_space_t *self);
q3_r32       q3_half_space_distance(q3_half_space_t *self, q3_vec3_t p);
q3_vec3_t q3_half_space_projected(q3_half_space_t *self, q3_vec3_t p);

static inline q3_half_space_t q3_tmulhs(q3_transform_t *tx, q3_half_space_t *p);
static inline q3_half_space_t q3_tmul_scale_hs(q3_transform_t *tx, q3_vec3_t scale, q3_half_space_t *p);
static inline q3_half_space_t q3_tmulhs_trans(q3_transform_t *tx, q3_half_space_t *p);

//--------------------------------------------------------------------------------------------------
// q3Raycast_data
//--------------------------------------------------------------------------------------------------
struct q3_raycast_data_t {
	q3_vec3_t start;    // Beginning point of the ray
	q3_vec3_t dir;	    // Direction of the ray (normalized)
	q3_r32 t;			    // Time specifying ray endpoint
	q3_r32 toi;		    // Solved time of impact
	q3_vec3_t normal;	// Surface normal at impact
};

static inline void q3_raycast_data_set(q3_raycast_data_t *self, q3_vec3_t start_point, q3_vec3_t direction, q3_r32 end_point_time);
// Uses toi, start and dir to compute the point at toi. Should
// only be called after a raycast has been conducted with a
// return value of true.
static inline q3_vec3_t q3_raycast_data_get_impact_point(q3_raycast_data_t *self);

//--------------------------------------------------------------------------------------------------
// q3Dynamic_AABBTree
//--------------------------------------------------------------------------------------------------
// Resources:
// http://box2d.org/2014/08/balancing-dynamic-trees/
// http://www.randygaul.net/2013/08/06/dynamic-aabb-tree/

typedef struct q3_aabb_node_t q3_aabb_node_t;

const q3_i32 Q3_AABB_NULL_NODE = -1;
struct q3_aabb_node_t {
    // Fat AABB for leafs, bounding AABB for branches
    q3_aabb_t aabb;
    union {
        q3_i32 parent;
        q3_i32 next; // free list
    };

    // Child indices
    struct {
        q3_i32 left;
        q3_i32 right;
    };

    // Since only leaf nodes hold userdata, we can use the
    // same memory used for left/right indices to store
    // the userdata void pointer
    void *user_data;

    // leaf, free nodes = -1
    q3_i32 height;
};

typedef struct q3_dynamic_aabb_tree_t q3_dynamic_aabb_tree_t;
struct q3_dynamic_aabb_tree_t {
	q3_i32 m_root;
	q3_aabb_node_t *m_nodes;
	q3_i32 m_count;	// Number of active nodes
	q3_i32 m_capacity;	// Max capacity of nodes
	q3_i32 m_freelist;
};

typedef struct q3_query_i q3_query_i;
struct q3_query_i {
    bool (*tree_callback)(q3_query_i *self, q3_i32 id);
};

q3_dynamic_aabb_tree_t q3_dynamic_aabb_tree_init(void);
void q3_dynamic_aabb_tree_destroy(q3_dynamic_aabb_tree_t *self);

// Provide tight-AABB
q3_i32        q3_dynamic_aabb_tree_insert(q3_dynamic_aabb_tree_t *self, q3_aabb_t *aabb, void *user_data);
void       q3_dynamic_aabb_tree_remove(q3_dynamic_aabb_tree_t *self, q3_i32 id);
bool       q3_dynamic_aabb_tree_update(q3_dynamic_aabb_tree_t *self, q3_i32 id, q3_aabb_t *aabb);
void      *q3_dynamic_aabb_tree_get_user_data(q3_dynamic_aabb_tree_t *self, q3_i32 id);
q3_aabb_t *q3_dynamic_aabb_tree_get_fat_aabb(q3_dynamic_aabb_tree_t *self, q3_i32 id);
void       q3_dynamic_aabb_tree_render(q3_dynamic_aabb_tree_t *self, q3_render_t *render);
void       q3_dynamic_aabb_tree_query_aabb(q3_dynamic_aabb_tree_t *self, q3_query_i *cb, q3_aabb_t *aabb);
void       q3_dynamic_aabb_tree_query_raycast(q3_dynamic_aabb_tree_t *self, q3_query_i *cb, q3_raycast_data_t *ray_cast);
// for testing
void       q3_dynamic_aabb_tree_validate(q3_dynamic_aabb_tree_t *self);


// Insert nodes at a given index until m_capacity into the free list
static inline void q3_dynamic_aabb_tree_add_to_freelist(q3_dynamic_aabb_tree_t *self, q3_i32 index);
static inline void q3_dynamic_aabb_tree_deallocate_node(q3_dynamic_aabb_tree_t *self, q3_i32 index);

typedef struct q3_contact_manager_t q3_contact_manager_t;
typedef struct q3_box_t q3_box_t;

typedef struct q3_contact_pair_t q3_contact_pair_t;
struct q3_contact_pair_t {
	q3_i32 A;
	q3_i32 B;
};

typedef struct q3_broad_phase_t q3_broad_phase_t;
struct q3_broad_phase_t {
    q3_query_i m_interface;

	q3_contact_manager_t *m_manager;

	q3_contact_pair_t* m_pair_buffer;
	q3_i32 m_pair_count;
	q3_i32 m_pair_capacity;

	q3_i32* m_move_buffer;
	q3_i32 m_move_count;
	q3_i32 m_move_capacity;

	q3_dynamic_aabb_tree_t m_tree;
	q3_i32 m_current_index;
};

q3_broad_phase_t q3_broad_phase_init(q3_contact_manager_t *manager);
void q3_broad_phase_destroy(q3_broad_phase_t *self);
void q3_broad_phase_insert_box(q3_broad_phase_t *self, q3_box_t *shape, q3_aabb_t *aabb);
void q3_broad_phase_remove_box(q3_broad_phase_t *self, q3_box_t *shape);
// Generates the contact list. All previous contacts are returned to the allocator
// before generation occurs.
void q3_broad_phase_update_pairs(q3_broad_phase_t *self);
void q3_broad_phase_update(q3_broad_phase_t *self, q3_i32 id, q3_aabb_t *aabb);
bool q3_broad_phase_test_overlap(q3_broad_phase_t *self, q3_i32 a, q3_i32 b);

static inline bool q3_broad_phase_tree_call_back(q3_query_i *interface, q3_i32 index);

//--------------------------------------------------------------------------------------------------
// q3Contact_manager
//--------------------------------------------------------------------------------------------------

typedef struct q3_contact_constraint_t q3_contact_constraint_t;
typedef struct q3_contact_listener_i q3_contact_listener_i;

typedef struct q3_contact_manager_t q3_contact_manager_t;
struct q3_contact_manager_t {
	q3_contact_constraint_t* m_contact_list;
	q3_i32 m_contact_count;
	q3_stack_t* m_stack;
	q3_paged_allocator_t m_allocator;
	q3_broad_phase_t m_broadphase;
	q3_contact_listener_i *m_contact_listener;
};

void q3_contact_manager_init(q3_contact_manager_t *self, q3_stack_t *stack);

// Add a new contact constraint for a pair of objects
// unless the contact constraint already exists
void q3_contact_manager_add_contact(q3_contact_manager_t *self, q3_box_t *a, q3_box_t *b);
// Has broadphase find all contacts and call Add_contact on the
// Contact_manager for each pair found
void q3_contact_manager_find_new_contacts(q3_contact_manager_t *self);
// Remove a specific contact
void q3_contact_manager_remove_contact(q3_contact_manager_t *self, q3_contact_constraint_t *contact);
// Remove all contacts from a body
void q3_contact_manager_remove_contacts_from_body(q3_contact_manager_t *self, q3_body_t *body);
void q3_contact_manager_remove_from_broadphase(q3_contact_manager_t *self, q3_body_t *body);
// Remove contacts without broadphase overlap
// Solves contact manifolds
void q3_contact_manager_test_collisions(q3_contact_manager_t *self);
void q3_contact_manager_render_contacts(q3_contact_manager_t *self, q3_render_t* debug_drawer);

//--------------------------------------------------------------------------------------------------
// q3Contact_solver
//--------------------------------------------------------------------------------------------------
typedef struct q3_island_t q3_island_t;
typedef struct q3_velocity_state_t q3_velocity_state_t;

typedef struct q3_contact_state_t q3_contact_state_t;
struct q3_contact_state_t
{
	q3_vec3_t ra;			// Vector from C.O.M to contact position
	q3_vec3_t rb;			// Vector from C.O.M to contact position
	q3_r32 penetration;		// Depth of penetration from collision
	q3_r32 normal_impulse;		// Accumulated normal impulse
	q3_r32 tangent_impulse[2];	// Accumulated friction impulse
	q3_r32 bias;				// Restitution + baumgarte
	q3_r32 normal_mass;			// Normal constraint mass
	q3_r32 tangent_mass[2];		// Tangent constraint mass
};

typedef struct q3_contact_constraint_state_t q3_contact_constraint_state_t;
struct q3_contact_constraint_state_t {
	q3_contact_state_t contacts[8];
	q3_i32 contact_count;
	q3_vec3_t tangent_vectors[2];	// Tangent vectors
	q3_vec3_t normal;				// From A to B
	q3_vec3_t center_a;
	q3_vec3_t center_b;
	q3_mat3_t i_a;
	q3_mat3_t i_b;
	q3_r32 m_a;
	q3_r32 m_b;
	q3_r32 restitution;
	q3_r32 friction;
	q3_i32 index_a;
	q3_i32 index_b;
};

typedef struct q3_contact_solver_t q3_contact_solver_t;
struct q3_contact_solver_t {
	q3_island_t *m_island;
	q3_contact_constraint_state_t *m_contacts;
	q3_i32 m_contact_count;
	q3_velocity_state_t *m_velocities;

	bool m_enable_friction;
};

void q3_contact_solver_initialize(q3_contact_solver_t *self, q3_island_t *island);
void q3_contact_solver_shut_down(q3_contact_solver_t *self);
void q3_contact_solver_pre_solve(q3_contact_solver_t *self, q3_r32 dt);
void q3_contact_solver_solve(q3_contact_solver_t *self);

//--------------------------------------------------------------------------------------------------
// q3Island
//--------------------------------------------------------------------------------------------------
typedef struct q3_velocity_state_t q3_velocity_state_t;
struct q3_velocity_state_t {
	q3_vec3_t w;
	q3_vec3_t v;
};

typedef struct q3_island_t q3_island_t;
struct q3_island_t
{

	q3_body_t **m_bodies;
	q3_velocity_state_t *m_velocities;
	q3_i32 m_body_capacity;
	q3_i32 m_body_count;

	q3_contact_constraint_t **m_contacts;
	q3_contact_constraint_state_t *m_contact_states;
	q3_i32 m_contact_count;
	q3_i32 m_contact_capacity;

	q3_r32 m_dt;
	q3_vec3_t m_gravity;
	q3_i32 m_iterations;

	bool m_allow_sleep;
	bool m_enable_friction;
};

void q3_island_solve(q3_island_t *self);
void q3_island_add_body(q3_island_t *self, q3_body_t *body);
void q3_island_add_costraint(q3_island_t *self, q3_contact_constraint_t *contact);
void q3_island_initialize(q3_island_t *self);

//--------------------------------------------------------------------------------------------------
// q3Scene
//--------------------------------------------------------------------------------------------------

// This listener is used to gather information about two shapes colliding. This
// can be used for game logic and sounds. Physics objects created in these
// callbacks will not be reported until the following frame. These callbacks
// can be called frequently, so make them efficient.
typedef struct q3_contact_listener_i q3_contact_listener_i;
struct q3_contact_listener_i {
	void (*begin_contact)(q3_contact_listener_i *self, q3_contact_constraint_t *contact);
	void (*end_contact)(q3_contact_listener_i *self, q3_contact_constraint_t *contact);
};

// This class represents general queries for points, AABBs and Raycasting.
// report_shape is called the moment a valid shape is found. The return
// value of Report_shape controls whether to continue or stop the query.
// By returning only true, all shapes that fulfill the query will be re-
// ported.
typedef struct q3_query_callback_i q3_query_callback_i;
struct q3_query_callback_i {
	bool (*report_shape)(q3_query_callback_i *self, q3_box_t *box);
};

struct q3_scene_t {
	q3_contact_manager_t m_contact_manager;
	q3_paged_allocator_t m_box_allocator;

	q3_i32 m_body_count;
	q3_body_t* m_body_list;

	q3_stack_t m_stack;
	q3_heap_t m_heap;

	q3_vec3_t m_gravity;
	q3_r32 m_dt;
	q3_i32 m_iterations;

	bool m_new_box;
	bool m_allow_sleep;
	bool m_enable_friction;
};

typedef struct q3_scene_desc_t q3_scene_desc_t;
struct q3_scene_desc_t {
    q3_r32 dt;
    q3_vec3_t gravity; // default (0, -9.8, 0)
    q3_i32 iterations; // default 20
};

void q3_scene_init(q3_scene_t *self, q3_scene_desc_t *desc);
void q3_scene_destroy(q3_scene_t *self);
// Run the simulation forward in time by dt (fixed timestep). Variable
// timestep is not supported.
void q3_scene_step(q3_scene_t *self);
// Construct a new rigid body. The Body_def can be reused at the user's
// discretion, as no reference to the Body_def is kept.
q3_body_t* q3_scene_create_body(q3_scene_t *self, q3_bodydef_t *def);
// Frees a body, removes all shapes associated with the body and frees
// all shapes and contacts associated and attached to this body.
void q3_scene_remove_body(q3_scene_t *self, q3_body_t* body);
void q3_scene_remove_all_bodies(q3_scene_t *self);
// Enables or disables rigid body sleeping. Sleeping is an effective CPU
// optimization where bodies are put to sleep if they don't move much.
// Sleeping bodies sit in memory without being updated, until the are
// touched by something that wakes them up. The default is enabled.
void q3_scene_set_allow_sleep(q3_scene_t *self, bool allow_sleep);
// Increasing the iteration count increases the CPU cost of simulating
// Scene.Step(). Decreasing the iterations makes the simulation less
// realistic (convergent). A good iteration number range is 5 to 20.
// Only positive numbers are accepted. Non-positive and negative
// inputs set the iteration count to 1.
void q3_scene_set_iterations(q3_scene_t *self, q3_i32 iterations);
// Friction occurs when two rigid bodies have shapes that slide along one
// another. The friction force resists this sliding motion.
void q3_scene_set_enable_friction(q3_scene_t *self, bool enabled);
// Render the scene with an interpolated time between the last frame and
// the current simulation step.
void q3_scene_render(q3_scene_t *self, q3_render_t* render);
// Removes all bodies from the scene.
void q3_scene_shutdown(q3_scene_t *self);
// Sets the listener to report cllision start/end. Provides the user
// with a pointer to an q3Contact_constraint. The q3Contact_constraint
// holds pointers to the two shapes involved in a collision, and the
// two bodies connected to each shape. The q3Contact_listener will be
// called very often, so it is recommended for the funciton to be very
// efficient. Provide a NULL pointer to remove the previously set
// listener.
void q3_scene_set_contact_listener(q3_scene_t *self, q3_contact_listener_i* listener);
// Query the world to find any shapes that can potentially intersect
// the provided AABB. This works by querying the broadphase with an
// AAABB -- only *potential* intersections are reported. Perhaps the
// user might use lm_distance as fine-grained collision detection.
void q3_scene_query_aabb(q3_scene_t *self, q3_query_callback_i *cb, q3_aabb_t aabb);
// Query the world to find any shapes intersecting a world space point.
void q3_scene_query_point(q3_scene_t *self, q3_query_callback_i *cb, q3_vec3_t point);
// Query the world to find any shapes intersecting a ray.
void q3_scene_ray_cast(q3_scene_t *self, q3_query_callback_i *cb, q3_raycast_data_t *ray_cast);
// Dump all rigid bodies and shapes into a log file. The log can be
// used as C++ code to re-create an initial scene setup. Contacts
// are *not* logged, meaning any cached resolution solutions will
// not be saved to the log file. This means the log file will be most
// accurate when dumped upon scene initialization, instead of mid-
// simulation.
void q3_scene_dump(q3_scene_t *self, FILE* file);

//--------------------------------------------------------------------------------------------------
// q3Render
//--------------------------------------------------------------------------------------------------
typedef struct q3_render_t q3_render_t;
struct q3_render_t {
	void (*set_pen_color)(q3_render_t *self, q3_f32 r, q3_f32 g, q3_f32 b, q3_f32 a);
	void (*set_pen_position)(q3_render_t *self, q3_f32 x, q3_f32 y, q3_f32 z);
	void (*set_scale)(q3_render_t *self, q3_f32 sx, q3_f32 sy, q3_f32 sz);
	// Render a line from pen position to this point.
	// Sets the pen position to the new point.
	void (*line)(q3_render_t *self, q3_f32 x, q3_f32 y, q3_f32 z);
	void (*set_tri_normal)(q3_render_t *self, q3_f32 x, q3_f32 y, q3_f32 z);
	// Render a triangle with the normal set by Set_tri_normal.
	void (*triangle)(
        q3_render_t *self, 
		q3_f32 x1, q3_f32 y1, q3_f32 z1,
		q3_f32 x2, q3_f32 y2, q3_f32 z2,
		q3_f32 x3, q3_f32 y3, q3_f32 z3
		);
	// Draw a point with the scale from Set_scale
	void (*point)(q3_render_t *self);
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// static inline implementations 
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

static inline void q3_swap(q3_r32 *a, q3_r32 *b) {
    q3_r32 t = *a;
    *a = *b;
    *b = t;
}

static inline void q3_uswap(q3_u8 *a, q3_u8 *b) {
    q3_u8 t = *a;
    *a = *b;
    *b = t;
}

static inline q3_r32 q3_invert(q3_r32 a) {
    return a != 0.f ? 1.f / a : 0.f;
}

static inline q3_r32 q3_sign(q3_r32 a) {
    if (a >= 0.f) {
        return 1.f;
    }
    else {
        return -1.f;
    }
}

static inline q3_r32 q3_abs(q3_r32 a) {
    if (a < 0.f) {
        return -a;
    }
    return a;
}

static inline q3_i32 q3_imin(q3_i32 a, q3_i32 b) {
    if (a < b) {
        return a;
    }
    return b;
}

static inline q3_r32 q3_rmin(q3_r32 a, q3_r32 b) {
    if (a < b) {
        return a;
    }
    return b;
}

static inline q3_i32 q3_imax(q3_i32 a, q3_i32 b) {
    if (a > b) {
        return a;
    }
    return b;
}

static inline q3_r32 q3_rmax(q3_r32 a, q3_r32 b) {
    if (a > b) {
        return a;
    }
    return b;
}

static inline q3_u8 q3_umax(q3_u8 a, q3_u8 b) {
    if (a > b) {
        return a;
    }
    return b;
}

static inline q3_r32 q3_clamp01(q3_r32 val) {
    if (val >= 1.f) {
        return 1.f;
    }
    if (val <= 0.f) {
        return 0.f;
    }
    return val;
}

static inline q3_r32 q3_clamp(q3_r32 min, q3_r32 max, q3_r32 a) {
    if (a >= max) {
        return max;
    }
    if (a <= min) {
        return min;
    }
    return a;
}

static inline q3_r32 q3_lerp(q3_r32 a, q3_r32 b, q3_r32 t) {
    return a * (1.f - t) + b * t;
}

static inline q3_r32 q3_random_float(q3_r32 l, q3_r32 h) {
    q3_r32 a = (q3_r32)rand();
    a /= (q3_r32)RAND_MAX;
    a = (h - l) * a + l;
    return a;
}

static inline q3_i32 q3_random_int(q3_i32 low, q3_i32 high) {
    return rand() % (high - low + 1) + low;
}

static inline q3_vec3_t q3_v3identity(void) {
    return (q3_vec3_t){ 0, 0, 0 };
}

static inline q3_vec3_t q3_v3mul(q3_vec3_t left, q3_vec3_t right) {
    return (q3_vec3_t){
        .x = left.x * right.x,
        .y = left.y * right.y,
        .z = left.z * right.z,
    };
}


static inline q3_vec3_t q3_v3div(q3_vec3_t left, q3_vec3_t right) {
    return (q3_vec3_t){
        .x = left.x / right.x,
        .y = left.y / right.y,
        .z = left.z / right.z,
    };
}

static inline q3_r32 q3_dot(q3_vec3_t a, q3_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline q3_vec3_t q3_cross(q3_vec3_t a, q3_vec3_t b) {
    return (q3_vec3_t){
		.x = (a.y * b.z) - (b.y * a.z),
		.y = (b.x * a.z) - (a.x * b.z),
		.z = (a.x * b.y) - (b.x * a.y)
    };
}

static inline q3_r32 q3_length(q3_vec3_t v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline q3_r32 q3_lengthsq(q3_vec3_t v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline q3_vec3_t q3_norm(q3_vec3_t v) {
    q3_r32 l = q3_length(v);
    if (l != 0) {
        q3_r32 inv = 1.f / l;
        return q3_v3mulf(v, inv);
    }

    return v;
}

static inline q3_r32 q3_distance(q3_vec3_t a, q3_vec3_t b) {
	q3_r32 xp = a.x - b.x;
	q3_r32 yp = a.y - b.y;
	q3_r32 zp = a.z - b.z;

	return sqrtf(xp * xp + yp * yp + zp * zp);
}

static inline q3_r32 q3_distancesq(q3_vec3_t a, q3_vec3_t b) {
	q3_r32 xp = a.x - b.x;
	q3_r32 yp = a.y - b.y;
	q3_r32 zp = a.z - b.z;

	return xp * xp + yp * yp + zp * zp;
}

static inline q3_vec3_t q3_v3abs(q3_vec3_t v) {
	return (q3_vec3_t){ q3_abs(v.x), q3_abs(v.y), q3_abs(v.z) };
}

static inline q3_vec3_t q3_v3min(q3_vec3_t a, q3_vec3_t b) {
	return (q3_vec3_t){ q3_rmin(a.x, b.x), q3_rmin(a.y, b.y), q3_rmin(a.z, b.z) };
}

static inline q3_vec3_t q3_v3max(q3_vec3_t a, q3_vec3_t b) {
	return (q3_vec3_t){ q3_rmax(a.x, b.x), q3_rmax(a.y, b.y), q3_rmax(a.z, b.z) };
}

static inline q3_r32 q3_min_per_elem(q3_vec3_t a) {
	return q3_rmin(a.x, q3_rmin(a.y, a.z));
}

static inline q3_r32 q3_max_per_elem(q3_vec3_t a) {
	return q3_rmax(a.x, q3_rmax(a.y, a.z));
}

static inline q3_vec3_t q3_v3lerp(q3_vec3_t a, q3_vec3_t b, q3_r32 t) {
    // (a * (1.f - t)) + (b * t)
    return q3_v3add(q3_v3mulf(a, 1.f - 5), q3_v3mulf(b, t));
}

static inline q3_mat3_t q3_m3identity(void) {
    return (q3_mat3_t){
        .ex = { 1, 0, 0 },
        .ey = { 0, 1, 0 },
        .ez = { 0, 0, 1 },
    };
}

static inline q3_mat3_t q3_rotate(q3_vec3_t x, q3_vec3_t y, q3_vec3_t z) {
	return (q3_mat3_t){ x, y, z };
}

static inline q3_mat3_t q3_transpose(q3_mat3_t *m) {
	return (q3_mat3_t) {
        .ex = { m->ex.x, m->ey.x, m->ez.x },
        .ey = { m->ex.y, m->ey.y, m->ez.y },
        .ez = { m->ex.z, m->ey.z, m->ez.z },
    };
}

static inline void q3_mat3_zero(q3_mat3_t *m) {
	memset(m, 0, sizeof(q3_r32) * 9);
}

static inline q3_mat3_t q3_diagonalf(q3_r32 a) {
	return (q3_mat3_t) {
        .ex = { a  , 0.f, 0.f },
        .ey = { 0.f, a  , 0.f },
        .ez = { 0.f, 0.f, a   },
    };
}

static inline q3_mat3_t q3_diagonal(q3_r32 a, q3_r32 b, q3_r32 c) {
	return (q3_mat3_t) {
        .ex = { a  , 0.f, 0.f },
        .ey = { 0.f, b  , 0.f },
        .ez = { 0.f, 0.f, c   },
    };
}

static inline q3_mat3_t q3_outer_product(q3_vec3_t u, q3_vec3_t v) {
	q3_vec3_t a = q3_v3mulf(v, u.x);
	q3_vec3_t b = q3_v3mulf(v, u.y);
	q3_vec3_t c = q3_v3mulf(v, u.z);

	return (q3_mat3_t) {
        .ex = { a.x, a.y, a.z },
        .ey = { b.x, b.y, b.z },
        .ez = { c.x, c.y, c.z },
    };
}

static inline q3_mat3_t q3_covariance(q3_vec3_t *points, q3_u32 num_points) {
	q3_r32 inv_num_points = 1.f / (q3_r32)num_points;
	q3_vec3_t c = {0};

	for (q3_u32 i = 0; i < num_points; ++i) {
        c = q3_v3add(c, points[i]);
    }

    c = q3_v3divf(c, (q3_r32)num_points);

	q3_r32 m00, m11, m22, m01, m02, m12;
	m00 = m11 = m22 = m01 = m02 = m12 = 0.f;

	for (q3_u32 i = 0; i < num_points; ++i) {
		q3_vec3_t p = q3_v3sub(points[i], c);

		m00 += p.x * p.x;
		m11 += p.y * p.y;
		m22 += p.z * p.z;
		m01 += p.x * p.y;
		m02 += p.x * p.z;
		m12 += p.y * p.z;
	}

	q3_r32 m01inv = m01 * inv_num_points;
	q3_r32 m02inv = m02 * inv_num_points;
	q3_r32 m12inv = m12 * inv_num_points;

	return (q3_mat3_t) {
        .ex = { m00 * inv_num_points, m01inv, m02inv },
        .ey = { m01inv, m11 * inv_num_points, m12inv },
        .ez = { m02inv, m12inv, m22 * inv_num_points },
    };
};

static inline q3_mat3_t q3_inverse(q3_mat3_t *m) {
	q3_vec3_t tmp0, tmp1, tmp2;

	tmp0 = q3_cross(m->ey, m->ez);
	tmp1 = q3_cross(m->ez, m->ex);
	tmp2 = q3_cross(m->ex, m->ey);

	q3_r32 detinv= 1.f / q3_dot(m->ez, tmp2);

	return (q3_mat3_t) {
        .ex = { tmp0.x * detinv, tmp1.x * detinv, tmp2.x * detinv },
        .ey = { tmp0.y * detinv, tmp1.y * detinv, tmp2.y * detinv },
        .ez = { tmp0.z * detinv, tmp1.z * detinv, tmp2.z * detinv },
    };
}

static inline q3_quat_t q3_qnorm(q3_quat_t q) {
	q3_r32 d = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;

	if(d == 0) {
		q.w = 1.f;
    }

	d = 1.f / sqrtf(d);

	if (d > 1.0e-8f) {
		q.x *= d;
		q.y *= d;
		q.z *= d;
		q.w *= d;
	}

    return q;
}

static inline q3_vec3_t q3_tmulv3(q3_transform_t *tx, q3_vec3_t v) {
	return q3_v3add(q3_m3mulv3(&tx->rotation, v), tx->position);
}

static inline q3_vec3_t q3_tmul_scale_v3(q3_transform_t *tx, q3_vec3_t scale, q3_vec3_t v) {
    return q3_v3add(q3_m3mulv3(&tx->rotation, q3_v3mul(scale, v)), tx->position);
}

static inline q3_transform_t q3_tmul(q3_transform_t *t, q3_transform_t *u) {
	return (q3_transform_t) {
        .rotation = q3_m3mul(&t->rotation, &u->rotation),
        .position = q3_v3add(q3_m3mulv3(&t->rotation, u->position), t->position),
    };
}

static inline q3_half_space_t q3_tmulhs(q3_transform_t *tx, q3_half_space_t *p) {
	q3_vec3_t origin = q3_half_space_origin(p);
	origin = q3_tmulv3(tx, origin);
	q3_vec3_t normal = q3_m3mulv3(&tx->rotation, p->normal);

    return (q3_half_space_t) {
        .normal = normal,
        .distance = q3_dot(origin, normal),
    };
}

static inline q3_half_space_t q3_tmul_scale_hs(q3_transform_t *tx, q3_vec3_t scale, q3_half_space_t *p) {
	q3_vec3_t origin = q3_half_space_origin(p);
	origin = q3_tmul_scale_v3(tx, scale, origin);
	q3_vec3_t normal = q3_m3mulv3(&tx->rotation, p->normal);

    return (q3_half_space_t) {
        .normal = normal,
        .distance = q3_dot(origin, normal),
    };
}

static inline q3_vec3_t q3_tmulv3_trans(q3_transform_t *tx, q3_vec3_t v) {
    q3_mat3_t t = q3_transpose(&tx->rotation);
    return q3_m3mulv3(&t, q3_v3sub(v, tx->position));
}

static inline q3_vec3_t q3_m3mulv3_trans(q3_mat3_t *r, q3_vec3_t v) {
    q3_mat3_t t = q3_transpose(r);
    return q3_m3mulv3(&t, v);
}

static inline q3_mat3_t q3_m3mul_trans(q3_mat3_t *r, q3_mat3_t *q) {
    q3_mat3_t t = q3_transpose(r);
    return q3_m3mul(&t, q);
}

static inline q3_transform_t q3_tmul_trans(q3_transform_t *t, q3_transform_t *u) {
	return (q3_transform_t) {
        .rotation = q3_m3mul_trans(&t->rotation, &u->rotation),
        .position = q3_m3mulv3_trans(&t->rotation, q3_v3sub(u->position, t->position)),
    };
}

static inline q3_half_space_t q3_tmulhs_trans(q3_transform_t *tx, q3_half_space_t *p) {
	q3_vec3_t origin = q3_v3mulf(p->normal, p->distance);
	origin = q3_tmulv3_trans(tx, origin);
	q3_vec3_t n = q3_m3mulv3_trans(&tx->rotation, p->normal);
    return (q3_half_space_t){ n, q3_dot(origin, n) };
}

static inline void q3_tidentity(q3_transform_t *tx) {
    tx->position = q3_v3identity();
    tx->rotation = q3_m3identity();
}

static inline q3_transform_t q3_tinverse(q3_transform_t *tx) {
    q3_mat3_t r = q3_transpose(&tx->rotation);
	return (q3_transform_t) {
        .rotation = r,
        .position = q3_m3mulv3(&r, q3_v3neg(tx->position)),
    };
}

// http://box2d.org/2014/02/computing-a-basis/
static inline void q3_compute_basis(q3_vec3_t a, q3_vec3_t *__restrict b, q3_vec3_t *__restrict c) {
	// Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
	// Then 3*s*s = 1, s = sqrt(1/3) = 0.57735027. This means that at least one component of a
	// unit vector must be greater or equal to 0.57735027. Can use SIMD select operation.

	if (q3_abs(a.x) >= 0.57735027f) {
        *b = (q3_vec3_t){ a.y, -a.x, 0.f };
    }
	else {
        *b = (q3_vec3_t){ 0.f, a.z, -a.y };
    }

	*b = q3_norm(*b);
	*c = q3_cross(a, *b);
}

static inline bool q3_aabb_to_aabb(q3_aabb_t *a, q3_aabb_t *b) {
	if (a->max.x < b->min.x || a->min.x > b->max.x) {
		return false;
    }

	if (a->max.y < b->min.y || a->min.y > b->max.y) {
		return false;
    }

	if (a->max.z < b->min.z || a->min.z > b->max.z) {
		return false;
    }

	return true;
}

static inline bool q3_aabb_contains(q3_aabb_t *self, q3_aabb_t *other) {
	return
		self->min.x <= other->min.x &&
		self->min.y <= other->min.y &&
		self->min.z <= other->min.z &&
		self->max.x >= other->max.x &&
		self->max.y >= other->max.y &&
		self->max.z >= other->max.z;
}

static inline bool q3_aabb_contains_point(q3_aabb_t *self, q3_vec3_t point) {
	return
		self->min.x <= point.x &&
		self->min.y <= point.y &&
		self->min.z <= point.z &&
		self->max.x >= point.x &&
		self->max.y >= point.y &&
		self->max.z >= point.z;
}

static inline q3_r32 q3_aabb_surface_area(q3_aabb_t *self) {
	q3_r32 x = self->max.x - self->min.x;
	q3_r32 y = self->max.y - self->min.y;
	q3_r32 z = self->max.z - self->min.z;

	return 2.f * (x * y + x * z + y * z);
}

static inline q3_aabb_t q3_aabb_combine(q3_aabb_t *a, q3_aabb_t *b) {
	return (q3_aabb_t){
        .min = q3_v3min(a->min, b->min),
        .max = q3_v3max(a->max, b->max),
    };
}

static inline void q3_raycast_data_set(q3_raycast_data_t *self, q3_vec3_t start_point, q3_vec3_t direction, q3_r32 end_point_time) {
    self->start = start_point;
    self->dir = direction;
    self->t = end_point_time;
}

// Uses toi, start and dir to compute the point at toi. Should
// only be called after a raycast has been conducted with a
// return value of true.
static inline q3_vec3_t q3_raycast_data_get_impact_point(q3_raycast_data_t *self) {
    return q3_v3add(self->start, q3_v3mulf(self->dir, self->toi));
}

// Insert nodes at a given index until m_capacity into the free list
static inline void q3_dynamic_aabb_tree_add_to_freelist(q3_dynamic_aabb_tree_t *self, q3_i32 index) {
	for (q3_i32 i = index; i < self->m_capacity - 1; ++i) {
		self->m_nodes[i].next = i + 1;
		self->m_nodes[i].height = Q3_AABB_NULL_NODE;
	}

	self->m_nodes[self->m_capacity - 1].next = Q3_AABB_NULL_NODE;
	self->m_nodes[self->m_capacity - 1].height = Q3_AABB_NULL_NODE;
	self->m_freelist = index;
}

static inline void q3_dynamic_aabb_tree_deallocate_node(q3_dynamic_aabb_tree_t *self, q3_i32 index) {
	assert(index >= 0 && index < self->m_capacity);

	self->m_nodes[index].next = self->m_freelist;
	self->m_nodes[index].height = Q3_AABB_NULL_NODE;
	self->m_freelist = index;

	--self->m_count;
}

static inline bool q3_broad_phase_tree_call_back(q3_query_i *interface, q3_i32 index) {
    q3_broad_phase_t *self = (q3_broad_phase_t *)interface;
	// Cannot collide with self
	if (index == self->m_current_index) {
		return true;
    }

	if (self->m_pair_count == self->m_pair_capacity) {
		q3_contact_pair_t* old_buffer = self->m_pair_buffer;
		self->m_pair_capacity *= 2;
		self->m_pair_buffer = q3_alloc(self->m_pair_capacity * sizeof(q3_contact_pair_t));
		memcpy(self->m_pair_buffer, old_buffer, self->m_pair_count * sizeof(q3_contact_pair_t));
		q3_free(old_buffer);
	}

	q3_i32 iA = q3_imin(index, self->m_current_index);
	q3_i32 iB = q3_imax(index, self->m_current_index);

	self->m_pair_buffer[self->m_pair_count].A = iA;
	self->m_pair_buffer[self->m_pair_count].B = iB;
	++self->m_pair_count;

	return true;
}

static inline q3_r32 q3_mix_restitution(q3_box_t *a, q3_box_t *b) {
    return q3_rmax(a->restitution, b->restitution);
}

static inline q3_r32 q3_mix_friction(q3_box_t *a, q3_box_t *b) {
    return sqrtf(a->friction * b->friction);
}

