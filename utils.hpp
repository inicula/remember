/*
 *  Minimal implementations of some STL components and some other utils:
 *      std::array,
 *      std::pair,
 *      std::for_each,
 *      std::clamp
 */

#pragma once
#include "Arduino.h"

namespace Tiny {
/* <array> */
template <typename T, size_t N> struct Array {
public:
    using iterator = T*;
    using const_iterator = const T*;
    using reference = T&;
    using const_reference = const T&;

    const_reference operator[](const size_t i) const { return data[i]; }
    reference operator[](const size_t i) { return data[i]; }
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

template <typename T, typename U> T clamp(const T& x, const U& low, const U& high)
{
    if (x < low)
        return low;
    if (x > high)
        return high;
    return x;
}

template <typename T> T clamp(const T& x, const Pair<T, T>& range)
{
    if (x < range.first)
        return range.first;
    if (x > range.second)
        return range.second;
    return x;
}

template <typename T> void swap(T& x, T& y)
{
    T tmp = x;
    x = y;
    y = tmp;
}

template <typename T, size_t N> void iota(Array<T, N>& array)
{
    T i = 0;
    for (auto& el : array)
        el = i++;
}

template <typename T, size_t N> void shuffle(Array<T, N>& array)
{
    for (size_t i = N - 1; i >= 1; --i)
        Tiny::swap(array[i], array[size_t(random(i + 1))]);
}

template <typename T, typename U, typename Callable, size_t N>
size_t find(const Array<T, N>& arr, const U& value, Callable c)
{
    for (size_t i = 0; i < N; ++i) {
        if (c(arr[i], value))
            return i;
    }

    return N;
}

struct String {
    constexpr String(const char* ptr)
        : ptr(ptr)
        , len(strlen(ptr))
    {
    }

    const char* ptr;
    size_t len;
};
}

#define UNREACHABLE __builtin_unreachable()
