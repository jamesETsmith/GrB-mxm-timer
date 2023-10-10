// stats_graph500_bin.c
//
// Count the total number of edges and report the min/max vertex id
// Used to verify correctness of binary file

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct header {
  uint64_t nvertices;
  uint64_t nedges;
} header;

typedef struct edge {
  uint64_t src;
  uint64_t dst;
} edge;

/*static const struct option header_options[] = {
    {"num_vertices"     , required_argument},
    {"num_edges"        , required_argument},
    {"is_sorted"        , no_argument},
    {"is_deduped"       , no_argument},
    {"is_permuted"      , no_argument},
    {"is_directed"      , no_argument},
    {"is_undirected"    , no_argument},
    {"format"           , required_argument},
    {NULL}
};*/

typedef struct edge_list_file_header {
  // Number of vertices in the file
  //  There can be fewer actual unique vertex ID's, but
  //  every vertex ID must be < num_vertices (0 <= vertexID < num_vertices)
  long num_vertices;
  // Number of edges in the file, including duplicates
  long num_edges;
  // Is the edge list sorted?
  bool is_sorted;
  // Have duplicate edges been removed?
  bool is_deduped;
  // Format of the edge list: (for example, el64)
  //   el   : src, dst
  //   wel  : src, dst, weight
  //   welt : src, dst, weight, timestamp
  // Suffixes:
  //       : text, delimited by spaces and newlines
  //   32  : binary, 32 bits per field
  //   64  : binary, 64 bits per field
  char* format;
  // Number of bytes in the file header.
  // Includes the newline character
  // There is no null terminator
  size_t header_length;
} edge_list_file_header;

void parse_edge_list_file_header(FILE* fp, edge_list_file_header* header) {
  // Get the first line of text from the file
  char line[256];
  if (!fgets(line, 256, fp)) {
    printf("Failed to read edge list header\n");
    fflush(stdout);
    exit(1);
  }
  size_t line_len = strlen(line);
  header->header_length = line_len;
  // Strip endline character
  if (line[line_len - 1] != '\n') {
    printf("Invalid edge list file header\n");
    fflush(stdout);
    exit(1);
  }
  line[--line_len] = '\0';

  // Split on spaces
  //    Assumption: never more than 32 arguments in header
  const int max_argc = 32;
  char* argv[max_argc];
  // Leave empty slot for expected program name
  argv[0] = strdup("edge list header");
  int argc = 1;
  char* token = strtok(line, " ");
  while (token && argc < max_argc) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }
  // Null terminator for argv
  argv[argc] = NULL;

  // emory quick code below
  header->num_vertices = atol(argv[6]);  // hard coding these for now
  header->num_edges = atol(argv[4]);
  printf("num_vertices: %lu\n", header->num_vertices);
  fflush(stdout);
  printf("num_edges: %lu\n", header->num_edges);
  fflush(stdout);

  // It's up to the caller to validate and interpret the arguments
}

int main(int argc, char* argv[]) {
  /* if no filename is given print usage info */
  if (argc < 2) {
    fprintf(stderr, "usage: %s <FILENAME>\n", argv[0]);
    return 1;
  }

  /* open input file */
  FILE* in = fopen(argv[1], "rb");
  printf("Opening %s for reading...\n", argv[1]);
  if (in == NULL) {
    printf("unable to open %s\n", argv[1]);
    exit(1);
  }

  /* read in ascii file header */
  /* Note: assumes binary file format. Won't work for others */
  edge_list_file_header myheader;
  parse_edge_list_file_header(in, &myheader);
  printf("Header: nvertices = %lu, nedges = %lu\n", myheader.num_vertices,
         myheader.num_edges);
  fflush(stdout);

  /* Read edges and track edges_read, min_id, and max_id; */
  uint64_t edges_read = 0;
  uint64_t max_id = 0;
  uint64_t min_id = myheader.num_vertices;

  uint64_t* vertex_degree =
      (uint64_t*)malloc(sizeof(uint64_t) * myheader.num_vertices);

#pragma unroll(8)
  for (size_t i = 0; i < myheader.num_vertices; i++) {
    vertex_degree[i] = 0;
  }

  char bufferA[16];
  while (fread(&bufferA, sizeof(long), 2, in)) {
    edge e;
    memcpy(&e.src, bufferA,
           sizeof(long));  // have to memcpy to get bits in correct order
    if (e.src < min_id) min_id = e.src;
    if (e.src > max_id) max_id = e.src;
    memcpy(&e.dst, bufferA + 8, sizeof(long));  // add 8 to get second number
    if (e.dst < min_id) min_id = e.dst;
    if (e.dst > max_id) max_id = e.dst;
    // fprintf(eout, "EDGE,%lu,%lu\n", e.src, e.dst); // binary files start
    // edges at 0 instead of 1

    vertex_degree[e.src] += 1;
    edges_read++;
  }

  uint64_t max_degree = 0;
  uint64_t n_disconnected = 0;  // i.e. vertices with no edges
  for (size_t i = 0; i < myheader.num_vertices; i++) {
    vertex_degree[i] /= 2;
    if (vertex_degree[i] > max_degree) {
      max_degree = vertex_degree[i];
    } else if (vertex_degree[i] == 0) {
      n_disconnected += 1;
    }
  }
  printf("  maximum edges from a vertex: %lu\n", max_degree);
  printf("  # vertices with 0 edges:     %lu\n", n_disconnected);
  printf("  edges_read = %lu, min_id = %lu, max_id = %lu\n", edges_read, min_id,
         max_id);
  if (edges_read != myheader.num_edges) printf("ERROR: num_edges\n");
  if (max_id >= myheader.num_vertices) printf("ERROR: vertex out of range\n");
  fflush(stdout);

  /* close both files */
  fclose(in);
}