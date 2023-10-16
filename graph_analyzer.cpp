#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct edge_list_file_header {
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
      header.num_edges = std::stol(tok);
      std::cout << "[Header] " << header.num_edges << " edges" << std::endl;
    } else if (tok == "--num_vertices") {
      std::getline(header_ss, tok, ' ');
      header.num_vertices = std::stol(tok);
      std::cout << "[Header] " << header.num_vertices << " vertices"
                << std::endl;
    }
  }

  return header;
}

void verify_graph(std::ifstream& fs, edge_list_file_header const& header) {
  std::vector<size_t> vertex_degree(header.num_vertices, 0);

  size_t src, dst;
  size_t n_edges_read = 0;

  // read edges
  while (fs) {
    if (!fs.read(reinterpret_cast<char*>(&src), sizeof(long))) {
      break;
    }
    if (!fs.read(reinterpret_cast<char*>(&dst), sizeof(long))) {
      break;
    }
    vertex_degree[src] += 1;
    n_edges_read += 1;

    if (src >= header.num_vertices || dst >= header.num_vertices) {
      std::cerr << "[ERROR]: edge index "
                << "( " << src << ", " << dst << ")"
                << "out of range!" << std::endl;
      exit(EXIT_FAILURE);
    }
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
  std::unordered_map<size_t, size_t> degree_map;
  //   std::map<size_t, size_t> degree_map;
  size_t max_degree = 0;
  for (auto vd : vertex_degree) {
    max_degree = std::max(max_degree, vd);

    auto it = degree_map.find(vd);
    if (it == degree_map.end()) {
      degree_map.insert({vd, 1});
    } else {
      it->second += 1;
    }
  }

  std::cout << "\nPrinting histogram" << std::endl;
  for (size_t i = 0; i < 25; i++) {
    auto it = degree_map.find(i);
    if (it != degree_map.end()) {
      std::cout << i << " " << it->second << std::endl;
    } else {
      std::cout << i << " 0" << std::endl;
    }
  }

  std::cout << "Largest degree " << max_degree << std::endl;
}

int main(int argc, char** argv) {
  // Check args
  if (argc < 2) {
    std::cerr << "USAGE: " << argv[0] << " <FILENAME>" << std::endl;
    return EXIT_FAILURE;
  }

  //
  std::ifstream fs(argv[1], std::ifstream::binary);

  //
  edge_list_file_header header = parse_header(fs);

  //
  verify_graph(fs, header);

  fs.close();
  return EXIT_SUCCESS;
}