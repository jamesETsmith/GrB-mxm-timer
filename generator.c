#include "generator.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

#include "compat.h"
#include "globals.h"
#include "prng.h"

/* SCALE can be up to 40.
   Edge factor 16 takes 44 bits for indexing the list, leaving 20.
   Top 32 bits of an index will have at most 12 bits active.
*/

static inline int64_t mult_big_mod(const int64_t k, const uint64_t hi,
                                   const uint64_t low) {
  const uint64_t k_hi = ((uint64_t)k) >> 32; /* At most 12 bits. */
  const uint64_t k_low = ((uint64_t)k) & 0xFFFFFFFFul;
  uint64_t out = (k_low * low) % NE;
  if (k_hi) {
    uint64_t t = ((((k_hi * low) << 16) % NE) << 16) % NE;
    out = (out + t) % NE;
  }
  if (hi) {
    uint64_t t = ((((hi * k_low) << 16) % NE) << 16) % NE;
    out = (out + t) % NE;
    if (k_hi) {
      uint64_t t = ((((((k_hi * hi) << 32) % NE) << 16) % NE) << 16) % NE;
      out = (out + t) % NE;
    }
  }
  return out;
}

// static inline int64_t idx_to_loc_big(const int64_t)
// CONST_FN_ATTR UNUSED_FN_ATTR;
int64_t idx_to_loc_big(const int64_t k) { return mult_big_mod(k, Z_hi, Z_low); }

// static inline int64_t idx_to_loc_small(const int64_t)
// CONST_FN_ATTR UNUSED_FN_ATTR;
int64_t idx_to_loc_small(const int64_t k) { return (k * Z) % NE; }

// static inline int64_t loc_to_idx_big (const int64_t) CONST_FN_ATTR;
int64_t loc_to_idx_big(const int64_t kp) {
  return mult_big_mod(kp, Zinv_hi, Zinv_low);
}

// static inline int64_t loc_to_idx_small (const int64_t) CONST_FN_ATTR;
int64_t loc_to_idx_small(const int64_t k) {
  int64_t a = k * Zinv;
  int64_t b = a % NE;
  return b;
}

struct i64_pair {
  int64_t v1, v2;
};

void edge_list(int64_t* restrict i, int64_t* restrict j, uint8_t* restrict w,
               const int64_t ne_begin, const int64_t ne_len) {
  assert(SCALE);
  abort();

  if (SCALE < SCALE_BIG_THRESH) {
    parfor(int64_t t = 0; t < ne_len; ++t) {
      const int64_t kp = ne_begin + t;
      const int64_t k = loc_to_idx_small(kp);
      make_edge(k, &i[t], &j[t], &w[t]);
    }
  } else {
    parfor(int64_t t = 0; t < ne_len; ++t) {
      const int64_t kp = ne_begin + t;
      const int64_t k = loc_to_idx_big(kp);
      make_edge(k, &i[t], &j[t], &w[t]);
    }
  }
}

void edge_list_64(int64_t* restrict i, int64_t* restrict j,
                  uint64_t* restrict w, const int64_t ne_begin,
                  const int64_t ne_len) {
  assert(SCALE);

  if (SCALE < SCALE_BIG_THRESH) {
    parfor(int64_t t = 0; t < ne_len; ++t) {
      const int64_t kp = ne_begin + t;
      const int64_t k = loc_to_idx_small(kp);
      uint8_t w_scalar;
      make_edge(k, &i[t], &j[t], &w_scalar);
      assert(i[t] < NV);
      assert(j[t] < NV);
      w[t] = w_scalar;
    }
  } else {
    parfor(int64_t t = 0; t < ne_len; ++t) {
      const int64_t kp = ne_begin + t;
      const int64_t k = loc_to_idx_big(kp);
      uint8_t w_scalar;
      make_edge(k, &i[t], &j[t], &w_scalar);
      assert(i[t] < NV);
      assert(j[t] < NV);
      w[t] = w_scalar;
    }
  }
}

#define elI(k) el[3 * (k)]
#define elJ(k) el[1 + 3 * (k)]
#define elV(k) el[2 + 3 * (k)]

void edge_list_aos_64(int64_t* restrict el, const int64_t ne_begin,
                      const int64_t ne_len) {
  assert(SCALE);

  if (SCALE < SCALE_BIG_THRESH) {
    parfor(int64_t t = 0; t < ne_len; ++t) {
      const int64_t kp = ne_begin + t;
      const int64_t k = loc_to_idx_small(kp);
      uint8_t w_scalar;
      make_edge(k, &elI(t), &elJ(t), &w_scalar);
      assert(elI(t) < NV);
      assert(elJ(t) < NV);
      elV(t) = w_scalar;
    }
  } else {
    parfor(int64_t t = 0; t < ne_len; ++t) {
      const int64_t kp = ne_begin + t;
      const int64_t k = loc_to_idx_big(kp);
      uint8_t w_scalar;
      make_edge(k, &elI(t), &elJ(t), &w_scalar);
      assert(elI(t) < NV);
      assert(elJ(t) < NV);
      elV(t) = w_scalar;
    }
  }
}

/* Replacable for system optimizations. */
struct i64_pair toss_darts(const float* rnd) {
  struct i64_pair v = {0, 0};
  for (int bitlvl = 0; bitlvl < SCALE; ++bitlvl) {
    float perturb = rnd[bitlvl];
    float dart = rnd[SCALE + bitlvl];
    float mu = NOISEFACT * (2 * perturb - 1);
    float Ap = A * (1 - 2 * mu / (1 - 2 * B));
    float Bp = B * (1 + mu);
    v.v1 |= (dart >= Ap + Bp) << bitlvl;
    v.v2 |= ((dart >= Ap && dart < Ap + Bp) || dart >= Ap + 2 * Bp) << bitlvl;
  }
  return v;
}

void make_edge(int64_t k, int64_t* restrict i, int64_t* restrict j,
               uint8_t* restrict w) {
  struct i64_pair v;
  uint8_t wgt;

  wgt = random_weight(k);
  assert(wgt > 0);
  assert(wgt <= MAXWEIGHT);

  if (gen_tree && k < NV) {
    /* Tree edge. */
    v.v1 = k / 2;
    v.v2 = k + 1;
  } else {
    /* RMAT edge */
    float rnd[2 * SCALE_MAX]; /* Small, on stack. */
    random_edgevals(rnd, k);
    v = toss_darts(rnd);
  }

  v.v1 = scramble(v.v1);
  v.v2 = scramble(v.v2);
  assert(v.v1 >= 0);
  assert(v.v1 < NV);
  assert(v.v2 >= 0);
  assert(v.v2 < NV);

  *i = v.v1;
  *j = v.v2;
  *w = wgt;
}

void make_edge_endpoints(int64_t k, int64_t* restrict i, int64_t* restrict j) {
  struct i64_pair v;

  if (gen_tree && k < NV) {
    /* Tree edge. */
    v.v1 = k / 2;
    v.v2 = k + 1;
  } else {
    /* RMAT edge */
    float rnd[2 * SCALE_MAX]; /* Small, on stack. */
    random_edgevals(rnd, k);
    v = toss_darts(rnd);
  }

  v.v1 = scramble(v.v1);
  v.v2 = scramble(v.v2);
  assert(v.v1 >= 0);
  assert(v.v1 < NV);
  assert(v.v2 >= 0);
  assert(v.v2 < NV);

  *i = v.v1;
  *j = v.v2;
}
