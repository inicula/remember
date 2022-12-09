/*
 *  Minimal implementations of some STL components:
 *      std::array,
 *      std::pair,
 *      std::for_each,
 *      std::clamp
 */

#pragma once

namespace Tiny {
/* <array> */
template <typename T, unsigned N> struct Array {
public:
    using iterator = T*;
    using const_iterator = const T*;
    using reference = T&;
    using const_reference = const T&;

    const_reference operator[](const unsigned i) const { return data[i]; }
    reference operator[](const unsigned i) { return data[i]; }
    const_iterator begin() const { return &data[0]; }
    iterator begin() { return &data[0]; }
    const_iterator end() const { return &data[N]; }
    iterator end() { return &data[N]; }

public:
    T data[N];
};

/* <utility> */
template <typename T, typename U> struct Pair {
    T first;
    U second;
};

/* <algorithm> */
template <typename Container, typename Callable> void forEach(Container& cont, Callable call)
{
    for (auto& el : cont)
        call(el);
}

template <typename T> const T& clamp(const T& x, const T& low, const T& high)
{
    if (x < low)
        return low;
    if (x > high)
        return high;
    return x;
}

template <typename T> const T& clamp(const T& x, const Pair<T, T>& range)
{
    if (x < range.first)
        return range.first;
    if (x > range.second)
        return range.second;
    return x;
}
}

#define UNREACHABLE __builtin_unreachable()
