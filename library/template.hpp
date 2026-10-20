// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
#pragma once

#include <bits/stdc++.h>
using namespace std;

template <typename T = int>
struct edge {
    int to;
    T cost;
    int id;
    edge() : to(-1), id(-1) {}
    edge(int to, T cost = 1, int id = -1) : to(to), cost(cost), id(id) {}
    operator int() const { return to; }
};

template <typename T>
using Graph = vector<vector<edge<T>>>;
