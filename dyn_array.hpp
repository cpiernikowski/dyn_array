#ifndef DYN_ARRAY_HPP
#define DYN_ARRAY_HPP

#include <memory>
#include <numeric>
#include <cassert>

#ifdef __GNUC__
#   define dyn_array_always_inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#   define dyn_array_always_inline __forceinline
#else
#   define dyn_array_always_inline
#endif

namespace cz {

    namespace detail {
        template <typename T, typename U, typename = void>
        struct not_eq_comparable : std::false_type {
        };

        template <typename T, typename U>
        struct not_eq_comparable<T, U, typename std::enable_if<
            std::is_convertible<decltype(std::declval<T>() != std::declval<U>()), bool>::value &&
            std::is_convertible<decltype(std::declval<U>() != std::declval<T>()), bool>::value
        >::type> : std::true_type {
        };
    }

    constexpr std::size_t operator""_uz(unsigned long long arg) {
        return static_cast<std::size_t>(arg);
    }

    constexpr std::size_t dyn_arr_default_initial_cap = 8_uz;
    constexpr std::size_t dyn_arr_default_multiplier = 2_uz;

    template <
        typename T,
        typename alloc_t = std::allocator<T>,
        std::size_t initial_cap = dyn_arr_default_initial_cap,
        std::size_t multiplier = dyn_arr_default_multiplier
    >
    class dyn_array {

        static_assert(std::is_copy_constructible<T>::value);
        static_assert(std::is_copy_constructible<alloc_t>::value);

    public:

        using value_type = T;
        using allocator_type = alloc_t;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = T*;
        using const_reverse_iterator = const T*;

    private:

        allocator_type m_allocator{};
        pointer m_begin = nullptr;
        size_type m_size = 0_uz;
        size_type m_cap = 0_uz;

        void _set_cap_and_alloc(size_type minimal_cap) {
            if (m_cap == 0_uz) {
                m_cap = initial_cap;
            }

            while (m_cap < minimal_cap) {
                m_cap *= multiplier;
            }

            m_begin = m_allocator.allocate(m_cap);
        }

        void _fill_with_val(const_reference value = {}) {
            static_assert(std::is_default_constructible<value_type>::value);
            assert(m_begin != nullptr && "dyn_array internal error");

            const const_iterator _end = end();
            for (iterator i = m_begin; i != _end; ++i) {
                m_allocator.construct(i, value);
            }
        }

        template <typename InIterator>
        void _fill_from_range_unchecked(InIterator f) {
            assert(m_begin != nullptr && "dyn_array internal error");

            const const_iterator _end = end();
            for (iterator i = m_begin; i != _end; ++i) {
                m_allocator.construct(i, *f++);
            }
        }

        void _destroy_all() {
            if (m_begin == nullptr) {
                return;
            }

            const const_iterator _end = end();
            for (iterator i = m_begin; i != _end; ++i) {
                m_allocator.destroy(i);
            }
        }

        void _dealloc() {
            if (m_begin == nullptr) {
                return;
            }

            m_allocator.deallocate(m_begin, m_cap);
        }

        template <template <typename> typename cmp_type, typename other_value_type>
        bool _compare_sums(dyn_array<other_value_type> const& other) const {
            if (m_begin == nullptr || other.begin() == nullptr)
                return false;

            static_assert(std::is_arithmetic<value_type>::value && std::is_arithmetic<other_value_type>::value);
            using common_t = typename std::common_type<value_type, other_value_type>::type;

            const common_t this_sum  = std::accumulate(begin(), end(), static_cast<common_t>(0));
            const common_t other_sum = std::accumulate(other.begin(), other.end(), static_cast<common_t>(0));

            return cmp_type<common_t>{}(this_sum, other_sum);
        }

    public:

        dyn_array() noexcept(noexcept(allocator_type())) {
        }

        explicit dyn_array(allocator_type const& alloc)
            noexcept(std::is_nothrow_copy_constructible<allocator_type>::value)
            : m_allocator{alloc} {
        }

        explicit dyn_array(size_type count, T const& value, allocator_type const& alloc = {})
            : m_allocator{alloc}
            , m_size{count} {
            static_assert(std::is_default_constructible<allocator_type>::value);
            _set_cap_and_alloc(count);
            _fill_with_val(value);
        }

        explicit dyn_array(size_type count, allocator_type const& alloc = {})
            : m_allocator{alloc}
            , m_size{count} {
            _set_cap_and_alloc(count);
            _fill_with_val();
        }

        template <typename InIterator>
        dyn_array(InIterator f, InIterator l, allocator_type const& alloc = {})
            : m_allocator{alloc}
            , m_size{static_cast<std::size_t>(l - f)} {
            static_assert(std::is_convertible<decltype(*f), value_type>::value);
            _set_cap_and_alloc(m_size);
            _fill_from_range_unchecked(f);
        }

        dyn_array(dyn_array const& other)
            : m_allocator{other.m_allocator}
            , m_size{other.m_size} {
            _set_cap_and_alloc(other.m_cap);
            _fill_from_range_unchecked(other.m_begin);
        }

        dyn_array(dyn_array const& other, allocator_type const& alloc)
            : m_allocator{alloc}
            , m_size{other.m_size} {
            _set_cap_and_alloc(other.m_cap);
            _fill_from_range_unchecked(other.m_begin);
        }

        dyn_array(dyn_array&& other)
            : m_allocator{other.m_allocator}
            , m_begin{other.m_begin}
            , m_size{other.m_size}
            , m_cap{other.m_cap} {
            other.m_begin = nullptr;
            other.m_size = 0;
            other.m_cap = 0;
        }

        dyn_array(dyn_array&& other, allocator_type const& alloc)
            : m_allocator{other.m_allocator}
            , m_begin{other.m_begin}
            , m_size{other.m_size}
            , m_cap{other.m_cap} {
            other.m_begin = nullptr;
            other.m_size = 0_uz;
            other.m_cap = 0_uz;
        }

        dyn_array(std::initializer_list<value_type> il, allocator_type const& alloc = {})
            : m_allocator{alloc}
            , m_size{il.size()} {        
            _set_cap_and_alloc(m_size);
            _fill_from_range_unchecked(il.begin());
        }

        ~dyn_array() {
            if (m_begin != nullptr) {
                const const_iterator _end = end();
                for (iterator i = m_begin; i != _end; ++i) {
                    m_allocator.destroy(i);
                }

                m_allocator.deallocate(m_begin, m_cap);
            }
        }

        dyn_array& operator=(dyn_array const& other) {
            if (this == &other) {
                return *this;
            }

            _destroy_all();

            if (m_cap < other.m_size) {
                _dealloc();
                _set_cap_and_alloc(other.m_size);
            }

            m_size = other.m_size;
            _fill_from_range_unchecked(other.m_begin);

            return *this;
        }

        dyn_array& operator=(dyn_array&& other) {
            if (this == &other) {
                return *this;
            }

            _destroy_all();
            _dealloc();

            m_begin = other.m_begin;
            m_size = other.m_size;
            m_cap = other.m_cap;

            other.m_begin = nullptr;
            other.m_size = 0_uz;
            other.m_cap = 0_uz;

            return *this;
        }

        template <typename other_value_type>
        bool operator==(dyn_array<other_value_type> const& other) const {
            return _compare_sums<std::equal_to>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator!=(dyn_array<other_value_type> const& other) const {
            return _compare_sums<std::not_equal_to>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator<(dyn_array<other_value_type> const& other) const {
            return _compare_sums<std::less>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator>(dyn_array<other_value_type> const& other) const {
            return _compare_sums<std::greater>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator<=(dyn_array<other_value_type> const& other) const {
            return _compare_sums<std::less_equal>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator>=(dyn_array<other_value_type> const& other) const {
            return _compare_sums<std::greater_equal>(other);
        }

        template <typename other_value_type>
        bool is_identical_to(dyn_array<other_value_type> const& other) const {
            static_assert(detail::not_eq_comparable<value_type, other_value_type>::value);

            if (m_size != other.size()) {
                return false;
            }

            auto p = other.begin();
            for (auto const& e : *this) {
                if (e != *p++) {
                    return false;
                }
            }

            return true;
        }

        dyn_array_always_inline void set_allocator(allocator_type const& other) noexcept(std::is_nothrow_copy_assignable<allocator_type>::value) {
            m_allocator = other;
        }

        dyn_array_always_inline T* begin() noexcept {
            return m_begin;
        }

        dyn_array_always_inline const T* begin() const noexcept {
            return m_begin;
        }

        dyn_array_always_inline T* end() noexcept {
            return m_begin + m_size;
        }

        dyn_array_always_inline const T* end() const noexcept {
            return m_begin + m_size;
        }

        dyn_array_always_inline size_type size() const noexcept {
            return m_size;
        }

        dyn_array_always_inline size_type cap() const noexcept {
            return m_cap;
        }
    };
}

#endif
