/* -*- C -*- */
#define _POSIX_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"
#include "el-generator-cmdline.h"
#include "generator.h"  // for make_edge
#include "globals.h"
#include "prng.h"  // for sample_roots

int verbose = 0;

struct gengetopt_args_info args;

// static const char filetag[] = " --format el64 --num_edges 10658
// --num_vertices 1024 --is_undirected --is_deduped"; static const char
// reverse_filetag[] = "segdeneg";

int main(int argc, char **argv) {
  VERBOSE_PRINT("LAUNCHED\n");
  if (0 != cmdline_parser(argc, argv, &args)) exit(1);
  if (NULL != getenv("VERBOSE")) {
    long lvl = strtol(getenv("VERBOSE"), NULL, 10);
    if (lvl > 1) verbose = lvl;
  }
  if (args.verbose_given) verbose = args.verbose_arg;

  FILE *f = stdout;
  if (args.filename_given && strcmp(args.filename_arg, "-")) {
    f = fopen(args.filename_arg, "w");
  }
  if (!f) DIE_PERROR("Error opening \"%s\": ", args.filename_arg);

  VERBOSE_PRINT("Starting el-generator\n");

  init_globals(args.scale_arg, args.edgefactor_arg, 255,
               1,  // unused
               args.A_arg, args.B_arg, args.noisefact_arg, args.tree_flag);

  VERBOSE_PRINT("Creating edge list... ");

  if (args.binary_flag) {
    // fwrite(filetag, 1, 8, f);
    fprintf(
        f, "--format el64 --num_edges %ld --num_vertices %ld --is_undirected\n",
        NE, NV);

  } else if (args.neo4j_flag)
    fprintf(f, ":TYPE,:START_ID,:END_ID\n");

  const size_t NE_chunk_size = args.NE_chunk_size_arg;
  const size_t nchunks = (NE + NE_chunk_size - 1) / NE_chunk_size;

  int64_t *el = NULL;
  el = malloc(3 * NE_chunk_size * sizeof(int64_t));
  assert(el);

  for (uint64_t ck = 0; ck < nchunks; ++ck) {
    uint64_t ngen = NE_chunk_size;
    if (ck * NE_chunk_size + ngen > NE) ngen = NE - ck * NE_chunk_size;
    VERBOSELVL_PRINT(2, "  chunk %ld/%ld  %ld %ld\n", (long)ck + 1,
                     (long)nchunks, (long)NE, (long)ngen);
    edge_list_aos_64(el, ck * NE_chunk_size, ngen);

    if (args.binary_flag) {
      // fwrite(el, sizeof(*el), 3 * ngen, f);
      // This is a hack, Jason's code was writing weights and we only need edges
      // could be sped up if we created array of just weight (i,j) instead of
      // (i,j,weight) triplets
      for (size_t k = 0; k < ngen; ++k) {
        fwrite(&el[k * 3], sizeof(uint64_t), 2, f);
      }

    } else {
      if (args.neo4j_flag)
        for (size_t k = 0; k < ngen; ++k)
          fprintf(f, "EDGE,%" PRId64 ",%" PRId64 "\n", el[3 * k],
                  el[1 + 3 * k]);
      else
        for (size_t k = 0; k < ngen; ++k)
          fprintf(f, "%" PRId64 "\t%" PRId64 "\t%" PRId64 "\n", el[3 * k],
                  el[1 + 3 * k], el[2 + 3 * k]);
    }
  }

  if (f) fclose(f);
  free(el);

  VERBOSE_PRINT("DONE\n");
}
