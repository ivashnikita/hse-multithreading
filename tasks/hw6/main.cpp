#include "dfs.h"

#include <iostream>

int main(int argc, char* argv[]) {
    std::string path = "example.csv";
    if (argc > 1) {
        path = argv[1];
    }

    auto graph = dfs::load_graph(path);

    std::cout << "graph loaded: " << graph.size() << " nodes" << std::endl;
    std::cout << "DFS from node 0:" << std::endl;

    auto walker = dfs::traverse(graph, 0);
    for (auto& node : walker) {
        std::cout << node << " ";
    }
    std::cout << std::endl;

    return 0;
}