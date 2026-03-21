#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/coroutine2/all.hpp>

namespace dfs {

using graph_t = std::vector<std::vector<int>>;
using coro_t = boost::coroutines2::coroutine<int>;

graph_t load_graph(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open file: " + path);
    }

    std::string line;
    // skip header
    std::getline(file, line);

    int max_node = -1;
    std::vector<std::pair<int, int>> edges;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream ss(line);
        std::string from_str, to_str;
        std::getline(ss, from_str, ',');
        std::getline(ss, to_str, ',');

        int from = std::stoi(from_str);
        int to = std::stoi(to_str);
        edges.push_back({from, to});

        max_node = std::max(max_node, std::max(from, to));
    }

    graph_t adj(max_node + 1);
    for (auto [from, to] : edges) {
        adj[from].push_back(to);
    }

    return adj;
}

// single coroutine DFS generator - yields each visited node
coro_t::pull_type traverse(const graph_t& graph, int start) {
    return coro_t::pull_type([&graph, start](coro_t::push_type& yield) {
        std::vector<bool> visited(graph.size(), false);

        std::vector<int> stack;
        stack.push_back(start);

        while (!stack.empty()) {
            int node = stack.back();
            stack.pop_back();

            if (visited[node]) {
                continue;
            }
            visited[node] = true;

            yield(node);

            for (auto it = graph[node].rbegin(); it != graph[node].rend(); ++it) {
                if (!visited[*it]) {
                    stack.push_back(*it);
                }
            }
        }
    });
}

}