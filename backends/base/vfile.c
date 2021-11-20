// SPDX-License-Identifier: Apache-2.0

#define SYS_DEBUG // define to enable debug logging
#include "sys_impl.h"

#include <stdlib.h>
#define memrealloc(ptr,size) realloc(ptr,size)
#define memfree(ptr)         free(ptr)

#define VFILE_MAP_INITCAP  32  // initial capacity
#define VFILE_FD_MIN  0x40000000  // minimum fd value vfile_map_alloc will return


// hash map; file descriptor -> vfile
typedef struct vfile_map {
  u32      cap;
  u32      len;
  fd_t*    keys;
  vfile_t* vals;
} vfile_map_t;

// storage of open virtual files
static fd_t     g_keys_st[VFILE_MAP_INITCAP];
static vfile_t  g_vals_st[VFILE_MAP_INITCAP];
static vfile_map_t g_vfile_map = {
  .cap = VFILE_MAP_INITCAP,
  .len = 0,
  .keys = g_keys_st,
  .vals = g_vals_st,
};


// static void vfile_map_dump(vfile_map_t* m) {
//   fprintf(stderr, "[");
//   for (u32 i = 0; i < m->len; i++)
//     fprintf(stderr, i ? " %d:%d" : "%d:%d", m->keys[i], m->vals[i].fd);
//   fprintf(stderr, "]\n");
// }


static bool vfile_map_grow(vfile_map_t* m) {
  u32 cap = m->cap * 2;
  assert(cap > m->cap); // overflow check (also asserts that m->cap > 0)
  usize keysize = cap * sizeof(fd_t);
  usize valsize = cap * sizeof(vfile_t);
  fd_t* newkeys = memrealloc(m->keys == g_keys_st ? NULL : m->keys, keysize + valsize);
  if (!newkeys)
    return false;
  if (m->keys == g_keys_st) {
    memcpy(newkeys, m->keys, m->len * sizeof(fd_t));
    memcpy(&newkeys[cap], m->vals, m->len * sizeof(vfile_t));
  }
  m->keys = newkeys;
  m->vals = (vfile_t*)&newkeys[cap];
  m->cap = cap;
  return true;
}


// note: returned pointer is only valid until next call to a mutating vfile_map_ function
static vfile_t* vfile_map_get(vfile_map_t* m, fd_t key) {
  // binary search
  assert(m->len*2 >= m->len); // overflow check
  u32 low = 0;
  u32 high = m->len;
  while (low < high) {
    u32 i = (low + high) / 2;
    fd_t d = m->keys[i] - key;
    if (d < 0) {
      low = i + 1;
    } else if (d > 0) {
      high = i;
    } else {
      return &m->vals[i];
    }
  }
  return NULL;
}


// note: returned pointer is only valid until next call to a mutating vfile_map_ function
static vfile_t* vfile_map_set(vfile_map_t* m, fd_t key) {
  assert(m->len*2 >= m->len); // overflow check
  u32 low = 0;
  u32 high = m->len;
  fd_t d = 0;
  u32 i = 0;

  while (low < high) {
    i = (low + high) / 2;
    d = m->keys[i] - key;
    if (d < 0) {
      low = i + 1;
    } else if (d > 0) {
      high = i;
    } else {
      return &m->vals[i];
    }
  }

  if (m->len == m->cap) {
    if (!vfile_map_grow(m))
      return NULL;
  }

  if (d > 0) {
    // insert before i; shift higher
    // i=1 [1 2 3 4 _] => [1 2 2 3 4]
    //                         > > >
    for (u32 j = m->len; j > i; j--) {
      m->keys[j] = m->keys[j-1];
      m->vals[j] = m->vals[j-1];
    }
  } else if (d < 0) {
    // append
    i++;
  }

  m->len++;
  m->keys[i] = key;
  m->vals[i].fd = key;
  return &m->vals[i];
}


// note: returned pointer is only valid until next call to a mutating vfile_map_ function
static vfile_t* vfile_map_alloc(vfile_map_t* m) {
  if (m->len == m->cap) {
    if (!vfile_map_grow(m))
      return NULL;
  }
  fd_t key;
  if (m->len == 0) {
    key = VFILE_FD_MIN;
  } else {
    key = MAX(VFILE_FD_MIN, m->vals[m->len - 1].fd+1);
  }
  m->keys[m->len] = key;
  m->vals[m->len].fd = key;
  vfile_t* f = &m->vals[m->len++];
  return f;
}


static bool vfile_map_del(vfile_map_t* m, fd_t key) {
  u32 low = 0;
  u32 high = m->len;
  while (low < high) {
    u32 i = (low + high) / 2;
    fd_t d = m->keys[i] - key;
    if (d < 0) {
      low = i + 1;
    } else if (d > 0) {
      high = i;
    } else {
      m->len--;
      if (i < m->len) {
        // i=2 [1 2 3 4 5] => [1 2 4 5 5]
        //                         < <
        memcpy(&m->keys[i], &m->keys[i+1], (m->len - i) * sizeof(fd_t));
        memcpy(&m->vals[i], &m->vals[i+1], (m->len - i) * sizeof(vfile_t));
      }
      return true;
    }
  }
  return false;
}


// mini unit test for vfile_map
#ifdef SYS_DEBUG
__attribute__((constructor,used))
static void vfile_map_test() {
  vfile_map_t m = {
    .cap = VFILE_MAP_INITCAP,
    .len = 0,
    .keys = g_keys_st, // must use these specific globals b/c of addr cmp
    .vals = g_vals_st,
  };

  // dlog("add 2");
  vfile_t* f2 = vfile_map_set(&m, 2);
  assert(f2 != NULL);
  assert(f2 == vfile_map_get(&m, 2));

  // dlog("add 3");
  vfile_t* f3 = vfile_map_set(&m, 3);
  assert(f3 != NULL);
  assert(f3 != f2);
  assert(f3 == vfile_map_get(&m, 3));

  // dlog("add 5");
  vfile_t* f5 = vfile_map_set(&m, 5);
  assert(f5 != NULL);
  assert(f5 != f3);
  assert(f5 == vfile_map_get(&m, 5));
  assert(2 == vfile_map_get(&m, 2)->fd);
  assert(3 == vfile_map_get(&m, 3)->fd);

  // dlog("add 4");
  vfile_t* f4 = vfile_map_set(&m, 4);
  assert(f4 != NULL);
  assert(f4 == vfile_map_get(&m, 4));

  // dlog("set 4");
  vfile_t* f4b = vfile_map_set(&m, 4);
  assert(f4b != NULL);
  assert(f4b == f4);
  assert(f4b == vfile_map_get(&m, 4));

  // dlog("set 1");
  vfile_t* f1 = vfile_map_set(&m, 1);
  assert(f1 != NULL);
  assert(f1 == vfile_map_get(&m, 1));
  // vfile_map_dump(&m);
  assert(2 == vfile_map_get(&m, 2)->fd);
  assert(3 == vfile_map_get(&m, 3)->fd);
  assert(4 == vfile_map_get(&m, 4)->fd);
  assert(5 == vfile_map_get(&m, 5)->fd);

  // dlog("del 3");
  assert(vfile_map_del(&m, 3));
  assert(vfile_map_get(&m, 3) == NULL);

  // dlog("del 5");
  assert(vfile_map_del(&m, 5));
  assert(vfile_map_get(&m, 5) == NULL);

  // dlog("del 1");
  assert(vfile_map_del(&m, 1));
  assert(vfile_map_get(&m, 1) == NULL);

  // dlog("del 4");
  assert(vfile_map_del(&m, 4));
  assert(vfile_map_get(&m, 4) == NULL);

  // dlog("del 2");
  assert(vfile_map_del(&m, 2));
  assert(vfile_map_get(&m, 2) == NULL);
}
#endif


fd_t vfile_open(vfile_t** fp, vfile_flag_t flags) {
  vfile_t* f;
  fd_t fdv[2];

  // allocate a file descriptor
  if (flags & VFILE_PIPE) {
    // Note: we allocade a file descriptor from the OS to make sure we are
    // not "shadowing" another file descriptor.
    err_t e = _psys_pipe(0, fdv, 0); // fdv[0] = readable, fdv[1] writable
    if (e < 0)
      return e;
    f = vfile_map_set(&g_vfile_map, fdv[0]);
  } else {
    f = vfile_map_alloc(&g_vfile_map);
  }

  if (!f) {
    if (flags & VFILE_PIPE)
      _psys_close(0, fdv[0]);
    return p_err_nomem;
  }

  f->flags = flags;
  *fp = f;

  if (flags & VFILE_PIPE)
    return (fd_t)fdv[1];
  return f->fd;
}


vfile_t* vfile_lookup(fd_t fd) {
  return vfile_map_get(&g_vfile_map, fd);
}


err_t vfile_close(vfile_t* f) {
  err_t ret = 0;
  if (f->on_syscall)
    ret = MAX(ret, (err_t)f->on_syscall(p_sysop_close, 0, 0, 0, 0, 0, f));
  return vfile_map_del(&g_vfile_map, f->fd) ? ret : p_err_badfd;
}
