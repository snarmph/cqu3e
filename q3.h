#pragma once

#include <stdlib.h>

typedef float          r32;
typedef double         r64;
typedef float          f32;
typedef double         f64;
typedef signed char	   i8;
typedef signed short   i16;
typedef signed int     i32;
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define Q3_UNUSED(A) (void)A

//--------------------------------------------------------------------------------------------------
// Internal Implementation Constants (do not change unless you know what you're doing)
//--------------------------------------------------------------------------------------------------
#define Q3_SLEEP_LINEAR     (r32){ 0.01 }
#define Q3_SLEEP_ANGULAR    (r32){ (3.0 / 180.0) * q3PI }
#define Q3_SLEEP_TIME       (r32){ 0.5 }
#define Q3_BAUMGARTE        (r32){ 0.2 }
#define Q3_PENETRATION_SLOP (r32){ 0.05 }

//--------------------------------------------------------------------------------------------------
// Memory Macros
//--------------------------------------------------------------------------------------------------
inline void* q3_alloc(i32 bytes) {
	return malloc(bytes);
}

inline void q3_free(void* memory) {
	free(memory);
}

#define Q3_PTR_ADD( P, BYTES ) (((u8 *)P) + (BYTES))

//--------------------------------------------------------------------------------------------------
// q3Stack
//--------------------------------------------------------------------------------------------------

typedef struct q3_stack_entry_t q3_stack_entry_t;
struct q3_stack_entry_t {
    u8 *data;
    i32 size;
};

typedef struct q3_stack_t q3_stack_t;
struct q3_stack_t {
	u8* m_memory;
	q3_stack_entry_t* m_entries;

	u32 m_index;

	i32 m_allocation;
	i32 m_entryCount;
	i32 m_entryCapacity;
	u32 m_stackSize;
};

q3_stack_t q3_stack_init(void);
void q3_stack_destroy(q3_stack_t *stack);

void  q3_stack_reserve(q3_stack_t *stack, u32 size );
void *q3_stack_allocate(q3_stack_t *stack, i32 size );
void  q3_stack_free(q3_stack_t *stack, void *data );

//--------------------------------------------------------------------------------------------------
// q3Heap
//--------------------------------------------------------------------------------------------------
// 20 MB heap size, change as necessary
const i32 Q3K_HEAP_SIZE = 1024 * 1024 * 20;
const i32 Q3K_HEAP_INITIAL_CAPACITY = 1024;

// Operates on first fit basis in attempt to improve cache coherency

typedef struct q3_heap_header_t q3_heap_header_t;
struct q3_heap_header_t {
    q3_heap_header_t* next;
    q3_heap_header_t* prev;
    i32 size;
};
typedef struct q3_heap_free_block_t q3_heap_free_block_t;
struct q3_heap_free_block_t {
    q3_heap_header_t* header;
    i32 size;
};

typedef struct q3_heap_t q3_heap_t;
struct q3_heap_t {
	q3_heap_header_t* m_memory;

	q3_heap_free_block_t* m_freeBlocks;
	i32 m_freeBlockCount;
	i32 m_freeBlockCapacity;
};

q3_heap_t q3_heap_init(void);
void q3_heap_destroy(q3_heap_t *heap);

void *q3_heap_allocate(q3_heap_t *heap, i32 size);
void  q3_heap_free(q3_heap_t *heap, void *memory);

//--------------------------------------------------------------------------------------------------
// q3PagedAllocator
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
	i32 m_blockSize;
	i32 m_blocksPerPage;

	q3_page_t *m_pages;
	i32 m_pageCount;

	q3_block_t *m_freeList;
};

q3_paged_allocator_t q3_paged_allocator_init(i32 element_size, i32 elements_per_page);
void q3_paged_allocator_cleanup(q3_paged_allocator_t *alloc);

void* q3_paged_allocator_allocate(q3_paged_allocator_t *alloc);
void  q3_paged_allocator_free(q3_paged_allocator_t *alloc, void* data);
void  q3_paged_allocator_clear(q3_paged_allocator_t *alloc);

#define Q3_R32_MAX FLT_MAX

const r32 Q3_PI = 3.14159265f;

typedef struct q3_vec3_t q3_vec3_t;
struct q3_vec3_t {
	union {
		r32 v[ 3 ];

		struct {
			r32 x;
			r32 y;
			r32 z;
		};
	};
};

q3_vec3_t q3_vec3_init(r32 x, r32 y, r32 z);
void q3_vec3_set(q3_vec3_t *vec, r32 _x, r32 _y, r32 _z );
void q3_vec3_setall(q3_vec3_t *vec, r32 a );

q3_vec3_t q3_add(q3_vec3_t left, q3_vec3_t right);
q3_vec3_t q3_sub(q3_vec3_t left, q3_vec3_t right);
q3_vec3_t q3_mul(q3_vec3_t vec, r32 f);
q3_vec3_t q3_div(q3_vec3_t vec, r32 f);
q3_vec3_t q3_neg(q3_vec3_t *self);

// TODO: q3vec3.inl

typedef struct q3_mat3_t q3_mat3_t;
struct q3_mat3_t {
	q3_vec3_t ex;
	q3_vec3_t ey;
	q3_vec3_t ez;
};

q3_mat3_t q3_mat3_init(r32 a, r32 b, r32 c, r32 d, r32 e, r32 f, r32 g, r32 h, r32 i);
q3_mat3_t q3_mat3_from_rows(q3_vec3_t _x, q3_vec3_t _y, q3_vec3_t _z);

void q3_mat3_set(q3_mat3_t *mat, r32 a, r32 b, r32 c, r32 d, r32 e, r32 f, r32 g, r32 h, r32 i );
void q3_mat3_set_from_angle(q3_mat3_t *mat, q3_vec3_t axis, r32 angle);
void q3_mat3_setrows(q3_mat3_t *mat, q3_vec3_t x, q3_vec3_t y, q3_vec3_t z);

q3_mat3_t q3_mat3_mul(q3_mat3_t *left, q3_mat3_t *right);
q3_mat3_t q3_mat3_mulv(r32 f );
q3_mat3_t q3_mat3_add(q3_mat3_t *left, q3_mat3_t *right);
q3_mat3_t q3_mat3_sub(q3_mat3_t *left, q3_mat3_t *right);

// TODO: q3mat3.inl

typedef struct q3_quat_t q3_quat_t;
struct q3_quat_t {
	union {
		r32 v[ 4 ];

		struct {
			r32 x;
			r32 y;
			r32 z;

			r32 w;
		};
	};
};

q3_quat_t q3_quat_init(r32 a, r32 b, r32 c, r32 d);
q3_quat_t q3_quat_from_angle(q3_vec3_t axis, r32 radians);

void q3_quat_set(q3_quat_t *quat, q3_vec3_t axis, r32 radians);
void q3_quat_toaxisangle(q3_quat_t quat, q3_vec3_t* axis, r32* angle);
void q3_quat_integrate(q3_quat_t *quat, q3_vec3_t dv, r32 dt);

q3_quat_t q3_quat_mul(q3_quat_t left, q3_quat_t right);
q3_mat3_t q3_quat_to_mat3(q3_quat_t quat);

// TODO: inline stuff

typedef struct q3_transform_t q3_transform_t;
struct q3_transform_t {
	q3_vec3_t position;
	q3_mat3_t rotation;
};

// TODO: q3transform.inl

// TODO: q3math.inl

// #include "../math/q3Math.h"
// #include "../common/q3Geometry.h"
// #include "q3DynamicAABBTree.h"
// #include "../broadphase/q3BroadPhase.h"
// #include "../dynamics/q3ContactManager.h"

// #include "scene/q3Scene.h"

