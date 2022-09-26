#ifndef DYN_ARRAY_HPP
#define DYN_ARRAY_HPP

#include <memory>
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

    template <
        typename T,
        typename alloc_t = std::allocator<T>,
        typename SizeT = std::size_t,
        SizeT initial_cap = 8,
        SizeT multiplier = 2
    >
    class dyn_array {

        static_assert(std::is_copy_constructible<T>::value);
        static_assert(std::is_copy_constructible<alloc_t>::value);
        static_assert(std::is_integral<SizeT>::value);

    public:

        using value_type = T;
        using allocator_type = alloc_t;
        using size_type = SizeT;
        using reference = T&;
        using const_reference = T const&;
        using pointer = T*;
        using const_pointer = T const*;
        using iterator = T*;
        using const_iterator = T const*;

        static constexpr size_type default_initial_cap = 8;
        static constexpr size_type default_multiplier = 2;

    private:

        allocator_type m_allocator{};
        pointer m_begin = nullptr;
        size_type m_size = 0;
        size_type m_cap = 0;

        void _set_cap_and_alloc(size_type minimal_cap) {
            if (m_cap == 0) {
                m_cap = initial_cap;
            }

            while (m_cap < minimal_cap) {
                m_cap *= multiplier;
            }

            m_begin = m_allocator.allocate(m_cap);
        }

        void _set_cap_and_realloc(size_type minimal_cap) {
            if (m_cap == 0) {
                m_cap = initial_cap;
            }

            size_type new_cap = m_cap;

            while (new_cap < minimal_cap) {
                new_cap *= multiplier;
            }

            _realloc(new_cap);
        }

        void _fill_with_val(const_reference value = {}) {
            assert(m_begin != nullptr && "dyn_array internal error");

            const const_iterator _end = end();
            for (iterator it = m_begin; it != _end; ++it) {
                m_allocator.construct(it, value);
            }
        }

        template <typename InIterator>
        void _fill_from_range_unchecked(InIterator f) {
            assert(m_begin != nullptr && "dyn_array internal error");

            const const_iterator _end = end();
            for (iterator it = m_begin; it != _end; ++it) {
                m_allocator.construct(it, *f++);
            }
        }

        void _destroy_all() {
            if (m_begin == nullptr) {
                return;
            }

            const const_iterator _end = end();
            for (iterator it = m_begin; it != _end; ++it) {
                m_allocator.destroy(it);
            }
        }

        void _dealloc() {
            if (m_begin == nullptr) {
                return;
            }

            m_allocator.deallocate(m_begin, m_cap);
        }

        void _realloc(size_type size) {
            const auto old_p = m_begin;
            m_begin = m_allocator.allocate(size);

            for (size_type i = 0; i < m_size; ++i) {
                m_allocator.construct(m_begin + i, std::move(old_p[i]));
            }

            if (old_p) {
                m_allocator.deallocate(old_p, m_cap);
            }

            m_cap = size;
        }

        template <template <typename> typename cmp_type, typename other_value_type>
        dyn_array_always_inline bool _lex_cmp(dyn_array<other_value_type> const& other) const {
            return std::lexicographical_compare(begin(), end(), other.begin(), other.end(), cmp_type<typename std::common_type<value_type, other_value_type>::type>{});
        }

    public:

        dyn_array() noexcept(noexcept(allocator_type())) {
        }

        explicit dyn_array(allocator_type const& alloc) noexcept(std::is_nothrow_copy_constructible<allocator_type>::value)
            : m_allocator{alloc} {
        }

        dyn_array(size_type count, const_reference value, allocator_type const& alloc = {})
            : m_allocator{alloc}
            , m_size{count} {
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
            , m_size{static_cast<std::size_t>(std::distance(f, l))} {
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
            : m_allocator{alloc}
            , m_begin{other.m_begin}
            , m_size{other.m_size}
            , m_cap{other.m_cap} {
            other.m_begin = nullptr;
            other.m_size = 0;
            other.m_cap = 0;
        }

        dyn_array(std::initializer_list<value_type> il, allocator_type const& alloc = {})
            : m_allocator{alloc}
            , m_size{il.size()} {        
            _set_cap_and_alloc(m_size);
            _fill_from_range_unchecked(il.begin());
        }

        ~dyn_array() {
            if (m_begin == nullptr)  {
                return;
            }

            const const_iterator _end = end();
            for (iterator i = m_begin; i != _end; ++i) {
                m_allocator.destroy(i);
            }

            m_allocator.deallocate(m_begin, m_cap);
        }

        dyn_array& operator=(dyn_array const& other) {
            assert(this != &other);

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
            assert(this != &other);

            _destroy_all();
            _dealloc();

            m_begin = other.m_begin;
            m_size = other.m_size;
            m_cap = other.m_cap;

            other.m_begin = nullptr;
            other.m_size = 0;
            other.m_cap = 0;

            return *this;
        }

        template <typename other_value_type>
        bool operator==(dyn_array<other_value_type> const& other) const {
            if (m_size != other.m_size) {
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

        template <typename other_value_type>
        dyn_array_always_inline bool operator!=(dyn_array<other_value_type> const& other) const {
            return not (*this == other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator<(dyn_array<other_value_type> const& other) const {
            return _lex_cmp<std::less>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator>(dyn_array<other_value_type> const& other) const {
            return _lex_cmp<std::greater>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator<=(dyn_array<other_value_type> const& other) const {
            return _lex_cmp<std::less_equal>(other);
        }

        template <typename other_value_type>
        dyn_array_always_inline bool operator>=(dyn_array<other_value_type> const& other) const {
            return _lex_cmp<std::greater_equal>(other);
        }

        dyn_array_always_inline reference operator[](size_type idx) noexcept {
            assert(idx < m_size);
            return m_begin[idx];
        }

        dyn_array_always_inline const_reference operator[](size_type idx) const noexcept {
            assert(idx < m_size);
            return m_begin[idx];
        }

        dyn_array_always_inline dyn_array operator[](std::pair<size_type, size_type> idxes) const { // [first, last)
            assert(idxes.first <= idxes.second && idxes.first < m_size && idxes.second <= m_size);
            return dyn_array(m_begin + idxes.first, m_begin + idxes.second);
        }

        dyn_array_always_inline void reserve(size_type n) {
            if (m_cap < n) {
                _realloc(n);
            }
        }

        dyn_array_always_inline void shrink_to_fit() {
            if (m_size < m_cap) {
                _realloc(m_size);
            }
        }

        void push_back(T const& arg) {
            if (m_size == m_cap) {
                _set_cap_and_realloc(m_size + 1);
            }

            m_allocator.construct(m_begin + m_size++, arg);
        }

        void push_back(T&& arg) {
            if (m_size == m_cap) {
                _set_cap_and_realloc(m_size + 1);
            }

            m_allocator.construct(m_begin + m_size++, std::move(arg));
        }

        template <typename... Types>
        void emplace_back(Types&&... args) {
            if (m_size == m_cap) {
                _set_cap_and_realloc(m_size + 1);
            }

            m_allocator.construct(m_begin + m_size++, std::forward<Types>(args)...);
        }

        dyn_array_always_inline value_type pop_back() {
            assert(m_size > 0);
            return std::move(m_begin[--m_size]);
        }

        void remove_at(size_type idx) {
            assert(idx < m_size);
            for (auto _end = --m_size; idx < _end; ++idx) {
                m_begin[idx] = std::move(m_begin[idx + 1]);
            }
        }

        void resize(size_type n) {
            if (m_cap < n) {
                _realloc(n);
            }

            for (size_type i = n; i < m_size; ++i) {
                m_allocator.destroy(m_begin + i);
            }

            for (size_type i = m_size; i < n; ++i) {
                m_allocator.construct(m_begin + i);
            }

            m_size = n;
        }

        void clear() {
            _destroy_all();
            m_size = 0;
        }

        dyn_array_always_inline dyn_array slice(size_type f, size_type l) { // [first, last)
            return (*this)[{f, l}];
        }

        dyn_array_always_inline reference front() noexcept {
            assert(m_size > 0);
            return *m_begin;
        }

        dyn_array_always_inline const_reference front() const noexcept {
            assert(m_size > 0);
            return *m_begin;
        }

        dyn_array_always_inline reference back() noexcept {
            assert(m_size > 0);
            return m_begin[m_size - 1];
        }

        dyn_array_always_inline const_reference back() const noexcept {
            assert(m_size > 0);
            return m_begin[m_size - 1];
        }

        dyn_array_always_inline pointer data() noexcept {
            return m_begin;
        }

        dyn_array_always_inline const_pointer data() const noexcept {
            return m_begin;
        }

        template <typename = typename std::enable_if<std::is_copy_assignable<allocator_type>::value>::type>
        dyn_array_always_inline void set_allocator(allocator_type const& other) noexcept(std::is_nothrow_copy_assignable<allocator_type>::value) {
            m_allocator = other;
        }

        template <typename = typename std::enable_if<std::is_move_assignable<allocator_type>::value>::type>
        dyn_array_always_inline void set_allocator(allocator_type&& other) noexcept(std::is_nothrow_move_assignable<allocator_type>::value) {
            m_allocator = std::move(other);
        }

        dyn_array_always_inline allocator_type& get_allocator() noexcept {
            return m_allocator;
        }

        dyn_array_always_inline allocator_type const& get_allocator() const noexcept {
            return m_allocator;
        }

        dyn_array_always_inline allocator_type const& get_const_allocator() const noexcept {
            return m_allocator;
        }

        dyn_array_always_inline iterator begin() noexcept {
            return m_begin;
        }

        dyn_array_always_inline const_iterator begin() const noexcept {
            return m_begin;
        }

        dyn_array_always_inline const_iterator cbegin() const noexcept {
            return m_begin;
        }

        dyn_array_always_inline std::reverse_iterator<iterator> rbegin() {
            return std::reverse_iterator<iterator>(begin());
        }

        dyn_array_always_inline std::reverse_iterator<iterator> rbegin() const {
            return std::reverse_iterator<const_iterator>(begin());
        }

        dyn_array_always_inline iterator end() noexcept {
            return m_begin + m_size;
        }

        dyn_array_always_inline const_iterator end() const noexcept {
            return m_begin + m_size;
        }

        dyn_array_always_inline const_iterator cend() const noexcept {
            return m_begin + m_size;
        }

        dyn_array_always_inline std::reverse_iterator<iterator> rend() {
            return std::reverse_iterator<iterator>(end());
        }

        dyn_array_always_inline std::reverse_iterator<iterator> rend() const {
            return std::reverse_iterator<const_iterator>(end());
        }

        dyn_array_always_inline size_type size() const noexcept {
            return m_size;
        }

        dyn_array_always_inline size_type cap() const noexcept {
            return m_cap;
        }

        dyn_array_always_inline bool is_empty() const noexcept {
            return m_size == 0;
        }
    };
}

#endif
