//
// Created by Yixin Hu on 10/12/21.
//

#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <set>
#include <vector>

namespace wmtk {

template <class T>
inline std::vector<T> set_intersection(const std::vector<T>& v1, const std::vector<T>& v2)
{
    std::vector<T> v;
    std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(v));
    return v;
}

template <class T>
inline void vector_unique(std::vector<T>& v)
{
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}

template <class T>
inline void vector_print(std::vector<T>& v)
{
    for (auto& e : v) std::cout << e << " ";
    std::cout << std::endl;
}

template <class T>
inline void vector_sort(std::vector<T>& v)
{
    std::sort(v.begin(), v.end());
}

template <class T>
inline bool vector_erase(std::vector<T>& v, const T& t)
{
    auto it = std::find(v.begin(), v.end(), t);
    if (it == v.end()) return false;
    v.erase(it);
    return true;
}

template <class T>
inline bool vector_contains(std::vector<T>& v, const T& t)
{
    auto it = std::find(v.begin(), v.end(), t);
    if (it == v.end()) return false;
    return true;
}

template <typename T>
inline bool set_erase(std::vector<T>& v, const T& t)
{
    auto it = std::lower_bound(v.begin(), v.end(), t);
    if (it == v.end() || *it != t) return false; // not found
    v.erase(it);
    return true;
}

template <typename T>
inline bool set_insert(std::vector<T>& vec, const T& val)
{
    auto it = std::lower_bound(vec.begin(), vec.end(), val);
    vec.insert(it, val);
    return true;
}
} // namespace wmtk
