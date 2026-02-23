#include <utility>
#include <cstddef>

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
