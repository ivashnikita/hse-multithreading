#include "dfs.h"

#include <gtest/gtest.h>

TEST(DFS, LinearGraph) {
    dfs::graph_t g(4);
    g[0] = {1};
    g[1] = {2};
    g[2] = {3};

    std::vector<int> result;
    for (auto& node : dfs::traverse(g, 0)) {
        result.push_back(node);
    }

    EXPECT_EQ(result, (std::vector<int>{0, 1, 2, 3}));
}

TEST(DFS, BranchingGraph) {
    dfs::graph_t g(5);
    g[0] = {1, 2};
    g[1] = {3, 4};

    std::vector<int> result;
    for (auto& node : dfs::traverse(g, 0)) {
        result.push_back(node);
    }

    EXPECT_EQ(result, (std::vector<int>{0, 1, 3, 4, 2}));
}

TEST(DFS, CycleGraph) {
    dfs::graph_t g(3);
    g[0] = {1};
    g[1] = {2};
    g[2] = {0};

    std::vector<int> result;
    for (auto& node : dfs::traverse(g, 0)) {
        result.push_back(node);
    }

    EXPECT_EQ(result, (std::vector<int>{0, 1, 2}));
}

TEST(DFS, SingleNode) {
    dfs::graph_t g(1);

    std::vector<int> result;
    for (auto& node : dfs::traverse(g, 0)) {
        result.push_back(node);
    }

    EXPECT_EQ(result, (std::vector<int>{0}));
}
