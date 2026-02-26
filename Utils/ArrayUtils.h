#ifndef ARRAYAPPEND_H
#define ARRAYAPPEND_H

#include <utility>
#include <cstddef>
#include <array>

template<typename T, std::size_t N>
struct ArrayWrapper {
    T data[N];
};

template<typename T,
    std::size_t N1,
    std::size_t N2,
    std::size_t... I1,
    std::size_t... I2>
constexpr auto append_impl(const T(&a)[N1],
    const T(&b)[N2],
    std::index_sequence<I1...>,
    std::index_sequence<I2...>)
{
    return ArrayWrapper<T, N1 + N2>{
        { a[I1]..., b[I2]... }
    };
}

/*
constexpr int a[] = {1,2,3};
constexpr int b[] = {4,5};

constexpr auto result = append(a,b);

static_assert(result.data[4] == 5);
static_assert(sizeof(result.data)/sizeof(int) == 5);
*/
template<typename T, std::size_t N1, std::size_t N2>
constexpr auto append(const T(&a)[N1],
    const T(&b)[N2])
{
    return append_impl(a, b,
        std::make_index_sequence<N1>{},
        std::make_index_sequence<N2>{});
}


template<typename T,
    std::size_t N1,
    std::size_t N2,
    std::size_t... I1,
    std::size_t... I2>
constexpr std::array<T, N1 + N2>
append_impl(const std::array<T, N1>& a,
    const std::array<T, N2>& b,
    std::index_sequence<I1...>,
    std::index_sequence<I2...>)
{
    return { a[I1]..., b[I2]... };
}

/*
constexpr std::array<int,3> a{{1,2,3}};
constexpr std::array<int,2> b{{4,5}};

constexpr auto c = append(a,b);

static_assert(c.size() == 5, "size error");
static_assert(c[3] == 4, "value error");
*/
template<typename T, std::size_t N1, std::size_t N2>
constexpr std::array<T, N1 + N2>
append(const std::array<T, N1>& a,
    const std::array<T, N2>& b)
{
    return append_impl(a, b,
        std::make_index_sequence<N1>{},
        std::make_index_sequence<N2>{});
}

template<typename T, std::size_t... I>
constexpr std::array<T, sizeof...(I)>
make_filled_array_impl(T value, std::index_sequence<I...>) { return { { (static_cast<void>(I), value)... } }; }
/*
constexpr auto tmp = make_filled_array<6>((BYTE)0xFF);
constexpr BYTE keyA[6] = {
    tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]
};
*/
template<std::size_t N, typename T>
constexpr std::array<T, N> make_filled_array(T value) { return make_filled_array_impl<T>(value, std::make_index_sequence<N>{}); }

template<typename T, std::size_t... I>
constexpr auto make_filled_array(T value, std::index_sequence<I...>) { return std::array<T, sizeof...(I)>{ { (static_cast<void>(I), value)... } }; }
// constexpr auto keyA = make_filled_array<6>((BYTE)0xFF);
template<std::size_t N, typename T>
constexpr auto make_filled_array(T value) { return make_filled_array<T>(value, std::make_index_sequence<N>{}); }

#endif // ARRAYAPPEND_H