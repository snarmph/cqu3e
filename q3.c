#include "q3.h"

#include <float.h>
#include <string.h>

//--------------------------------------------------------------------------------------------------
// q3Stack
//--------------------------------------------------------------------------------------------------

q3_stack_t q3_stack_init(void) {
    return (q3_stack_t) {
        .m_entries = q3_alloc(sizeof(q3_stack_entry_t) * 64),
        .m_entry_capacity = 64,
    };
}

void q3_stack_destroy(q3_stack_t *self) {
    if (self->m_memory) {
        q3_free(self->m_memory);
    }
    assert(self->m_index == 0);
    assert(self->m_entry_count == 0);
}

void q3_stack_reserve(q3_stack_t *self, q3_u32 size) {
    assert(!self->m_index);

    if (size == 0) {
        return;
    }

    if (size >= self->m_stack_size) {
        if (self->m_memory) {
            q3_free(self->m_memory);
        }
        self->m_memory = q3_alloc(size);
        self->m_stack_size = size;
    }
}

void *q3_stack_allocate(q3_stack_t *self, q3_i32 size) {
	assert(self->m_index + size <= self->m_stack_size);

	if (self->m_entry_count == self->m_entry_capacity)
	{
		q3_stack_entry_t *old_entries = self->m_entries;
		self->m_entry_capacity *= 2;
		self->m_entries = q3_alloc(self->m_entry_capacity * sizeof(q3_stack_entry_t));
		memcpy(self->m_entries, old_entries, self->m_entry_count * sizeof(q3_stack_entry_t));
		q3_free(old_entries);
	}

	q3_stack_entry_t* entry = self->m_entries + self->m_entry_count;
	entry->size = size;

	entry->data = self->m_memory + self->m_index;
	self->m_index += size;

	self->m_allocation += size;
	++self->m_entry_count;

	return entry->data;
}

void q3_stack_free(q3_stack_t *self, void *data) {
	// Cannot call free when there are no entries.
	assert(self->m_entry_count > 0);

	q3_stack_entry_t *entry = self->m_entries + self->m_entry_count - 1;

	// Validate that the data * is a proper location to free.
	// Must be in reverse order of allocation.
	assert(data == entry->data);

	self->m_index -= entry->size;

	self->m_allocation -= entry->size;
	--self->m_entry_count;
}

//--------------------------------------------------------------------------------------------------
// q3Heap
//--------------------------------------------------------------------------------------------------

q3_heap_t q3_heap_init(void) {
    q3_heap_t self = {0};

	self.m_memory->size = Q3K_HEAP_SIZE;

	self.m_free_blocks = q3_alloc(sizeof(q3_heap_free_block_t) * Q3K_HEAP_INITIAL_CAPACITY); self.m_free_block_count = 1;
	self.m_free_block_capacity = Q3K_HEAP_INITIAL_CAPACITY;

	self.m_free_blocks->header = self.m_memory;
	self.m_free_blocks->size = Q3K_HEAP_SIZE;

    return self;
}

void q3_heap_destroy(q3_heap_t *self) {
    q3_free(self->m_memory);
    q3_free(self->m_free_blocks);
}

void *q3_heap_allocate(q3_heap_t *self, q3_i32 size) {
	q3_i32 size_needed = size + sizeof(q3_heap_header_t);
	q3_heap_free_block_t* first_fit = NULL;

	for (q3_i32 i = 0; i < self->m_free_block_count; ++i) {
		q3_heap_free_block_t* block = self->m_free_blocks + i;

		if (block->size >= size_needed) {
			first_fit = block;
			break;
		}
	}

	if (!first_fit) {
		return NULL;
    }

	q3_heap_header_t* node = first_fit->header;
	q3_heap_header_t* new_node = (q3_heap_header_t*)Q3_PTR_ADD(node, size_needed);
	node->size = size_needed;

	first_fit->size -= size_needed;
	first_fit->header = new_node;
	new_node->next = node->next;
	if (node->next) {
		node->next->prev = new_node;
    }
	node->next = new_node;
	new_node->prev = node;

	return Q3_PTR_ADD(node, sizeof(q3_heap_header_t));
}

void q3_heap_free(q3_heap_t *self, void *memory) {
	assert(memory);
	q3_heap_header_t* node = (q3_heap_header_t*)Q3_PTR_ADD(memory, -(q3_i32)(sizeof(q3_heap_header_t)));

	q3_heap_header_t* next = node->next;
	q3_heap_header_t* prev = node->prev;
	q3_heap_free_block_t* next_block = NULL;
	q3_i32 prev_block_index = ~0;
	q3_heap_free_block_t* prev_block = NULL;
	q3_i32 free_block_count = self->m_free_block_count;

	for (q3_i32 i = 0; i < free_block_count; ++i) {
		q3_heap_free_block_t* block = self->m_free_blocks + i;
		q3_heap_header_t* header = block->header;

		if (header == next) {
			next_block = block;
        }
		else if (header == prev) {
			prev_block = block;
			prev_block_index = i;
	    }
	}

	bool merged = false;

	if (prev_block) {
		merged = true;

		prev->next = next;
		if (next) {
			next->prev = prev;
        }
		prev_block->size += node->size;
		prev->size = prev_block->size;

		if (next_block) {
			next_block->header = prev;
			next_block->size += prev->size;
			prev->size = next_block->size;

			q3_heap_header_t* nextnext = next->next;
			prev->next = nextnext;

			if (nextnext)
				nextnext->prev = prev;

			// Remove the next_block from the free_blocks array
			assert(self->m_free_block_count);
			assert(prev_block_index != ~0);
			--self->m_free_block_count;
			self->m_free_blocks[prev_block_index] = self->m_free_blocks[self->m_free_block_count];
		}
	}
	else if (next_block) {
		merged = true;

		next_block->header = node;
		next_block->size += node->size;
		node->size = next_block->size;

		q3_heap_header_t* nextnext = next->next;

		if (nextnext) {
			nextnext->prev = node;
        }

		node->next = nextnext;
	}

	if (!merged) {
		q3_heap_free_block_t block;
		block.header = node;
		block.size = node->size;

		if (self->m_free_block_count == self->m_free_block_capacity) {
			q3_heap_free_block_t* old_blocks = self->m_free_blocks;
			q3_i32 old_capacity = self->m_free_block_capacity;

			self->m_free_block_capacity *= 2;
			self->m_free_blocks = q3_alloc(sizeof(q3_heap_free_block_t) * self->m_free_block_capacity);

			memcpy(self->m_free_blocks, old_blocks, sizeof(q3_heap_free_block_t) * old_capacity);
			q3_free(old_blocks);
		}

		self->m_free_blocks[self->m_free_block_count++] = block;
	}
}

//--------------------------------------------------------------------------------------------------
// q3Paged_allocator
//--------------------------------------------------------------------------------------------------

q3_paged_allocator_t q3_paged_allocator_init(q3_i32 element_size, q3_i32 elements_per_page) {
    return (q3_paged_allocator_t){
        .m_block_size = element_size,
        .m_blocks_per_page = elements_per_page,
    };
}

void q3_paged_allocator_destroy(q3_paged_allocator_t *self) {
    q3_paged_allocator_clear(self);
}

void* q3_paged_allocator_allocate(q3_paged_allocator_t *self) {
	if (self->m_freelist) {
		q3_block_t* data = self->m_freelist;
		self->m_freelist = data->next;

		return data;
	}
	else {
		q3_page_t* page = q3_alloc(self->m_block_size * self->m_blocks_per_page + sizeof(q3_page_t));
		++self->m_page_count;

		page->next = self->m_pages;
		page->data = (q3_block_t*)Q3_PTR_ADD(page, sizeof(q3_page_t));
		self->m_pages = page;

		q3_i32 blocks_per_page_minus_one = self->m_blocks_per_page - 1;
		for (q3_i32 i = 0; i < blocks_per_page_minus_one; ++i) {
			q3_block_t *node = (q3_block_t *)Q3_PTR_ADD(page->data, self->m_block_size * i);
			q3_block_t *next = (q3_block_t *)Q3_PTR_ADD(page->data, self->m_block_size * (i + 1));
			node->next = next;
		}

		q3_block_t *last = (q3_block_t *)Q3_PTR_ADD(page->data, self->m_block_size * (blocks_per_page_minus_one));
		last->next = NULL;

		self->m_freelist = page->data->next;

		return page->data;
	}
}

void q3_paged_allocator_free(q3_paged_allocator_t *self, void* data) {
#ifdef _DEBUG
	bool found = false;

    q3_block_t *data_block = data;

	for (q3_page_t *page = self->m_pages; page; page = page->next) {
		if (data_block >= page->data && 
            data_block < (q3_block_t*)Q3_PTR_ADD(page->data, self->m_block_size * self->m_blocks_per_page)
) {
			found = true;
			break;
		}
	}

	// Address of data does not lie within any pages of this allocator.
	assert(found);
#endif // DEBUG

	((q3_block_t*)data)->next = self->m_freelist;
	self->m_freelist = ((q3_block_t*)data);
}

void q3_paged_allocator_clear(q3_paged_allocator_t *self) {
	q3_page_t* page = self->m_pages;

	for (q3_i32 i = 0; i < self->m_page_count; ++i) {
		q3_page_t* next = page->next;
		q3_free(page);
		page = next;
	}

	self->m_freelist = NULL;
	self->m_page_count = 0;
}

//--------------------------------------------------------------------------------------------------
// q3_vec3_t
//--------------------------------------------------------------------------------------------------

void q3_vec3_setall(q3_vec3_t *self, q3_r32 a) {
    self->x = a;
    self->y = a;
    self->z = a;
}

q3_vec3_t q3_v3add(q3_vec3_t left, q3_vec3_t right) {
    return (q3_vec3_t){
        .x = left.x + right.x,
        .y = left.y + right.y,
        .z = left.z + right.z,
    };
}

q3_vec3_t q3_v3addf(q3_vec3_t vec, q3_r32 f) {
    return (q3_vec3_t){
        .x = vec.x + f,
        .y = vec.y + f,
        .z = vec.z + f,
    };
}

q3_vec3_t q3_v3sub(q3_vec3_t left, q3_vec3_t right) {
    return (q3_vec3_t){
        .x = left.x - right.x,
        .y = left.y - right.y,
        .z = left.z - right.z,
    };
}

q3_vec3_t q3_v3subf(q3_vec3_t vec, q3_r32 f) {
    return (q3_vec3_t){
        .x = vec.x - f,
        .y = vec.y - f,
        .z = vec.z - f,
    };
}

q3_vec3_t q3_v3mulf(q3_vec3_t vec, q3_r32 f) {
    return (q3_vec3_t){
        .x = vec.x * f,
        .y = vec.y * f,
        .z = vec.z * f,
    };
}

q3_vec3_t q3_v3divf(q3_vec3_t vec, q3_r32 f) {
    return (q3_vec3_t){
        .x = vec.x / f,
        .y = vec.y / f,
        .z = vec.z / f,
    };
}

q3_vec3_t q3_v3neg(q3_vec3_t vec) {
    return (q3_vec3_t){
        .x = -vec.x,
        .y = -vec.y,
        .z = -vec.z,
    };
}

//--------------------------------------------------------------------------------------------------
// q3_mat3_t
//--------------------------------------------------------------------------------------------------

q3_mat3_t q3_mat3_init(q3_r32 a, q3_r32 b, q3_r32 c, q3_r32 d, q3_r32 e, q3_r32 f, q3_r32 g, q3_r32 h, q3_r32 i) {
    return (q3_mat3_t){
        .ex = { a, b, c },
        .ey = { d, e, f },
        .ez = { g, h, i },
    };
}

q3_mat3_t q3_mat3_from_rows(q3_vec3_t x, q3_vec3_t y, q3_vec3_t z) {
    return (q3_mat3_t){
        .ex = x,
        .ey = y,
        .ez = z,
    };
}

void q3_mat3_set(q3_mat3_t *self, q3_r32 a, q3_r32 b, q3_r32 c, q3_r32 d, q3_r32 e, q3_r32 f, q3_r32 g, q3_r32 h, q3_r32 i) {
    self->ex = (q3_vec3_t){ a, b, c };
    self->ey = (q3_vec3_t){ d, e, f };
    self->ez = (q3_vec3_t){ g, h, i };
}

void q3_mat3_set_from_angle(q3_mat3_t *self, q3_vec3_t axis, q3_r32 angle) {
	q3_r32 s = sinf(angle);
	q3_r32 c = cosf(angle);
	q3_r32 x = axis.x;
	q3_r32 y = axis.y;
	q3_r32 z = axis.z;
	q3_r32 xy = x * y;
	q3_r32 yz = y * z;
	q3_r32 zx = z * x;
	q3_r32 t = 1.f - c;

	q3_mat3_set(
        self,
		x * x * t + c, xy * t + z * s, zx * t - y * s,
		xy * t - z * s, y * y * t + c, yz * t + x * s,
		zx * t + y * s, yz * t - x * s, z * z * t + c
);
}

void q3_mat3_setrows(q3_mat3_t *self, q3_vec3_t x, q3_vec3_t y, q3_vec3_t z) {
    self->ex = x;
    self->ey = y;
    self->ez = z;
}

q3_mat3_t q3_m3mul(q3_mat3_t *left, q3_mat3_t *right) {
    return (q3_mat3_t) {
        q3_m3mulv3(left, right->ex), 
        q3_m3mulv3(left, right->ey), 
        q3_m3mulv3(left, right->ez), 
    };
}

q3_mat3_t q3_m3mulf(q3_mat3_t *m, q3_r32 f) {
    return (q3_mat3_t) {
        .ex = q3_v3mulf(m->ex, f),
        .ey = q3_v3mulf(m->ey, f),
        .ez = q3_v3mulf(m->ez, f),
    };
}

q3_vec3_t q3_m3mulv3(q3_mat3_t *self, q3_vec3_t v) {
    return (q3_vec3_t){
		self->ex.x * v.x + self->ey.x * v.y + self->ez.x * v.z,
		self->ex.y * v.x + self->ey.y * v.y + self->ez.y * v.z,
		self->ex.z * v.x + self->ey.z * v.y + self->ez.z * v.z
    };
}

q3_mat3_t q3_m3add(q3_mat3_t *left, q3_mat3_t *right) {
    return (q3_mat3_t) {
        .ex = q3_v3add(left->ex, right->ex),
        .ey = q3_v3add(left->ey, right->ey),
        .ez = q3_v3add(left->ez, right->ez),
    };
}

q3_mat3_t q3_m3sub(q3_mat3_t *left, q3_mat3_t *right) {
    return (q3_mat3_t) {
        .ex = q3_v3sub(left->ex, right->ex),
        .ey = q3_v3sub(left->ey, right->ey),
        .ez = q3_v3sub(left->ez, right->ez),
    };
}

q3_vec3_t q3_mat3_get(q3_mat3_t *self, q3_u32 index) {
    switch (index) {
        case 0: return self->ex;
        case 1: return self->ey;
        case 2: return self->ez;
        default:
            assert(false);
            return self->ex;
    }
}

q3_vec3_t q3_mat3_column0(q3_mat3_t *self) {
    return (q3_vec3_t){ self->ex.x, self->ey.x, self->ez.x };

}

q3_vec3_t q3_mat3_column1(q3_mat3_t *self) {
    return (q3_vec3_t){ self->ex.y, self->ey.y, self->ez.y };
}

q3_vec3_t q3_mat3_column2(q3_mat3_t *self) {
    return (q3_vec3_t){ self->ex.z, self->ey.z, self->ez.z };
}

//--------------------------------------------------------------------------------------------------
// q3Quaterion
//--------------------------------------------------------------------------------------------------

q3_quat_t q3_quat_from_angle(q3_vec3_t axis, q3_r32 radians) {
    q3_quat_t out = {0};
    q3_quat_set(&out, axis, radians);
    return out;
}

void q3_quat_set(q3_quat_t *self, q3_vec3_t axis, q3_r32 radians) {
    q3_r32 half_angle = radians * 0.5f;
	q3_r32 s = sinf(half_angle);
	self->x = s * axis.x;
	self->y = s * axis.y;
	self->z = s * axis.z;
	self->w = cosf(half_angle);
}

void q3_quat_to_axis_angle(q3_quat_t self, q3_vec3_t* axis, q3_r32* angle) {
	assert(self.w <= 1.f);

	*angle = 2.f * acosf(self.w);

	q3_r32 l = sqrtf(1.f - self.w * self.w);

	if (l == 0.f) {
        *axis = (q3_vec3_t){0};
	}
	else {
		l = 1.f / l;
        *axis = (q3_vec3_t){self.x * l, self.y * l, self.z * l};
	}
}

void q3_quat_integrate(q3_quat_t *self, q3_vec3_t dv, q3_r32 dt) {
    q3_quat_t q = { dv.x * dt, dv.y * dt, dv.z * dt, 0.f };

    q = q3_qmul(q, *self);

	self->x += q.x * 0.5f;
	self->y += q.y * 0.5f;
	self->z += q.z * 0.5f;
	self->w += q.w * 0.5f;

    *self = q3_qnorm(*self);
}

q3_quat_t q3_qmul(q3_quat_t left, q3_quat_t right) {
    return (q3_quat_t){
		left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
		left.w * right.y + left.y * right.w + left.z * right.x - left.x * right.z,
		left.w * right.z + left.z * right.w + left.x * right.y - left.y * right.x,
		left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z
    };
}

q3_mat3_t q3_quat_to_mat3(q3_quat_t quat) {
	q3_r32 qx2 = quat.x + quat.x;
	q3_r32 qy2 = quat.y + quat.y;
	q3_r32 qz2 = quat.z + quat.z;
	q3_r32 qxqx2 = quat.x * qx2;
	q3_r32 qxqy2 = quat.x * qy2;
	q3_r32 qxqz2 = quat.x * qz2;
	q3_r32 qxqw2 = quat.w * qx2;
	q3_r32 qyqy2 = quat.y * qy2;
	q3_r32 qyqz2 = quat.y * qz2;
	q3_r32 qyqw2 = quat.w * qy2;
	q3_r32 qzqz2 = quat.z * qz2;
	q3_r32 qzqw2 = quat.w * qz2;


    return (q3_mat3_t){
        .ex = { 1.f - qyqy2 - qzqz2, qxqy2 + qzqw2, qxqz2 - qyqw2 },
        .ey = { qxqy2 - qzqw2, 1.f - qxqx2 - qzqz2, qyqz2 + qxqw2 },
        .ez = { qxqz2 + qyqw2, qyqz2 - qxqw2, 1.f - qxqx2 - qyqy2 },
    };
}

//--------------------------------------------------------------------------------------------------
// q3_box_t
//--------------------------------------------------------------------------------------------------

bool q3_box_test_point(q3_box_t *self, q3_transform_t *tx, q3_vec3_t p) {
	q3_transform_t world = q3_tmul(tx, &self->local);
	q3_vec3_t p0 = q3_tmulv3_trans(&world, p);

	for (int i = 0; i < 3; ++i) {
		q3_r32 d  = p0.v[i];
		q3_r32 ei = self->e.v[i];

		if (d > ei || d < -ei) {
			return false;
		}
	}

	return true;
}

bool q3_box_raycast(q3_box_t *self, q3_transform_t *tx, q3_raycast_data_t* raycast) {
	q3_transform_t world = q3_tmul(tx, &self->local);
	q3_vec3_t d = q3_m3mulv3_trans(&world.rotation, raycast->dir);
	q3_vec3_t p = q3_tmulv3_trans(&world, raycast->start);
	const q3_r32 epsilon = 1.0e-8f;
	q3_r32 tmin = 0;
	q3_r32 tmax = raycast->t;

	// t = (e[i] - p.[i]) / d[i]
	q3_r32 t0;
	q3_r32 t1;
	q3_vec3_t n0;

	for (int i = 0; i < 3; ++i) {
		// Check for ray parallel to and outside of AABB
		if (q3_abs(d.v[i]) < epsilon) {
			// Detect separating axes
			if (p.v[i] < -self->e.v[i] || p.v[i] > self->e.v[i]) {
				return false;
			}
		}
		else {
			q3_r32 d0 = 1.f / d.v[i];
			q3_r32 s = q3_sign(d.v[i]);
			q3_r32 ei = self->e.v[i] * s;
			q3_vec3_t n = {0};
			n.v[i] = -s;

			t0 = -(ei + p.v[i]) * d0;
			t1 = (ei - p.v[i]) * d0;

			if (t0 > tmin) {
				n0 = n;
				tmin = t0;
			}

			tmax = q3_rmin(tmax, t1);

			if (tmin > tmax) {
				return false;
			}
		}
	}

	raycast->normal = q3_m3mulv3(&world.rotation, n0); 
	raycast->toi = tmin;

	return true;
}

void q3_box_compute_aabb(q3_box_t *self, q3_transform_t *tx, q3_aabb_t* aabb) {
	q3_transform_t world = q3_tmul(tx, &self->local);

	q3_vec3_t v[8] = {
        { -self->e.x, -self->e.y, -self->e.z },
        { -self->e.x, -self->e.y,  self->e.z },
        { -self->e.x,  self->e.y, -self->e.z },
        { -self->e.x,  self->e.y,  self->e.z },
        {  self->e.x, -self->e.y, -self->e.z },
        {  self->e.x, -self->e.y,  self->e.z },
        {  self->e.x,  self->e.y, -self->e.z },
        {  self->e.x,  self->e.y,  self->e.z }
	};

	for (q3_i32 i = 0; i < 8; ++i) {
		v[i] = q3_tmulv3(&world, v[i]);
    }

	q3_vec3_t min = { Q3_R32_MAX, Q3_R32_MAX, Q3_R32_MAX };
	q3_vec3_t max = { -Q3_R32_MAX, -Q3_R32_MAX, -Q3_R32_MAX };

	for (q3_i32 i = 0; i < 8; ++i) {
		min = q3_v3min(min, v[i]);
		max = q3_v3max(max, v[i]);
	}

	aabb->min = min;
	aabb->max = max;
}

void q3_box_compute_mass(q3_box_t *self, q3_mass_data_t* md) {
	// Calculate inertia tensor
	q3_r32 ex2  = 4.f * self->e.x * self->e.x;
	q3_r32 ey2  = 4.f * self->e.y * self->e.y;
	q3_r32 ez2  = 4.f * self->e.z * self->e.z;
	q3_r32 mass = 8.f * self->e.x * self->e.y * self->e.z * self->density;
	q3_r32 x    = (1.f / 12.f) * mass * (ey2 + ez2);
	q3_r32 y    = (1.f / 12.f) * mass * (ex2 + ez2);
	q3_r32 z    = (1.f / 12.f) * mass * (ex2 + ey2);
	q3_mat3_t I = q3_diagonal(x, y, z);

	// Transform tensor to local space
    q3_mat3_t r = q3_transpose(&self->local.rotation);
    I = q3_m3mul(&self->local.rotation, &I);
    I = q3_m3mul(&I, &r);
	q3_mat3_t identity = q3_m3identity();
    
	// I += (identity * dot(local.position, local.position) - outer_product(local.position, local.position)) * mass;
    q3_mat3_t result = q3_m3mulf(&identity, q3_dot(self->local.position, self->local.position));
    q3_mat3_t outer = q3_outer_product(self->local.position, self->local.position);
    result = q3_m3sub(&result, &outer);
    result = q3_m3mulf(&result, mass);
    I = q3_m3add(&I, &result);

	md->center = self->local.position;
	md->inertia = I;
	md->mass = mass;
}

//--------------------------------------------------------------------------------------------------
const q3_i32 q3_box_indices[36] = {
	1 - 1, 7 - 1, 5 - 1,
	1 - 1, 3 - 1, 7 - 1,
	1 - 1, 4 - 1, 3 - 1,
	1 - 1, 2 - 1, 4 - 1,
	3 - 1, 8 - 1, 7 - 1,
	3 - 1, 4 - 1, 8 - 1,
	5 - 1, 7 - 1, 8 - 1,
	5 - 1, 8 - 1, 6 - 1,
	1 - 1, 5 - 1, 6 - 1,
	1 - 1, 6 - 1, 2 - 1,
	2 - 1, 6 - 1, 8 - 1,
	2 - 1, 8 - 1, 4 - 1
};

//--------------------------------------------------------------------------------------------------

void q3_box_render(q3_box_t *self, q3_transform_t *tx, bool awake, q3_render_t* render) {
	q3_transform_t world = q3_tmul(tx, &self->local);

	q3_vec3_t vertices[8] = {
		{ -self->e.x, -self->e.y, -self->e.z },
		{ -self->e.x, -self->e.y,  self->e.z },
		{ -self->e.x,  self->e.y, -self->e.z },
		{ -self->e.x,  self->e.y,  self->e.z },
		{  self->e.x, -self->e.y, -self->e.z },
		{  self->e.x, -self->e.y,  self->e.z },
		{  self->e.x,  self->e.y, -self->e.z },
		{  self->e.x,  self->e.y,  self->e.z }
	};

	for (q3_i32 i = 0; i < 36; i += 3) {
		q3_vec3_t a = q3_tmulv3(&world, vertices[q3_box_indices[i]]);
		q3_vec3_t b = q3_tmulv3(&world, vertices[q3_box_indices[i + 1]]);
		q3_vec3_t c = q3_tmulv3(&world, vertices[q3_box_indices[i + 2]]);

		q3_vec3_t n = q3_norm(q3_cross(q3_v3sub(b, a), q3_v3sub(c, a)));

        render->set_tri_normal(n.x, n.y, n.z);
		render->triangle(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
	}
}


//--------------------------------------------------------------------------------------------------
// q3_box_t_def
//--------------------------------------------------------------------------------------------------

q3_boxdef_t q3_boxdef_init(void) {
    return (q3_boxdef_t){
        .m_friction = 0.4f,
        .m_restitution = 0.2f,
        .m_density = 1.f,
        .m_sensor = false,
    };
}

void q3_boxdef_set(q3_boxdef_t *self, q3_transform_t *tx, q3_vec3_t extents) {
    self->m_tx = *tx;
    self->m_e = q3_v3mulf(extents, 0.5f);
}

//--------------------------------------------------------------------------------------------------
// q3Contact
//--------------------------------------------------------------------------------------------------

void q3_manifold_set_pair(q3_manifold_t *self, q3_box_t *a, q3_box_t *b) {
    self->A = a;
    self->B = b;
    self->sensor = a->sensor || b->sensor;
}

void q3_contact_constraint_solve_collision(q3_contact_constraint_t *self) {
	self->manifold.contact_count = 0;

    q3_box_to_box(&self->manifold, self->A, self->B);

	if (self->manifold.contact_count > 0) {
		if (self->m_flags & Q3_COLLIDING) {
			self->m_flags |= Q3_WAS_COLLIDING;
        }
		else {
			self->m_flags |= Q3_COLLIDING;
        }
	}
	else {
		if (self->m_flags & Q3_COLLIDING) {
			self->m_flags &= ~Q3_COLLIDING;
			self->m_flags |= Q3_WAS_COLLIDING;
		}
		else {
			self->m_flags &= ~Q3_WAS_COLLIDING;
        }
	}
}

//--------------------------------------------------------------------------------------------------
// q_box_to_box
//--------------------------------------------------------------------------------------------------

inline bool q3__track_face_axis(
    q3_i32* axis, 
    q3_i32 n, 
    q3_r32 s, 
    q3_r32* s_max, 
    q3_vec3_t normal, 
    q3_vec3_t *axis_normal
) {
	if (s > 0.f) {
		return true;
    }

	if (s > *s_max) {
		*s_max = s;
		*axis = n;
		*axis_normal = normal;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
inline bool q3__track_edge_axis(
    q3_i32* axis, 
    q3_i32 n, 
    q3_r32 s, 
    q3_r32* s_max, 
    q3_vec3_t normal, 
    q3_vec3_t *axis_normal
) {
	if (s > 0.f) {
		return true;
    }

	q3_r32 l = 1.f / q3_length(normal);
	s *= l;

	if (s > *s_max) {
		*s_max = s;
		*axis = n;
		*axis_normal = q3_v3mulf(normal, l);
	}

	return false;
}

//--------------------------------------------------------------------------------------------------

typedef struct q3__clip_vertex_t q3__clip_vertex_t;
struct q3__clip_vertex_t {
	q3_vec3_t v;
	q3_feature_pair_t f;
};

q3__clip_vertex_t q3__clip_vertex_init(void) {
    return (q3__clip_vertex_t) {
        .f.key = ~0,
    };
}

//--------------------------------------------------------------------------------------------------

void q3__compute_reference_edges_and_basis(
    q3_vec3_t e_r, 
    q3_transform_t *rtx, 
    q3_vec3_t n, 
    q3_i32 axis, 
    q3_u8* out, 
    q3_mat3_t* basis, 
    q3_vec3_t* e
) {
	n = q3_m3mulv3_trans(&rtx->rotation, n);

	if (axis >= 3) {
		axis -= 3;
    }

	switch (axis) {
        case 0:
            if (n.x > 0.f) {
                out[0] = 1;
                out[1] = 8;
                out[2] = 7;
                out[3] = 9;

                *e = (q3_vec3_t){ e_r.y, e_r.z, e_r.x };
                q3_mat3_setrows(basis, rtx->rotation.ey, rtx->rotation.ez, rtx->rotation.ex);
            }
            else {
                out[0] = 11;
                out[1] = 3;
                out[2] = 10;
                out[3] = 5;

                *e = (q3_vec3_t){ e_r.z, e_r.y, e_r.x };
                q3_mat3_setrows(basis, rtx->rotation.ez, rtx->rotation.ey, q3_v3neg(rtx->rotation.ex));
            }
            break;

        case 1:
            if (n.y > 0.f) {
                out[0] = 0;
                out[1] = 1;
                out[2] = 2;
                out[3] = 3;

                *e = (q3_vec3_t){ e_r.z, e_r.x, e_r.y };
                q3_mat3_setrows(basis, rtx->rotation.ez, rtx->rotation.ex, rtx->rotation.ey);
            }
            else {
                out[0] = 4;
                out[1] = 5;
                out[2] = 6;
                out[3] = 7;

                *e = (q3_vec3_t){ e_r.z, e_r.x, e_r.y };
                q3_mat3_setrows(basis, rtx->rotation.ez, q3_v3neg(rtx->rotation.ex), q3_v3neg(rtx->rotation.ey));
            }
            break;

        case 2:
            if (n.z > 0.f) {
                out[0] = 11;
                out[1] = 4;
                out[2] = 8;
                out[3] = 0;

                *e = (q3_vec3_t){ e_r.y, e_r.x, e_r.z };
                q3_mat3_setrows(basis, q3_v3neg(rtx->rotation.ey), rtx->rotation.ex, rtx->rotation.ez);
            }
            else {
                out[0] = 6;
                out[1] = 10;
                out[2] = 2;
                out[3] = 9;

                *e = (q3_vec3_t){ e_r.y, e_r.x, e_r.z };
                q3_mat3_setrows(basis, q3_v3neg(rtx->rotation.ey), q3_v3neg(rtx->rotation.ex), q3_v3neg(rtx->rotation.ez));
            }
            break;
	}
}

//--------------------------------------------------------------------------------------------------
void q3__compute_incident_face(
    q3_transform_t *itx, 
    q3_vec3_t e, 
    q3_vec3_t n, 
    q3__clip_vertex_t *out
) {
	n = q3_v3neg(q3_m3mulv3_trans(&itx->rotation, n));
	q3_vec3_t abs_n = q3_v3abs(n);

	if (abs_n.x > abs_n.y && abs_n.x > abs_n.z) {
		if (n.x > 0.f) {
			out[0].v = (q3_vec3_t){ e.x,  e.y, -e.z };
			out[1].v = (q3_vec3_t){ e.x,  e.y,  e.z };
			out[2].v = (q3_vec3_t){ e.x, -e.y,  e.z };
			out[3].v = (q3_vec3_t){ e.x, -e.y, -e.z };

			out[0].f.in_i = 9;
			out[0].f.out_i = 1;
			out[1].f.in_i = 1;
			out[1].f.out_i = 8;
			out[2].f.in_i = 8;
			out[2].f.out_i = 7;
			out[3].f.in_i = 7;
			out[3].f.out_i = 9;
		}
		else {
			out[0].v = (q3_vec3_t){ -e.x, -e.y,  e.z };
			out[1].v = (q3_vec3_t){ -e.x,  e.y,  e.z };
			out[2].v = (q3_vec3_t){ -e.x,  e.y, -e.z };
			out[3].v = (q3_vec3_t){ -e.x, -e.y, -e.z };

			out[0].f.in_i = 5;
			out[0].f.out_i = 11;
			out[1].f.in_i = 11;
			out[1].f.out_i = 3;
			out[2].f.in_i = 3;
			out[2].f.out_i = 10;
			out[3].f.in_i = 10;
			out[3].f.out_i = 5;
		}
	}
	else if (abs_n.y > abs_n.x && abs_n.y > abs_n.z) {
		if (n.y > 0.f) {
			out[0].v = (q3_vec3_t){ -e.x,  e.y,  e.z };
			out[1].v = (q3_vec3_t){ e.x,  e.y,  e.z };
			out[2].v = (q3_vec3_t){ e.x,  e.y, -e.z };
			out[3].v = (q3_vec3_t){ -e.x,  e.y, -e.z };

			out[0].f.in_i = 3;
			out[0].f.out_i = 0;
			out[1].f.in_i = 0;
			out[1].f.out_i = 1;
			out[2].f.in_i = 1;
			out[2].f.out_i = 2;
			out[3].f.in_i = 2;
			out[3].f.out_i = 3;
		}
		else {
			out[0].v = (q3_vec3_t){ e.x, -e.y,  e.z };
			out[1].v = (q3_vec3_t){ -e.x, -e.y,  e.z };
			out[2].v = (q3_vec3_t){ -e.x, -e.y, -e.z };
			out[3].v = (q3_vec3_t){ e.x, -e.y, -e.z };

			out[0].f.in_i = 7;
			out[0].f.out_i = 4;
			out[1].f.in_i = 4;
			out[1].f.out_i = 5;
			out[2].f.in_i = 5;
			out[2].f.out_i = 6;
			out[3].f.in_i = 5;
			out[3].f.out_i = 6;
		}
	}
	else {
		if (n.z > 0.f) {
			out[0].v = (q3_vec3_t){ -e.x,  e.y,  e.z };
			out[1].v = (q3_vec3_t){ -e.x, -e.y,  e.z };
			out[2].v = (q3_vec3_t){ e.x, -e.y,  e.z };
			out[3].v = (q3_vec3_t){ e.x,  e.y,  e.z };

			out[0].f.in_i = 0;
			out[0].f.out_i = 11;
			out[1].f.in_i = 11;
			out[1].f.out_i = 4;
			out[2].f.in_i = 4;
			out[2].f.out_i = 8;
			out[3].f.in_i = 8;
			out[3].f.out_i = 0;
		}
		else {
			out[0].v = (q3_vec3_t){ e.x, -e.y, -e.z };
			out[1].v = (q3_vec3_t){ -e.x, -e.y, -e.z };
			out[2].v = (q3_vec3_t){ -e.x,  e.y, -e.z };
			out[3].v = (q3_vec3_t){ e.x,  e.y, -e.z };

			out[0].f.in_i = 9;
			out[0].f.out_i = 6;
			out[1].f.in_i = 6;
			out[1].f.out_i = 10;
			out[2].f.in_i = 10;
			out[2].f.out_i = 2;
			out[3].f.in_i = 2;
			out[3].f.out_i = 9;
        }
	}

	for (q3_i32 i = 0; i < 4; ++i) {
		out[i].v = q3_tmulv3(itx, out[i].v);
    }
}

//--------------------------------------------------------------------------------------------------
#define in_front(a) ((a) < 0.f)
#define behind(a)   ((a) >= 0.f)
#define on(a)       ((a) < 0.005f && (a) > -0.005f)

q3_i32 q3__orthographic(
    q3_r32 sign, 
    q3_r32 e,
    q3_i32 axis,
    q3_i32 clip_edge,
    q3__clip_vertex_t* in,
    q3_i32 in_count,
    q3__clip_vertex_t* out
) {
	q3_i32 out_count = 0;
	q3__clip_vertex_t a = in[in_count - 1];

	for (q3_i32 i = 0; i < in_count; ++i)
	{
		q3__clip_vertex_t b = in[i];

		q3_r32 da = sign * a.v.v[axis] - e;
		q3_r32 db = sign * b.v.v[axis] - e;

		q3__clip_vertex_t cv;

		// B
		if (((in_front(da) && in_front(db)) || on(da) || on(db))) {
			assert(out_count < 8);
			out[out_count++] = b;
		}

		// I
		else if (in_front(da) && behind(db)) {
			cv.f = b.f;
			// cv.v = a.v + (b.v - a.v) * (da / (da - db));
			cv.v = q3_v3add(a.v, q3_v3mulf(q3_v3sub(b.v, a.v), (da / (da - db))));
			cv.f.out_r = clip_edge;
			cv.f.out_i = 0;
			assert(out_count < 8);
			out[out_count++] = cv;
		}

		// I, B
		else if (behind(da) && in_front(db))
		{
			cv.f = a.f;
			// cv.v = a.v + (b.v - a.v) * (da / (da - db));
			cv.v = q3_v3add(a.v, q3_v3mulf(q3_v3sub(b.v, a.v), (da / (da - db))));
			cv.f.in_r = clip_edge;
			cv.f.in_i = 0;
			assert(out_count < 8);
			out[out_count++] = cv;

			assert(out_count < 8);
			out[out_count++] = b;
		}

		a = b;
	}

	return out_count;
}

//--------------------------------------------------------------------------------------------------
// Resources (also see q3_box_tto_box's resources):
// http://www.randygaul.net/2013/10/27/sutherland-hodgman-clipping/
q3_i32 q3__clip(
    q3_vec3_t r_pos,
    q3_vec3_t e,
    q3_u8* clip_edges,
    q3_mat3_t *basis,
    q3__clip_vertex_t* incident,
    q3__clip_vertex_t* out_verts,
    q3_r32* out_depths
) {
	q3_i32 in_count = 4;
	q3_i32 out_count;
	q3__clip_vertex_t in[8];
	q3__clip_vertex_t out[8];

	for (q3_i32 i = 0; i < 4; ++i) {
		in[i].v = q3_m3mulv3_trans(basis, q3_v3sub(incident[i].v, r_pos));
    }

	out_count = q3__orthographic(1.f, e.x, 0, clip_edges[0], in, in_count, out);

	if (!out_count) {
		return 0;
    }

	in_count = q3__orthographic(1.f, e.y, 1, clip_edges[1], out, out_count, in);

	if (!in_count) {
		return 0;
    }

	out_count = q3__orthographic(-1.f, e.x, 0, clip_edges[2], in, in_count, out);

	if (!out_count) {
		return 0;
    }

	in_count = q3__orthographic(-1.f, e.y, 1, clip_edges[3], out, out_count, in);

	// Keep incident vertices behind the reference face
	out_count = 0;
	for (q3_i32 i = 0; i < in_count; ++i) {
		q3_r32 d = in[i].v.z - e.z;

		if (d <= 0.f) {
			out_verts[out_count].v = q3_v3add(q3_m3mulv3(basis, in[i].v), r_pos);
			out_verts[out_count].f = in[i].f;
			out_depths[out_count++] = d;
		}
	}

	assert(out_count <= 8);

	return out_count;
}


//--------------------------------------------------------------------------------------------------
inline void q3__edges_contact(
    q3_vec3_t *CA,
    q3_vec3_t *CB,
    q3_vec3_t PA,
    q3_vec3_t QA,
    q3_vec3_t PB,
    q3_vec3_t QB
) {
	q3_vec3_t DA = q3_v3sub(QA, PA);
	q3_vec3_t DB = q3_v3sub(QB, PB);
	q3_vec3_t r  = q3_v3sub(PA, PB);
	q3_r32 a = q3_dot(DA, DA);
	q3_r32 e = q3_dot(DB, DB);
	q3_r32 f = q3_dot(DB, r);
	q3_r32 c = q3_dot(DA, r);

	q3_r32 b = q3_dot(DA, DB);
	q3_r32 denom = a * e - b * b;

	q3_r32 TA = (b * f - c * e) / denom;
	q3_r32 TB = (b * TA + f) / e;

	*CA = q3_v3add(PA, q3_v3mulf(DA, TA));
	*CB = q3_v3add(PB, q3_v3mulf(DB, TB));
}

//--------------------------------------------------------------------------------------------------
void q3__support_edge(
    q3_transform_t *tx,
    q3_vec3_t e,
    q3_vec3_t n,
    q3_vec3_t* a_out,
    q3_vec3_t* b_out
) {
	n = q3_m3mulv3_trans(&tx->rotation, n);
	q3_vec3_t abs_n = q3_v3abs(n);
	q3_vec3_t a, b;

	// x > y
	if (abs_n.x > abs_n.y) {
		// x > y > z
		if (abs_n.y > abs_n.z) {
			a = (q3_vec3_t){ e.x, e.y, e.z };
			b = (q3_vec3_t){ e.x, e.y, -e.z };
		}
		// x > z > y || z > x > y
		else {
			a = (q3_vec3_t){ e.x, e.y, e.z };
			b = (q3_vec3_t){ e.x, -e.y, e.z };
		}
	}
	// y > x
	else {
		// y > x > z
		if (abs_n.x > abs_n.z) {
			a = (q3_vec3_t){ e.x, e.y, e.z };
			b = (q3_vec3_t){ e.x, e.y, -e.z };
		}
		// z > y > x || y > z > x
		else {
			a = (q3_vec3_t){ e.x, e.y, e.z };
			b = (q3_vec3_t){ -e.x, e.y, e.z };
		}
	}

	q3_r32 signx = q3_sign(n.x);
	q3_r32 signy = q3_sign(n.y);
	q3_r32 signz = q3_sign(n.z);

	a.x *= signx;
	a.y *= signy;
	a.z *= signz;
	b.x *= signx;
	b.y *= signy;
	b.z *= signz;

	*a_out = q3_tmulv3(tx, a);
	*b_out = q3_tmulv3(tx, b);
}

//--------------------------------------------------------------------------------------------------
// Resources:
// http://www.randygaul.net/2014/05/22/deriving-obb-to-obb-intersection-sat/
// https://box2d.googlecode.com/files/GDC2007_Erin_catto.zip
// https://box2d.googlecode.com/files/Box2D_Lite.zip
void q3_box_to_box(q3_manifold_t* m, q3_box_t* a, q3_box_t* b) {
	q3_transform_t atx = a->body->m_tx;
	q3_transform_t btx = b->body->m_tx;
	q3_transform_t a_l = a->local;
	q3_transform_t b_l = b->local;
	atx = q3_tmul(&atx, &a_l);
	btx = q3_tmul(&btx, &b_l);
	q3_vec3_t e_a = a->e;
	q3_vec3_t e_b = b->e;

	// B's frame in A's space
    q3_mat3_t C = q3_transpose(&atx.rotation);
	C = q3_m3mul(&C, &btx.rotation);

	q3_mat3_t abs_c;
	bool parallel = false;
	const q3_r32 k_cos_tol = 1.0e-6f;
	for (q3_i32 i = 0; i < 3; ++i) {
		for (q3_i32 j = 0; j < 3; ++j) {
			q3_r32 val = q3_abs(C.v[i].v[j]);
            abs_c.v[i].v[j] = val;

			if (val + k_cos_tol >= 1.f) {
				parallel = true;
            }
		}
	}

	// Vector from center A to center B in A's space
	q3_vec3_t t = q3_m3mulv3_trans(&atx.rotation, q3_v3sub(btx.position, atx.position));

	// Query states
	q3_r32 s;
	q3_r32 a_max = -Q3_R32_MAX;
	q3_r32 b_max = -Q3_R32_MAX;
	q3_r32 e_max = -Q3_R32_MAX;
	q3_i32 a_axis = ~0;
	q3_i32 b_axis = ~0;
	q3_i32 e_axis = ~0;
	q3_vec3_t n_a;
	q3_vec3_t n_b;
	q3_vec3_t n_e;

	// Face axis checks

	// a's x axis
	s = q3_abs(t.x) - (e_a.x + q3_dot(q3_mat3_column0(&abs_c), e_b));
	if (q3__track_face_axis(&a_axis, 0, s, &a_max, atx.rotation.ex, &n_a)) {
		return;
    }

	// a's y axis
	s = q3_abs(t.y) - (e_a.y + q3_dot(q3_mat3_column1(&abs_c), e_b));
	if (q3__track_face_axis(&a_axis, 1, s, &a_max, atx.rotation.ey, &n_a)) {
		return;
    }

	// a's z axis
	s = q3_abs(t.z) - (e_a.z + q3_dot(q3_mat3_column2(&abs_c), e_b));
	if (q3__track_face_axis(&a_axis, 2, s, &a_max, atx.rotation.ez, &n_a)) {
		return;
    }

	// b's x axis
	s = q3_abs(q3_dot(t, C.ex)) - (e_b.x + q3_dot(abs_c.ex, e_a));
	if (q3__track_face_axis(&b_axis, 3, s, &b_max, btx.rotation.ex, &n_b)) {
		return;
    }

	// b's y axis
	s = q3_abs(q3_dot(t, C.ey)) - (e_b.y + q3_dot(abs_c.ey, e_a));
	if (q3__track_face_axis(&b_axis, 4, s, &b_max, btx.rotation.ey, &n_b)) {
		return;
    }

	// b's z axis
	s = q3_abs(q3_dot(t, C.ez)) - (e_b.z + q3_dot(abs_c.ez, e_a));
	if (q3__track_face_axis(&b_axis, 5, s, &b_max, btx.rotation.ez, &n_b)) {
		return;
    }

	if (!parallel) {
		// Edge axis checks
		q3_r32 r_a;
		q3_r32 r_b;

		// Cross(a.x, b.x)
		r_a = e_a.y * abs_c.v[0].v[2] + e_a.z * abs_c.v[0].v[1];
		r_b = e_b.y * abs_c.v[2].v[0] + e_b.z * abs_c.v[1].v[0];
		s = q3_abs(t.z * C.v[0].v[1] - t.y * C.v[0].v[2]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 6, s, &e_max, (q3_vec3_t){ 0.f, -C.v[0].v[2], C.v[0].v[1] }, &n_e)) {
			return;
        }

		// Cross(a.x, b.y)
		r_a = e_a.y * abs_c.v[1].v[2] + e_a.z * abs_c.v[1].v[1];
		r_b = e_b.x * abs_c.v[2].v[0] + e_b.z * abs_c.v[0].v[0];
		s = q3_abs(t.z * C.v[1].v[1] - t.y * C.v[1].v[2]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 7, s, &e_max, (q3_vec3_t){ 0.f, -C.v[1].v[2], C.v[1].v[1] }, &n_e)) {
			return;
        }

		// Cross(a.x, b.z)
		r_a = e_a.y * abs_c.v[2].v[2] + e_a.z * abs_c.v[2].v[1];
		r_b = e_b.x * abs_c.v[1].v[0] + e_b.y * abs_c.v[0].v[0];
		s = q3_abs(t.z * C.v[2].v[1] - t.y * C.v[2].v[2]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 8, s, &e_max, (q3_vec3_t){ 0.f, -C.v[2].v[2], C.v[2].v[1] }, &n_e)) {
			return;
        }

		// Cross(a.y, b.x)
		r_a = e_a.x * abs_c.v[0].v[2] + e_a.z * abs_c.v[0].v[0];
		r_b = e_b.y * abs_c.v[2].v[1] + e_b.z * abs_c.v[1].v[1];
		s = q3_abs(t.x * C.v[0].v[2] - t.z * C.v[0].v[0]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 9, s, &e_max, (q3_vec3_t){ C.v[0].v[2], 0.f, -C.v[0].v[0] }, &n_e)) {
			return;
        }

		// Cross(a.y, b.y)
		r_a = e_a.x * abs_c.v[1].v[2] + e_a.z * abs_c.v[1].v[0];
		r_b = e_b.x * abs_c.v[2].v[1] + e_b.z * abs_c.v[0].v[1];
		s = q3_abs(t.x * C.v[1].v[2] - t.z * C.v[1].v[0]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 10, s, &e_max, (q3_vec3_t){ C.v[1].v[2], 0.f, -C.v[1].v[0] }, &n_e)) {
			return;
        }

		// Cross(a.y, b.z)
		r_a = e_a.x * abs_c.v[2].v[2] + e_a.z * abs_c.v[2].v[0];
		r_b = e_b.x * abs_c.v[1].v[1] + e_b.y * abs_c.v[0].v[1];
		s = q3_abs(t.x * C.v[2].v[2] - t.z * C.v[2].v[0]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 11, s, &e_max, (q3_vec3_t){ C.v[2].v[2], 0.f, -C.v[2].v[0] }, &n_e)) {
			return;
        }

		// Cross(a.z, b.x)
		r_a = e_a.x * abs_c.v[0].v[1] + e_a.y * abs_c.v[0].v[0];
		r_b = e_b.y * abs_c.v[2].v[2] + e_b.z * abs_c.v[1].v[2];
		s = q3_abs(t.y * C.v[0].v[0] - t.x * C.v[0].v[1]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 12, s, &e_max, (q3_vec3_t){ -C.v[0].v[1], C.v[0].v[0], 0.f }, &n_e)) {
			return;
        }

		// Cross(a.z, b.y)
		r_a = e_a.x * abs_c.v[1].v[1] + e_a.y * abs_c.v[1].v[0];
		r_b = e_b.x * abs_c.v[2].v[2] + e_b.z * abs_c.v[0].v[2];
		s = q3_abs(t.y * C.v[1].v[0] - t.x * C.v[1].v[1]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 13, s, &e_max, (q3_vec3_t){ -C.v[1].v[1], C.v[1].v[0], 0.f }, &n_e)) {
			return;
        }

		// Cross(a.z, b.z)
		r_a = e_a.x * abs_c.v[2].v[1] + e_a.y * abs_c.v[2].v[0];
		r_b = e_b.x * abs_c.v[1].v[2] + e_b.y * abs_c.v[0].v[2];
		s = q3_abs(t.y * C.v[2].v[0] - t.x * C.v[2].v[1]) - (r_a + r_b);
		if (q3__track_edge_axis(&e_axis, 14, s, &e_max, (q3_vec3_t){ -C.v[2].v[1], C.v[2].v[0], 0.f }, &n_e)) {
			return;
        }
	}

	// Artificial axis bias to improve frame coherence
	const q3_r32 k_rel_tol = 0.95f;
	const q3_r32 k_abs_tol = 0.01f;
	q3_i32 axis;
	q3_r32 s_max;
	q3_vec3_t n;
	q3_r32 face_max = q3_rmax(a_max, b_max);
	if (k_rel_tol * e_max > face_max + k_abs_tol) {
		axis = e_axis;
		s_max = e_max;
		n = n_e;
	}
	else {
		if (k_rel_tol * b_max > a_max + k_abs_tol) {
			axis = b_axis;
			s_max = b_max;
			n = n_b;
		}
		else {
			axis = a_axis;
			s_max = a_max;
			n = n_a;
		}
	}

	if (q3_dot(n, q3_v3sub(btx.position, atx.position)) < 0.f) {
		n = q3_v3neg(n);
    }

	if (axis == ~0) {
		return;
    }

	if (axis < 6) {
		q3_transform_t rtx;
		q3_transform_t itx;
		q3_vec3_t e_r;
		q3_vec3_t e_i;
		bool flip;

		if (axis < 3) {
			rtx = atx;
			itx = btx;
			e_r = e_a;
			e_i = e_b;
			flip = false;
		}
		else {
			rtx = btx;
			itx = atx;
			e_r = e_b;
			e_i = e_a;
			flip = true;
            n = q3_v3neg(n);
		}

		// Compute reference and incident edge information necessary for clipping
		q3__clip_vertex_t incident[4];
		q3__compute_incident_face(&itx, e_i, n, incident);
		q3_u8 clip_edges[4];
		q3_mat3_t basis;
		q3_vec3_t e;
		q3__compute_reference_edges_and_basis(e_r, &rtx, n, axis, clip_edges, &basis, &e);

		// Clip the incident face against the reference face side planes
		q3__clip_vertex_t out[8];
		q3_r32 depths[8];
		q3_i32 out_num;
		out_num = q3__clip(rtx.position, e, clip_edges, &basis, incident, out, depths);

		if (out_num) {
			m->contact_count = out_num;
			m->normal = flip ? q3_v3neg(n) : n;

			for (q3_i32 i = 0; i < out_num; ++i) {
				q3_contact_t* c = m->contacts + i;

				q3_feature_pair_t pair = out[i].f;

				if (flip) {
                    q3_uswap(&pair.in_i, &pair.in_r);
                    q3_uswap(&pair.out_i, &pair.out_r);
				}

				c->fp = out[i].f;
				c->position = out[i].v;
				c->penetration = depths[i];
			}
		}
	}
	else {
		n = q3_m3mulv3(&atx.rotation, n);

		if (q3_dot(n, q3_v3sub(btx.position, atx.position)) < 0.f) {
            n = q3_v3neg(n);
        }

		q3_vec3_t PA, QA;
		q3_vec3_t PB, QB;
		q3__support_edge(&atx, e_a, n, &PA, &QA);
		q3__support_edge(&btx, e_b, q3_v3neg(n), &PB, &QB);

		q3_vec3_t CA, CB;
		q3__edges_contact(&CA, &CB, PA, QA, PB, QB);

		m->normal = n;
		m->contact_count = 1;

		q3_contact_t* c = m->contacts;
		q3_feature_pair_t pair;
		pair.key = axis;
		c->fp = pair;
		c->penetration = s_max;
		c->position = q3_v3mulf(q3_v3add(CA, CB), 0.5f);
	}
}

// m_flags
typedef enum
{
    Q3_BODY_FLAGS_AWAKE        = 0x001,
    Q3_BODY_FLAGS_ACTIVE       = 0x002,
    Q3_BODY_FLAGS_ALLOW_SLEEP  = 0x004,
    Q3_BODY_FLAGS_ISLAND       = 0x010,
    Q3_BODY_FLAGS_STATIC       = 0x020,
    Q3_BODY_FLAGS_DYNAMIC      = 0x040,
    Q3_BODY_FLAGS_KINEMATIC    = 0x080,
    Q3_BODY_FLAGS_LOCK_AXIS_X  = 0x100,
    Q3_BODY_FLAGS_LOCK_AXIS_Y  = 0x200,
    Q3_BODY_FLAGS_LOCK_AXIS_Z  = 0x400,
} q3_body_flags_e;

q3_body_t q3__body_init(q3_bodydef_t *def, q3_scene_t* scene) {
    q3_body_t out = {
        .m_linear_velocity  = def->linear_velocity,
        .m_angular_velocity = def->angular_velocity,
        .m_q                = q3_quat_from_angle(q3_norm(def->axis), def->angle),
        .m_tx.position      = def->position,
        .m_gravity_scale    = def->gravity_scale,
        .m_layers           = def->layers,
        .m_user_data        = def->user_data,
        .m_scene            = scene,
        .m_linear_damping   = def->linear_damping,
        .m_angular_damping  = def->angular_damping,
    };

    out.m_tx.rotation = q3_quat_to_mat3(out.m_q);

	if (def->body_type == Q3_BODY_DYNAMIC) {
		out.m_flags |= Q3_BODY_FLAGS_DYNAMIC;
    }
	else {
		if (def->body_type == Q3_BODY_STATIC) {
			out.m_flags |= Q3_BODY_FLAGS_STATIC;
			out.m_linear_velocity = q3_v3identity();
			out.m_angular_velocity = q3_v3identity();
			out.m_force = q3_v3identity();
			out.m_torque = q3_v3identity();
		}
		else if (def->body_type == Q3_BODY_KINEMATIC) {
			out.m_flags |= Q3_BODY_FLAGS_KINEMATIC;
        }
	}

	if (def->allow_sleep) {
		out.m_flags |= Q3_BODY_FLAGS_ALLOW_SLEEP;
    }

	if (def->awake) {
		out.m_flags |= Q3_BODY_FLAGS_AWAKE;
    }

	if (def->active) {
		out.m_flags |= Q3_BODY_FLAGS_ACTIVE;
    }

	if (def->lock_axis_x) {
		out.m_flags |= Q3_BODY_FLAGS_LOCK_AXIS_X;
    }

	if (def->lock_axis_y) {
		out.m_flags |= Q3_BODY_FLAGS_LOCK_AXIS_Y;
    }

	if (def->lock_axis_z) {
		out.m_flags |= Q3_BODY_FLAGS_LOCK_AXIS_Z;
    }

    return out;
}

void q3__body_calculate_mass_data(q3_body_t *self) {
	q3_mat3_t inertia = q3_diagonalf(0.f);
	self->m_inv_inertia_model = q3_diagonalf(0.f);
	self->m_inv_inertia_world = q3_diagonalf(0.f);
	self->m_inv_mass = 0.f;
	self->m_mass = 0.f;
	q3_r32 mass = 0.f;

	if (self->m_flags & Q3_BODY_FLAGS_STATIC || self->m_flags & Q3_BODY_FLAGS_KINEMATIC) {
		self->m_local_center = q3_v3identity();
		self->m_world_center = self->m_tx.position;
		return;
	}

	q3_vec3_t lc = {0};

	for (q3_box_t* box = self->m_boxes; box; box = box->next) {
		if (box->density == 0.f) {
			continue;
        }

		q3_mass_data_t md;
        q3_box_compute_mass(box, &md);
		mass += md.mass;
        inertia = q3_m3add(&inertia, &md.inertia);
        lc = q3_v3add(lc, q3_v3mulf(md.center, md.mass));
	}

	if (mass > 0.f) {
		self->m_mass = mass;
		self->m_inv_mass = 1.f / mass;
        lc = q3_v3mulf(lc, self->m_inv_mass);
		q3_mat3_t identity = q3_m3identity();

		// inertia -= (identity * q3_dot(lc, lc) - q3_outer_product(lc, lc)) * mass;

        identity = q3_m3mulf(&identity, q3_dot(lc, lc));
        q3_mat3_t tmp = q3_outer_product(lc, lc);
        identity = q3_m3sub(&identity, &tmp);
        identity = q3_m3mulf(&identity, mass);
        
        inertia = q3_m3sub(&inertia, &identity);

		self->m_inv_inertia_model = q3_inverse(&inertia);

		if (self->m_flags & Q3_BODY_FLAGS_LOCK_AXIS_X) {
			self->m_inv_inertia_model.ex = q3_v3identity();
        }

		if (self->m_flags & Q3_BODY_FLAGS_LOCK_AXIS_Y) {
			self->m_inv_inertia_model.ey = q3_v3identity();
        }

		if (self->m_flags & Q3_BODY_FLAGS_LOCK_AXIS_Z) {
			self->m_inv_inertia_model.ez = q3_v3identity();
        }
	}
	else {
		// Force all dynamic bodies to have some mass
		self->m_inv_mass = 1.f;
		self->m_inv_inertia_model = q3_diagonalf(0.f);
		self->m_inv_inertia_world = q3_diagonalf(0.f);
	}

	self->m_local_center = lc;
	self->m_world_center = q3_tmulv3(&self->m_tx, lc);
}

void q3__body_synchronize_proxies(q3_body_t *self) {
	q3_broad_phase_t* broadphase = &self->m_scene->m_contact_manager.m_broadphase;

    self->m_tx.position = q3_v3sub(self->m_world_center, q3_m3mulv3(&self->m_tx.rotation, self->m_local_center));

	q3_aabb_t aabb;
	q3_transform_t tx = self->m_tx;

	q3_box_t* box = self->m_boxes;
	while (box) {
        q3_box_compute_aabb(box, &tx, &aabb);
        q3_broad_phase_update(broadphase, box->broad_phase_index, &aabb);
		box = box->next;
	}
}

q3_box_t* q3_body_add_box(q3_body_t *self, q3_boxdef_t *def) {
	q3_aabb_t aabb;
	q3_box_t* box = q3_heap_allocate(&self->m_scene->m_heap, sizeof(q3_box_t));
	box->local = def->m_tx;
	box->e = def->m_e;
	box->next = self->m_boxes;
	self->m_boxes = box;
    q3_box_compute_aabb(box, &self->m_tx, &aabb);

	box->body = self;
	box->friction = def->m_friction;
	box->restitution = def->m_restitution;
	box->density = def->m_density;
	box->sensor = def->m_sensor;

    q3__body_calculate_mass_data(self);

    q3_broad_phase_insert_box(&self->m_scene->m_contact_manager.m_broadphase, box, &aabb);
	self->m_scene->m_new_box = true;

	return box;
}

void q3_body_remove_box(q3_body_t *self, q3_box_t *box) {
	assert(box);
	assert(box->body == self);

	q3_box_t* node = self->m_boxes;

	bool found = false;
	if (node == box) {
		self->m_boxes = node->next;
		found = true;
	}
	else {
		while (node) {
			if (node->next == box) {
				node->next = box->next;
				found = true;
				break;
			}

			node = node->next;
		}
	}

	// This shape was not connected to this body.
	assert(found);

	// Remove all contacts associated with this shape
	q3_contact_edge_t* edge = self->m_contact_list;
	while (edge) {
		q3_contact_constraint_t* contact = edge->constraint;
		edge = edge->next;

		q3_box_t* A = contact->A;
		q3_box_t* B = contact->B;

		if (box == A || box == B) {
            q3_contact_manager_remove_contact(&self->m_scene->m_contact_manager, contact);
        }
	}

    q3_broad_phase_remove_box(&self->m_scene->m_contact_manager.m_broadphase, box);

    q3__body_calculate_mass_data(self);

    q3_heap_free(&self->m_scene->m_heap, box);
}

void q3_body_remove_all_boxes(q3_body_t *self) {
	while (self->m_boxes) {
		q3_box_t* next = self->m_boxes->next;

        q3_broad_phase_remove_box(&self->m_scene->m_contact_manager.m_broadphase, self->m_boxes);
        q3_heap_free(&self->m_scene->m_heap, self->m_boxes);

		self->m_boxes = next;
	}

    q3_contact_manager_remove_contacts_from_body(&self->m_scene->m_contact_manager, self);
}

void q3_body_apply_linear_force(q3_body_t *self, q3_vec3_t force) {
	self->m_force = q3_v3add(self->m_force, q3_v3mulf(force, self->m_mass));

    q3_body_set_to_awake(self);
}

void q3_body_apply_force_at_world_point(q3_body_t *self, q3_vec3_t force, q3_vec3_t point) {
	self->m_force = q3_v3add(self->m_force, q3_v3mulf(force, self->m_mass));
    self->m_torque = q3_v3add(self->m_torque, q3_cross(q3_v3sub(point, self->m_world_center), force));

    q3_body_set_to_awake(self);
}

void q3_body_apply_linear_impulse(q3_body_t *self, q3_vec3_t impulse) {
	self->m_linear_velocity = q3_v3add(self->m_linear_velocity, q3_v3mulf(impulse, self->m_inv_mass));

    q3_body_set_to_awake(self);
}

void q3_body_apply_linear_impulse_at_world_point(q3_body_t *self, q3_vec3_t impulse, q3_vec3_t point) {
	self->m_linear_velocity = q3_v3add(self->m_linear_velocity, q3_v3mulf(impulse, self->m_inv_mass));
    self->m_angular_velocity = q3_v3add(self->m_angular_velocity, q3_cross(q3_v3sub(point, self->m_world_center), impulse));

    q3_body_set_to_awake(self);
}

void q3_body_apply_torque(q3_body_t *self, q3_vec3_t torque) {
    self->m_torque = q3_v3add(self->m_torque, torque);
}

void q3_body_set_to_awake(q3_body_t *self) {
	if(!(self->m_flags & Q3_BODY_FLAGS_AWAKE))
	{
		self->m_flags |= Q3_BODY_FLAGS_AWAKE;
		self->m_sleep_time = 0.f;
	}
}

void q3_body_set_to_sleep(q3_body_t *self) {
	self->m_flags &= ~Q3_BODY_FLAGS_AWAKE;
	self->m_sleep_time = 0.f;
	self->m_linear_velocity  = q3_v3identity();
	self->m_angular_velocity = q3_v3identity();
	self->m_force            = q3_v3identity();
	self->m_torque           = q3_v3identity();
}

bool q3_body_is_awake(q3_body_t *self) {
	return self->m_flags & Q3_BODY_FLAGS_AWAKE ? true : false;
}

q3_vec3_t q3_body_get_local_point(q3_body_t *self, q3_vec3_t p) {
    return q3_tmulv3_trans(&self->m_tx, p);
}

q3_vec3_t q3_body_get_local_vector(q3_body_t *self, q3_vec3_t v) {
	return q3_m3mulv3_trans(&self->m_tx.rotation, v);
}

q3_vec3_t q3_body_get_world_point(q3_body_t *self, q3_vec3_t p) {
    return q3_tmulv3(&self->m_tx, p);
}

q3_vec3_t q3_body_get_world_vector(q3_body_t *self, q3_vec3_t v) {
	return q3_m3mulv3(&self->m_tx.rotation, v);
}

q3_vec3_t q3_body_get_velocity_at_world_point(q3_body_t *self, q3_vec3_t p) {
	q3_vec3_t direction_to_point = q3_v3sub(p, self->m_world_center);
	q3_vec3_t relative_angular_vel = q3_cross(self->m_angular_velocity, direction_to_point);

	return q3_v3add(self->m_linear_velocity, relative_angular_vel);
}

void q3_body_set_linear_velocity(q3_body_t *self, q3_vec3_t v) {
	// Velocity of static bodies cannot be adjusted
	if (self->m_flags & Q3_BODY_FLAGS_STATIC) {
		assert(false);
    }

	if (q3_dot(v, v) > 0.f) {
        q3_body_set_to_awake(self);
	}

	self->m_linear_velocity = v;
}

void q3_body_set_angular_velocity(q3_body_t *self, q3_vec3_t v) {
	// Velocity of static bodies cannot be adjusted
	if (self->m_flags & Q3_BODY_FLAGS_STATIC) {
		assert(false);
    }

	if (q3_dot(v, v) > 0.f) {
        q3_body_set_to_awake(self);
	}

	self->m_angular_velocity = v;
}

bool q3_body_can_collide(q3_body_t *self, q3_body_t *other) {
	if (self == other) {
		return false;
    }

	// Every collision must have at least one dynamic body involved
	if (!(self->m_flags & Q3_BODY_FLAGS_DYNAMIC) && !(other->m_flags & Q3_BODY_FLAGS_DYNAMIC)) {
		return false;
    }

	if (!(self->m_layers & other->m_layers)) {
		return false;
    }

	return true;
}

void q3_body_set_transform(q3_body_t *self, q3_vec3_t position) {
	self->m_world_center = position;

    q3__body_synchronize_proxies(self);
}

void q3_body_set_transform_angle(q3_body_t *self, q3_vec3_t position, q3_vec3_t axis, q3_r32 angle) {
	self->m_world_center = position;
    q3_quat_set(&self->m_q, axis, angle);
	self->m_tx.rotation = q3_quat_to_mat3(self->m_q);

    q3__body_synchronize_proxies(self);
}

void q3_body_render(q3_body_t *self, q3_render_t *render) {
	bool awake = q3_body_is_awake(self);
	q3_box_t* box = self->m_boxes;

	while (box) {
        q3_box_render(box, &self->m_tx, awake, render);
		box = box->next;
	}
}

void q3_body_dump(q3_body_t *self, FILE *file, q3_i32 index) {
	fprintf(file, "{\n");
	fprintf(file, "\tq3Body_def bd;\n");

	switch (self->m_flags & (Q3_BODY_FLAGS_STATIC | Q3_BODY_FLAGS_DYNAMIC | Q3_BODY_FLAGS_KINEMATIC)) {
        case Q3_BODY_FLAGS_STATIC:
            fprintf(file, "\tbd.body_type = q3Body_type(%d);\n", Q3_BODY_STATIC);
            break;

        case Q3_BODY_FLAGS_DYNAMIC:
            fprintf(file, "\tbd.body_type = q3Body_type(%d);\n", Q3_BODY_DYNAMIC);
            break;

        case Q3_BODY_FLAGS_KINEMATIC:
            fprintf(file, "\tbd.body_type = q3Body_type(%d);\n", Q3_BODY_KINEMATIC);
            break;
	}

	fprintf(file, "\tbd.position.Set(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", self->m_tx.position.x, self->m_tx.position.y, self->m_tx.position.z);
	q3_vec3_t axis;
	q3_r32 angle;
    q3_quat_to_axis_angle(self->m_q, &axis, &angle);
	fprintf(file, "\tbd.axis.Set(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", axis.x, axis.y, axis.z);
	fprintf(file, "\tbd.angle = r32(%.15lf);\n", angle);
	fprintf(file, "\tbd.linear_velocity.Set(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", self->m_linear_velocity.x, self->m_linear_velocity.y, self->m_linear_velocity.z);
	fprintf(file, "\tbd.angular_velocity.Set(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", self->m_angular_velocity.x, self->m_angular_velocity.y, self->m_angular_velocity.z);
	fprintf(file, "\tbd.gravity_scale = r32(%.15lf);\n", self->m_gravity_scale);
	fprintf(file, "\tbd.layers = %d;\n", self->m_layers);
	fprintf(file, "\tbd.allow_sleep = bool(%d);\n", self->m_flags & Q3_BODY_FLAGS_ALLOW_SLEEP);
	fprintf(file, "\tbd.awake = bool(%d);\n", self->m_flags & Q3_BODY_FLAGS_AWAKE);
	fprintf(file, "\tbd.awake = bool(%d);\n", self->m_flags & Q3_BODY_FLAGS_AWAKE);
	fprintf(file, "\tbd.lock_axis_x = bool(%d);\n", self->m_flags & Q3_BODY_FLAGS_LOCK_AXIS_X);
	fprintf(file, "\tbd.lock_axis_y = bool(%d);\n", self->m_flags & Q3_BODY_FLAGS_LOCK_AXIS_Y);
	fprintf(file, "\tbd.lock_axis_z = bool(%d);\n", self->m_flags & Q3_BODY_FLAGS_LOCK_AXIS_Z);
	fprintf(file, "\tbodies[%d] = scene.Create_body(bd);\n\n", index);

	q3_box_t* box = self->m_boxes;

	while (box) {
		fprintf(file, "\t{\n");
		fprintf(file, "\t\tq3_box_t_def sd;\n");
		fprintf(file, "\t\tsd.Set_friction(r32(%.15lf));\n", box->friction);
		fprintf(file, "\t\tsd.Set_restitution(r32(%.15lf));\n", box->restitution);
		fprintf(file, "\t\tsd.Set_density(r32(%.15lf));\n", box->density);
		q3_i32 sensor = (int)box->sensor;
		fprintf(file, "\t\tsd.Set_sensor(bool(%d));\n", sensor);
		fprintf(file, "\t\tq3Transform box_tx;\n");
		q3_transform_t box_tx = box->local;
		q3_vec3_t x_axis = box_tx.rotation.ex;
		q3_vec3_t y_axis = box_tx.rotation.ey;
		q3_vec3_t z_axis = box_tx.rotation.ez;
		fprintf(file, "\t\tq3_vec3_t x_axis(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", x_axis.x, x_axis.y, x_axis.z);
		fprintf(file, "\t\tq3_vec3_t y_axis(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", y_axis.x, y_axis.y, y_axis.z);
		fprintf(file, "\t\tq3_vec3_t z_axis(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", z_axis.x, z_axis.y, z_axis.z);
		fprintf(file, "\t\tbox_tx.rotation.Set_rows(x_axis, y_axis, z_axis);\n");
		fprintf(file, "\t\tbox_tx.position.Set(r32(%.15lf), r32(%.15lf), r32(%.15lf));\n", box_tx.position.x, box_tx.position.y, box_tx.position.z);
		fprintf(file, "\t\tsd.Set(box_tx, q3_vec3_t(r32(%.15lf), r32(%.15lf), r32(%.15lf)));\n", box->e.x * 2.0f, box->e.y * 2.0f, box->e.z * 2.0f);
		fprintf(file, "\t\tbodies[%d]->Add_box(sd);\n", index);
		fprintf(file, "\t}\n");
		box = box->next;
	}

	fprintf(file, "}\n\n");
}

//--------------------------------------------------------------------------------------------------
// q3Half_space
//--------------------------------------------------------------------------------------------------

void q3_half_space_set3(q3_half_space_t *self, q3_vec3_t a, q3_vec3_t b, q3_vec3_t c) {
    self->normal = q3_norm(q3_cross(q3_v3sub(b, a), q3_v3sub(c, a)));
    self->distance = q3_dot(self->normal, a);
}

void q3_half_space_set2(q3_half_space_t *self, q3_vec3_t n, q3_vec3_t p) {
    self->normal = q3_norm(n);
    self->distance = q3_dot(self->normal, p);
}

q3_vec3_t q3_half_space_origin(q3_half_space_t *self) {
    return q3_v3mulf(self->normal, self->distance);
}

q3_r32 q3_half_space_distance(q3_half_space_t *self, q3_vec3_t p) {
    return q3_dot(self->normal, p) - self->distance;
}

q3_vec3_t q3_half_space_projected(q3_half_space_t *self, q3_vec3_t p) {
    return q3_v3sub(p, q3_v3mulf(self->normal, q3_half_space_distance(self, p)));
}

//--------------------------------------------------------------------------------------------------
// q3Dynamic_AABBTree
//--------------------------------------------------------------------------------------------------

bool q3__aabb_node_is_leaf(q3_aabb_node_t *self) {
    return self->right == Q3_AABB_NULL_NODE;
}

void q3__fatten_aabb(q3_aabb_t *aabb) {
	const q3_r32 k_fattener = 0.5f;
	aabb->min = q3_v3subf(aabb->min, k_fattener);
	aabb->max = q3_v3addf(aabb->max, k_fattener);
}

q3_i32 q3__dynamic_aabb_tree_allocate_node(q3_dynamic_aabb_tree_t *self) {
	if (self->m_freelist == Q3_AABB_NULL_NODE) {
		self->m_capacity *= 2;
		q3_aabb_node_t *new_nodes = q3_alloc(sizeof(q3_aabb_node_t) * self->m_capacity);
		memcpy(new_nodes, self->m_nodes, sizeof(q3_aabb_node_t) * self->m_count);
		q3_free(self->m_nodes);
		self->m_nodes = new_nodes;

		q3_dynamic_aabb_tree_add_to_freelist(self, self->m_count);
	}

	q3_i32 free_node = self->m_freelist;
	self->m_freelist = self->m_nodes[self->m_freelist].next;
	self->m_nodes[free_node].height = 0;
	self->m_nodes[free_node].left = Q3_AABB_NULL_NODE;
	self->m_nodes[free_node].right = Q3_AABB_NULL_NODE;
	self->m_nodes[free_node].parent = Q3_AABB_NULL_NODE;
	self->m_nodes[free_node].user_data = NULL;
	++self->m_count;
	return free_node;
}

q3_i32 q3__dynamic_aabb_tree_balance(q3_dynamic_aabb_tree_t *self, q3_i32 i_a) {
	q3_aabb_node_t *A = self->m_nodes + i_a;

	if (q3__aabb_node_is_leaf(A) || A->height == 1) {
		return i_a;
    }

	/*      A
	      /   \
	     B     C
	    / \   / \
	   D   E F   G
	*/

	q3_i32 i_b = A->left;
	q3_i32 i_c = A->right;
	q3_aabb_node_t *B = self->m_nodes + i_b;
	q3_aabb_node_t *C = self->m_nodes + i_c;

	q3_i32 balance = C->height - B->height;

	// C is higher, promote C
	if (balance > 1) {
		q3_i32 i_f = C->left;
		q3_i32 i_g = C->right;
		q3_aabb_node_t *F = self->m_nodes + i_f;
		q3_aabb_node_t *G = self->m_nodes + i_g;

		// grand_parent point to C
		if (A->parent != Q3_AABB_NULL_NODE) {
			if(self->m_nodes[A->parent].left == i_a) {
				self->m_nodes[A->parent].left = i_c;
            }
			else {
				self->m_nodes[A->parent].right = i_c;
            }
		}
		else {
			self->m_root = i_c;
        }

		// Swap A and C
		C->left = i_a;
		C->parent = A->parent;
		A->parent = i_c;

		// Finish rotation
		if (F->height > G->height) {
			C->right = i_f;
			A->right = i_g;
			G->parent = i_a;
			A->aabb = q3_aabb_combine(&B->aabb, &G->aabb);
			C->aabb = q3_aabb_combine(&A->aabb, &F->aabb);

			A->height = 1 + q3_rmax(B->height, G->height);
			C->height = 1 + q3_rmax(A->height, F->height);
		}
		else {
			C->right = i_g;
			A->right = i_f;
			F->parent = i_a;
			A->aabb = q3_aabb_combine(&B->aabb, &F->aabb);
			C->aabb = q3_aabb_combine(&A->aabb, &G->aabb);

			A->height = 1 + q3_rmax(B->height, F->height);
			C->height = 1 + q3_rmax(A->height, G->height);
		}

		return i_c;
	}
	// B is higher, promote B
	else if (balance < -1) {
		q3_i32 i_d = B->left;
		q3_i32 i_e = B->right;
		q3_aabb_node_t *D = self->m_nodes + i_d;
		q3_aabb_node_t *E = self->m_nodes + i_e;

		// grand_parent point to B
		if (A->parent != Q3_AABB_NULL_NODE) {
			if(self->m_nodes[A->parent].left == i_a) {
				self->m_nodes[A->parent].left = i_b;
            }
			else {
				self->m_nodes[A->parent].right = i_b;
            }
		}
		else {
			self->m_root = i_b;
        }

		// Swap A and B
		B->right = i_a;
		B->parent = A->parent;
		A->parent = i_b;

		// Finish rotation
		if (D->height > E->height) {
			B->left = i_d;
			A->left = i_e;
			E->parent = i_a;
			A->aabb = q3_aabb_combine(&C->aabb, &E->aabb);
			B->aabb = q3_aabb_combine(&A->aabb, &D->aabb);

			A->height = 1 + q3_rmax(C->height, E->height);
			B->height = 1 + q3_rmax(A->height, D->height);
		}
		else {
			B->left = i_e;
			A->left = i_d;
			D->parent = i_a;
			A->aabb = q3_aabb_combine(&C->aabb, &D->aabb);
			B->aabb = q3_aabb_combine(&A->aabb, &E->aabb);

			A->height = 1 + q3_rmax(C->height, D->height);
			B->height = 1 + q3_rmax(A->height, E->height);
		}

		return i_b;
	}

	return i_a;
}

// correct aabb hierarchy heights and aabbs starting at supplied
// index traversing up the heirarchy
void q3__dynamic_aabb_tree_sync_heirarchy(q3_dynamic_aabb_tree_t *self, q3_i32 index) {
	while (index != Q3_AABB_NULL_NODE)
	{
		index = q3__dynamic_aabb_tree_balance(self, index);

		q3_i32 left = self->m_nodes[index].left;
		q3_i32 right = self->m_nodes[index].right;

		self->m_nodes[index].height = 1 + q3_rmax(self->m_nodes[left].height, self->m_nodes[right].height);
		self->m_nodes[index].aabb = q3_aabb_combine(&self->m_nodes[left].aabb, &self->m_nodes[right].aabb);

		index = self->m_nodes[index].parent;
	}
}


void q3__dynamic_aabb_tree_insert_leaf(q3_dynamic_aabb_tree_t *self, q3_i32 id) {
	if (self->m_root == Q3_AABB_NULL_NODE) {
		self->m_root = id;
		self->m_nodes[self->m_root].parent = Q3_AABB_NULL_NODE;
		return;
	}

	// Search for sibling
	q3_i32 search_index = self->m_root;
	q3_aabb_t leaf_aabb = self->m_nodes[id].aabb;
	while (!q3__aabb_node_is_leaf(&self->m_nodes[search_index])) {
		// Cost for insertion at index (branch node), involves creation
		// of new branch to contain this index and the new leaf
		q3_aabb_t combined = q3_aabb_combine(&leaf_aabb, &self->m_nodes[search_index].aabb);
		q3_r32 combined_area = q3_aabb_surface_area(&combined);
		q3_r32 branch_cost = 2.f * combined_area;

		// inherited cost (surface area growth from heirarchy update after descent)
		q3_r32 inherited_cost = 2.f * (combined_area - q3_aabb_surface_area(&self->m_nodes[search_index].aabb));

		q3_i32 left = self->m_nodes[search_index].left;
		q3_i32 right = self->m_nodes[search_index].right;

		// calculate costs for left/right descents. if traversal is to a leaf,
		// then the cost of the combind aabb represents a new branch node. otherwise
		// the cost is only the inflation of the pre-existing branch.
		q3_r32 left_descent_cost;
		if (q3__aabb_node_is_leaf(&self->m_nodes[left])) {
            q3_aabb_t tmp = q3_aabb_combine(&leaf_aabb, &self->m_nodes[left].aabb);
			left_descent_cost = q3_aabb_surface_area(&tmp) + inherited_cost;
        }
		else {
            q3_aabb_t tmp = q3_aabb_combine(&leaf_aabb, &self->m_nodes[left].aabb);
			q3_r32 inflated = q3_aabb_surface_area(&tmp);
			q3_r32 branch_area = q3_aabb_surface_area(&self->m_nodes[left].aabb);
			left_descent_cost = inflated - branch_area + inherited_cost;
		}

		// cost for right descent
		q3_r32 right_descent_cost;
		if (q3__aabb_node_is_leaf(&self->m_nodes[right])) {
            q3_aabb_t tmp = q3_aabb_combine(&leaf_aabb, &self->m_nodes[right].aabb);
			right_descent_cost = q3_aabb_surface_area(&tmp) + inherited_cost;
        }
		else {
            q3_aabb_t tmp = q3_aabb_combine(&leaf_aabb, &self->m_nodes[right].aabb);
			q3_r32 inflated = q3_aabb_surface_area(&tmp);
			q3_r32 branch_area = q3_aabb_surface_area(&self->m_nodes[right].aabb);
			right_descent_cost = inflated - branch_area + inherited_cost;
		}

		// determine traversal direction, or early out on a branch index
		if (branch_cost < left_descent_cost && branch_cost < right_descent_cost) {
			break;
        }

		if (left_descent_cost < right_descent_cost) {
			search_index = left;
        }
		else {
			search_index = right;
        }
	}

	q3_i32 sibling = search_index;

	// Create new parent
	q3_i32 old_parent = self->m_nodes[sibling].parent;
	q3_i32 new_parent = q3__dynamic_aabb_tree_allocate_node(self);
	self->m_nodes[new_parent].parent = old_parent;
	self->m_nodes[new_parent].user_data = NULL;
	self->m_nodes[new_parent].aabb = q3_aabb_combine(&leaf_aabb, &self->m_nodes[sibling].aabb);
	self->m_nodes[new_parent].height = self->m_nodes[sibling].height + 1;

	// Sibling was root
	if (old_parent == Q3_AABB_NULL_NODE) {
		self->m_nodes[new_parent].left = sibling;
		self->m_nodes[new_parent].right = id;
		self->m_nodes[sibling].parent = new_parent;
		self->m_nodes[id].parent = new_parent;
		self->m_root = new_parent;
	}
	else {
		if (self->m_nodes[old_parent].left == sibling) {
			self->m_nodes[old_parent].left = new_parent;
        }
		else {
			self->m_nodes[old_parent].right = new_parent;
        }

		self->m_nodes[new_parent].left = sibling;
		self->m_nodes[new_parent].right = id;
		self->m_nodes[sibling].parent = new_parent;
		self->m_nodes[id].parent = new_parent;
	}

	q3__dynamic_aabb_tree_sync_heirarchy(self, self->m_nodes[id].parent);
}

void q3__dynamic_aabb_tree_remove_leaf(q3_dynamic_aabb_tree_t *self, q3_i32 id) {
	if (id == self->m_root) {
		self->m_root = Q3_AABB_NULL_NODE;
		return;
	}

	// Setup parent, grand_parent and sibling
	q3_i32 parent = self->m_nodes[id].parent;
	q3_i32 grand_parent = self->m_nodes[parent].parent;
	q3_i32 sibling;

	if (self->m_nodes[parent].left == id) {
		sibling = self->m_nodes[parent].right;
    }
	else {
		sibling = self->m_nodes[parent].left;
    }

	// Remove parent and replace with sibling
	if (grand_parent != Q3_AABB_NULL_NODE) {
		// Connect grand_parent to sibling
		if (self->m_nodes[grand_parent].left == parent) {
			self->m_nodes[grand_parent].left = sibling;
        }
		else {
			self->m_nodes[grand_parent].right = sibling;
        }

		// Connect sibling to grand_parent
		self->m_nodes[sibling].parent = grand_parent;
	}
	// Parent was root
	else {
		self->m_root = sibling;
		self->m_nodes[sibling].parent = Q3_AABB_NULL_NODE;
	}

	q3_dynamic_aabb_tree_deallocate_node(self, parent);
	q3__dynamic_aabb_tree_sync_heirarchy(self, grand_parent);
}

void q3__dynamic_aabb_tree_validate_structure(q3_dynamic_aabb_tree_t *self, q3_i32 index) {
	q3_aabb_node_t *n = self->m_nodes + index;

	q3_i32 il = n->left;
	q3_i32 ir = n->right;

	if (q3__aabb_node_is_leaf(n)) {
		assert(ir == Q3_AABB_NULL_NODE);
		assert(n->height == 0);
		return;
	}

	assert(il >= 0 && il < self->m_capacity);
	assert(ir >= 0 && ir < self->m_capacity);
	q3_aabb_node_t *l = self->m_nodes + il;
	q3_aabb_node_t *r = self->m_nodes + ir;

	assert(l->parent == index);
	assert(r->parent == index);

	q3__dynamic_aabb_tree_validate_structure(self, il);
	q3__dynamic_aabb_tree_validate_structure(self, ir);
}

void q3__dynamic_aabb_tree_render_node(q3_dynamic_aabb_tree_t *self, q3_render_t *render, q3_i32 index) {
	assert(index >= 0 && index < self->m_capacity);

	q3_aabb_node_t *n = self->m_nodes + index;
	q3_aabb_t *b = &n->aabb;

	render->set_pen_position(b->min.x, b->max.y, b->min.z);

	render->line(b->min.x, b->max.y, b->max.z);
	render->line(b->max.x, b->max.y, b->max.z);
	render->line(b->max.x, b->max.y, b->min.z);
	render->line(b->min.x, b->max.y, b->min.z);

	render->set_pen_position(b->min.x, b->min.y, b->min.z);

	render->line(b->min.x, b->min.y, b->max.z);
	render->line(b->max.x, b->min.y, b->max.z);
	render->line(b->max.x, b->min.y, b->min.z);
	render->line(b->min.x, b->min.y, b->min.z);

	render->set_pen_position(b->min.x, b->min.y, b->min.z);
	render->line(b->min.x, b->max.y, b->min.z);
	render->set_pen_position(b->max.x, b->min.y, b->min.z);
	render->line(b->max.x, b->max.y, b->min.z);
	render->set_pen_position(b->max.x, b->min.y, b->max.z);
	render->line(b->max.x, b->max.y, b->max.z);
	render->set_pen_position(b->min.x, b->min.y, b->max.z);
	render->line(b->min.x, b->max.y, b->max.z);

	if (!q3__aabb_node_is_leaf(n)) {
		q3__dynamic_aabb_tree_render_node(self, render, n->left);
		q3__dynamic_aabb_tree_render_node(self, render, n->right);
	}
}

q3_dynamic_aabb_tree_t q3_dynamic_aabb_tree_init(void) {
    q3_dynamic_aabb_tree_t out = {0};
	out.m_root = Q3_AABB_NULL_NODE;

	out.m_capacity = 1024;
	out.m_count = 0;
	out.m_nodes = q3_alloc(sizeof(q3_aabb_node_t) * out.m_capacity);

	q3_dynamic_aabb_tree_add_to_freelist(&out, 0);
    return out;
}

void q3_dynamic_aabb_tree_destroy(q3_dynamic_aabb_tree_t *self) {
	q3_free(self->m_nodes);
}

// Provide tight-AABB
q3_i32 q3_dynamic_aabb_tree_insert(q3_dynamic_aabb_tree_t *self, q3_aabb_t *aabb, void *user_data) {
	q3_i32 id = q3__dynamic_aabb_tree_allocate_node(self);

	// Fatten AABB and set height/userdata
	self->m_nodes[id].aabb = *aabb;
	q3__fatten_aabb(&self->m_nodes[id].aabb);
	self->m_nodes[id].user_data = user_data;
	self->m_nodes[id].height = 0;

	q3__dynamic_aabb_tree_insert_leaf(self, id);

	return id;
}

void q3_dynamic_aabb_tree_remove(q3_dynamic_aabb_tree_t *self, q3_i32 id) {
	assert(id >= 0 && id < self->m_capacity);
	assert(q3__aabb_node_is_leaf(&self->m_nodes[id]));

	q3__dynamic_aabb_tree_remove_leaf(self, id);
	q3_dynamic_aabb_tree_deallocate_node(self, id);
}

bool q3_dynamic_aabb_tree_update(q3_dynamic_aabb_tree_t *self, q3_i32 id, q3_aabb_t *aabb) {
	assert(id >= 0 && id < self->m_capacity);
	assert(q3__aabb_node_is_leaf(&self->m_nodes[id]));

	if (q3_aabb_contains(&self->m_nodes[id].aabb, aabb)) {
		return false;
    }

	q3__dynamic_aabb_tree_remove_leaf(self, id);

	self->m_nodes[id].aabb = *aabb;
	q3__fatten_aabb(&self->m_nodes[id].aabb);

	q3__dynamic_aabb_tree_insert_leaf(self, id);

	return true;
}

void *q3_dynamic_aabb_tree_get_user_data(q3_dynamic_aabb_tree_t *self, q3_i32 id) {
	assert(id >= 0 && id < self->m_capacity);

	return self->m_nodes[id].user_data;
}

q3_aabb_t *q3_dynamic_aabb_tree_get_fat_aabb(q3_dynamic_aabb_tree_t *self, q3_i32 id) {
	assert(id >= 0 && id < self->m_capacity);

	return &self->m_nodes[id].aabb;
}

void q3_dynamic_aabb_tree_render(q3_dynamic_aabb_tree_t *self, q3_render_t *render) {
	if (self->m_root != Q3_AABB_NULL_NODE) {
		render->set_pen_color(0.5f, 0.5f, 1.0f, 1.f);
		q3__dynamic_aabb_tree_render_node(self, render, self->m_root);
	}
}

void q3_dynamic_aabb_tree_query_aabb(q3_dynamic_aabb_tree_t *self, void *cb, q3_aabb_t *aabb) {
#define k_stack_capacity 256
	q3_i32 stack[k_stack_capacity];
	q3_i32 sp = 1;

	*stack = self->m_root;

	while (sp) {
		// k_stack_capacity too small
		assert(sp < k_stack_capacity);

		q3_i32 id = stack[--sp];

		q3_aabb_node_t *n = self->m_nodes + id;
		if (q3_aabb_to_aabb(aabb, &n->aabb)) {
			if (q3__aabb_node_is_leaf(n)) {
                // TODO
				// if (!cb->tree_call_back(id)) {
				// 	return;
				// }
			}
			else {
				stack[sp++] = n->left;
				stack[sp++] = n->right;
			}
		}
	}
#undef k_stack_capacity
}

void q3_dynamic_aabb_tree_query_raycast(q3_dynamic_aabb_tree_t *self, void *cb, q3_raycast_data_t *ray_cast) {
#define k_stack_capacity 256
	const q3_r32 k_epsilon = 1.0e-6f;
	q3_i32 stack[k_stack_capacity];
	q3_i32 sp = 1;

	*stack = self->m_root;

	q3_vec3_t p0 = ray_cast->start;
	q3_vec3_t p1 = q3_v3add(p0, q3_v3mulf(ray_cast->dir, ray_cast->t));

	while(sp) {
		// k_stack_capacity too small
		assert(sp < k_stack_capacity);

		q3_i32 id = stack[--sp];

		if (id == Q3_AABB_NULL_NODE) {
			continue;
        }

		q3_aabb_node_t *n = self->m_nodes + id;

		q3_vec3_t e = q3_v3sub(n->aabb.max, n->aabb.min);
		q3_vec3_t d = q3_v3sub(p1, p0);
		q3_vec3_t m = q3_v3sub(q3_v3sub(q3_v3add(p0, p1), n->aabb.min), n->aabb.max);

		q3_r32 adx = q3_abs(d.x);

		if (q3_abs(m.x) > e.x + adx) {
			continue;
        }

		q3_r32 ady = q3_abs(d.y);

		if (q3_abs(m.y) > e.y + ady) {
			continue;
        }

		q3_r32 adz = q3_abs(d.z);

		if (q3_abs(m.z) > e.z + adz) {
			continue;
        }

		adx += k_epsilon;
		ady += k_epsilon;
		adz += k_epsilon;

		if(q3_abs(m.y * d.z - m.z * d.y) > e.y * adz + e.z * ady) {
			continue;
        }

		if(q3_abs(m.z * d.x - m.x * d.z) > e.x * adz + e.z * adx) {
			continue;
        }

		if (q3_abs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) {
			continue;
        }

		if (q3__aabb_node_is_leaf(n)) {
            // TODO
			// if (!cb->tree_call_back(id)) {
			// 	return;
			// }
		}
		else {
			stack[sp++] = n->left;
			stack[sp++] = n->right;
		}
	}
#undef k_stack_capacity
}

// for testing
void q3_dynamic_aabb_tree_validate(q3_dynamic_aabb_tree_t *self) {
	// Verify free list
	q3_i32 free_nodes = 0;
	q3_i32 index = self->m_freelist;

	while (index != Q3_AABB_NULL_NODE) {
		assert(index >= 0 && index < self->m_capacity);
		index = self->m_nodes[index].next;
		++free_nodes;
	}

	assert(self->m_count + free_nodes == self->m_capacity);

	// Validate tree structure
	if (self->m_root != Q3_AABB_NULL_NODE) {
		assert(self->m_nodes[self->m_root].parent == Q3_AABB_NULL_NODE);

#ifdef _DEBUG
		q3__dynamic_aabb_tree_validate_structure(self, self->m_root);
#endif
	}
}

