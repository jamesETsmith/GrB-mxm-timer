#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * sorted_list is a sorted std::list<std::pair<size_t,size_t>> with a fixed size
 * N.
 *
 * inserting elements should take O(log(N)) where the bottleneck is
 * std::lower_bound.
 *
 * sorted_list::insert() gets called N_vertices times so keep N small (e.g. 256)
 *
 */
template <size_t N>
struct sorted_list {
  using T = std::pair<size_t, size_t>;
  using const_iterator_t = typename std::list<T>::const_iterator;
  using iterator_t = typename std::list<T>::iterator;
  using reverse_iterator_t = typename std::list<T>::reverse_iterator;

  std::list<T> data;

  // el = {vertex_degree, index}
  void insert(T const& el) {
    auto it = std::lower_bound(data.begin(), data.end(), el);
    data.insert(it, el);
    if (data.size() > N) {
      data.pop_front();
    }
  }

  iterator_t begin() { return data.begin(); }
  iterator_t end() { return data.end(); }
  reverse_iterator_t rbegin() { return data.rbegin(); }
  reverse_iterator_t rend() { return data.rend(); }
  const_iterator_t cbegin() const { return data.cbegin(); }
  const_iterator_t cend() const { return data.cend(); }
  const size_t size() const { return data.size(); }

  // Helper functions to make dumping this info out to JSON easier
  std::array<size_t, N> get_indices() {
    std::array<size_t, N> arr;
    size_t i = 0;
    for (auto el : data) {
      arr[i] = std::get<1>(el);
      i++;
    }
    return arr;
  }

  std::array<size_t, N> get_degrees() {
    std::array<size_t, N> arr;
    size_t i = 0;
    for (auto el : data) {
      arr[i] = std::get<0>(el);
      i++;
    }
    return arr;
  }
};

struct edge_list_file_header {
  // Number of vertices in the file
  //  There can be fewer actual unique vertex ID's, but
  //  every vertex ID must be < num_vertices (0 <= vertexID < num_vertices)
  uint64_t num_vertices;
  // Number of edges in the file, including duplicates
  uint64_t num_edges;
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

  // Seeds used by Jason's generator
  uint64_t seed0 = 0;
  uint64_t seed1 = 0;
  uint64_t seed2 = 0;
  uint64_t seed3 = 0;
};

edge_list_file_header parse_header(std::ifstream& fs) {
  std::string header_str;
  std::getline(fs, header_str);
  std::stringstream header_ss(header_str);

  std::cout << "Found header " << header_str << std::endl;

  edge_list_file_header header;
  std::string tok;

  // Parse token by token
  while (std::getline(header_ss, tok, ' ')) {
    if (tok == "--format") {
      std::getline(header_ss, tok, ' ');
      if (tok != "el64") {
        std::cerr << "The format in header is unsupported, must be el64"
                  << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (tok == "--num_edges") {
      std::getline(header_ss, tok, ' ');
      header.num_edges = std::stoul(tok);
      std::cout << "[Header] " << header.num_edges << " edges" << std::endl;
    } else if (tok == "--num_vertices") {
      std::getline(header_ss, tok, ' ');
      header.num_vertices = std::stoul(tok);
      std::cout << "[Header] " << header.num_vertices << " vertices"
                << std::endl;
    } else if (tok == "--seed0") {
      std::getline(header_ss, tok, ' ');
      header.seed0 = std::stoul(tok);
      std::cout << "[Header] seed0 = " << header.seed0 << std::endl;
    } else if (tok == "--seed1") {
      std::getline(header_ss, tok, ' ');
      header.seed1 = std::stoul(tok);
      std::cout << "[Header] seed1 = " << header.seed1 << std::endl;
    } else if (tok == "--seed2") {
      std::getline(header_ss, tok, ' ');
      header.seed2 = std::stoul(tok);
      std::cout << "[Header] seed2 = " << header.seed2 << std::endl;
    } else if (tok == "--seed3") {
      std::getline(header_ss, tok, ' ');
      header.seed3 = std::stoul(tok);
      std::cout << "[Header] seed3 = " << header.seed3 << std::endl;
    }
  }

  if (header.num_edges / header.num_vertices != 16) {
    std::cerr << "\n[WARNING]\n"
              << "[WARNING] header.num_edges / header.num_vertices != 16"
              << "\n[WARNING]\n"
              << std::endl;
  }

  return header;
}

void verify_graph(std::ifstream& fs, edge_list_file_header const& header,
                  std::string output) {
  //
  // Output data
  //
  nlohmann::json output_data;
  output_data["scale"] = static_cast<uint64_t>(std::log2(header.num_vertices));
  output_data["num_vertices"] = header.num_vertices;
  output_data["num_edges"] = header.num_edges;
  output_data["seeds"] = {header.seed0, header.seed1, header.seed2,
                          header.seed3};

  //
  // Helper objects
  //

  std::vector<size_t> vertex_degree(header.num_vertices, 0);

  size_t src, dst;
  size_t n_edges_read = 0;

  //
  // read edges
  //
  // TODO use mmap for this so we can do it in parallel (and with less memory)
  while (fs) {
    if (!fs.read(reinterpret_cast<char*>(&src), sizeof(long))) {
      break;
    }
    if (!fs.read(reinterpret_cast<char*>(&dst), sizeof(long))) {
      break;
    }
    // std::cout << n_edges_read << " " << src << " " << dst << std::endl;

    if (src >= header.num_vertices || dst >= header.num_vertices) {
      std::cerr << "[ERROR]: edge index "
                << "(" << src << ", " << dst << ") "
                << "out of range!" << std::endl;
      exit(EXIT_FAILURE);
    }

    vertex_degree[src] += 1;
    n_edges_read += 1;
  }

  //
  if (n_edges_read != header.num_edges) {
    std::cerr << "[ERROR]: number of edges read (" << n_edges_read
              << ") doesn't match the number "
                 "specified in header ("
              << header.num_edges << ")" << std::endl;
    exit(EXIT_FAILURE);
  }

  //
  //
  //
  // std::array<size_t, 33> degree_map;
  // degree_map.fill(0);
  std::vector<size_t> degree_map(33, 0);

  /*
   * Index 0 is special case of unconnected edges
   * Index 1 is [2^0, 2^1) i.e. degree == 1
   * Index 2 is [2^1, 2^2) i.e. degree ∈ [2,3]
   * Index 3 is [2^2, 2^3) i.e. degree ∈ [4,5,6,7]
   * Index N is [2^(N-1), 2^N)
   */
  // std::array<size_t, 41> hist;  // Binned by power of two
  // hist.fill(0);
  std::vector<size_t> hist(41, 0);

  sorted_list<256> largest_degree_vertices;

  size_t max_degree = 0;
  // ;

#pragma omp declare reduction(                                  \
        vec_size_t_plus : std::vector<size_t> : std::transform( \
                omp_out.begin(), omp_out.end(), omp_in.begin(), \
                    omp_out.begin(), std::plus<size_t>()))      \
    initializer(omp_priv = decltype(omp_orig)(omp_orig.size()))

#pragma omp parallel for schedule(static, 32) reduction(max : max_degree) \
    reduction(vec_size_t_plus : degree_map) reduction(vec_size_t_plus : hist)
  for (size_t i = 0; i < vertex_degree.size(); i++) {
    size_t vd = vertex_degree[i];
    max_degree = std::max(max_degree, vd);

    if (vd < degree_map.size()) {
      // #pragma omp atomic update
      degree_map[vd]++;
    }

    // Add to binned histogram
    if (vd == 0) {
      // #pragma omp atomic update
      hist[0]++;
    } else {
      size_t bin = std::floor(std::log2l(vd)) + 1;
      // #pragma omp atomic update
      hist[bin]++;
    }

    // Add to our sorted_list
    // largest_degree_vertices.insert({vd, i});
  }

  // Save to json
  output_data["hist"] = degree_map;
  output_data["binned_hist"] = hist;
  output_data["max_degree"] = max_degree;

  //
  // Logging and checks
  //
  std::cout << "\nPrinting histogram preview" << std::endl;
  for (size_t i = 0; i < std::min(16UL, degree_map.size()); i++) {
    std::cout << i << " " << degree_map[i] << std::endl;
  }

  std::cout << "\nHistogram (binned by powers of two) preview" << std::endl;
  for (size_t i = 0; i < std::min(hist.size(), 16UL); i++) {
    std::cout << i << " " << hist[i] << std::endl;
  }

  // Internal consistency check (only for the first few)
  for (size_t i = 0; i < 6; i++) {
    if (i == 0) {
      assert(hist[0] == degree_map[0]);  // cheating a little with operator[]
    } else {
      size_t bin_total = 0;
      assert(std::pow(2, i) <= degree_map.size());
      for (size_t lb = std::pow(2, i - 1); lb < std::pow(2, i); lb++) {
        bin_total += degree_map[lb];
      }
      if (hist[i] != bin_total) {
        std::cerr << "i: " << i << "    hist: " << hist[i] << "    bin_total "
                  << bin_total << std::endl
                  << std::flush;
        assert(hist[i] == bin_total);
      }
    }
  }

  std::cout << "Largest degree " << max_degree << std::endl;

  std::cout << "Largest degree vertices:" << std::endl;
  std::cout << "     Vertex ID    |      Degree    " << std::endl;
  std::for_each(largest_degree_vertices.rbegin(),
                largest_degree_vertices.rend(), [](auto el) {
                  printf("%16lu    %16lu\n", std::get<1>(el), std::get<0>(el));
                });

  output_data["largest degree vertices"]["indices"] =
      largest_degree_vertices.get_indices();
  output_data["largest degree vertices"]["degree"] =
      largest_degree_vertices.get_degrees();

  // Write out
  std::ofstream o(output);
  o << std::setw(4) << output_data << std::endl;
}

int main(int argc, char** argv) {
  // Check args
  if (argc < 2 || argc > 3) {
    std::cerr << "USAGE: " << argv[0] << " <FILENAME> [<OUTPUT>]" << std::endl;
    return EXIT_FAILURE;
  }

  //
  std::ifstream fs(argv[1], std::ifstream::binary);

  //
  edge_list_file_header header = parse_header(fs);

  //
  std::string output = "graph_meta.json";
  if (argc == 3) {
    output = argv[2];
  }
  std::cout << "Saving out to " << output << std::endl;
  verify_graph(fs, header, output);

  fs.close();
  return EXIT_SUCCESS;
}