#if !defined(GENERATOR_HEADER_)
#define GENERATOR_HEADER_
#include <inttypes.h>

void make_edge(int64_t, int64_t* restrict, int64_t* restrict,
               uint8_t* restrict);
void make_edge_endpoints(int64_t, int64_t* restrict, int64_t* restrict);
void edge_list(int64_t* restrict, int64_t* restrict, uint8_t* restrict,
               const int64_t, const int64_t);
void edge_list_64(int64_t* restrict, int64_t* restrict, uint64_t* restrict,
                  const int64_t, const int64_t);
void edge_list_aos_64(int64_t* restrict, const int64_t, const int64_t);

int64_t loc_to_idx_big(const int64_t kp);
int64_t loc_to_idx_small(const int64_t k);
int64_t idx_to_loc_big(const int64_t k);
int64_t idx_to_loc_small(const int64_t k);
#endif /* GENERATOR_HEADER_ */
