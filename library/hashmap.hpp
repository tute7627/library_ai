// Source: https://github.com/tute7627/library_ai/blob/main/library/hashmap.hpp
#pragma once

namespace cp {

template <class Key, class T>
class hashmap {
    static_assert(std::is_integral_v<Key> && !std::is_same_v<Key, bool> &&
                      sizeof(Key) <= sizeof(std::uint64_t),
                  "hashmap requires an integer key of at most 64 bits");
    static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                  "hashmap values must be move-constructible or copy-constructible");

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

private:
    static constexpr size_type min_capacity = 16;
    static constexpr std::uint8_t empty_control = 0x80;
    static constexpr std::uint8_t deleted_control = 0xfe;
    static constexpr size_type npos = static_cast<size_type>(-1);
    using allocator_type = std::allocator<value_type>;

    static std::uint64_t mix(std::uint64_t x) {
        x ^= x >> 30;
        x *= UINT64_C(0xbf58476d1ce4e5b9);
        x ^= x >> 27;
        x *= UINT64_C(0x94d049bb133111eb);
        return x ^ (x >> 31);
    }

    static unsigned first_bit64(std::uint64_t mask) {
        assert(mask != 0);
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<unsigned>(__builtin_ctzll(mask));
#else
        unsigned bit = 0;
        while ((mask & 1U) == 0) {
            ++bit;
            mask >>= 1;
        }
        return bit;
#endif
    }

    static std::uint64_t process_seed() {
        static const std::uint64_t seed = [] {
            std::uint64_t x = static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            x ^= static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&x));
            try {
                std::random_device random;
                x ^= (static_cast<std::uint64_t>(random()) << 32) ^ random();
            } catch (...) {
            }
            return mix(x);
        }();
        return seed;
    }

    pointer slots_ = nullptr;
    std::unique_ptr<std::uint8_t[]> controls_;
    std::unique_ptr<std::uint64_t[]> occupied_;
    size_type capacity_ = 0;
    size_type size_ = 0;
    size_type deleted_ = 0;
    std::uint64_t seed_;

    static size_type limit(size_type capacity) {
        return capacity / 2;
    }
    static size_type allocation_limit() {
        return std::allocator_traits<allocator_type>::max_size(allocator_type{});
    }
    static size_type rounded_capacity(size_type requested) {
        size_type capacity = min_capacity;
        while (capacity < requested) {
            if (capacity > allocation_limit() / 2) {
                throw std::length_error("cp::hashmap capacity too large");
            }
            capacity *= 2;
        }
        if (capacity > allocation_limit()) {
            throw std::length_error("cp::hashmap capacity too large");
        }
        return capacity;
    }
    static size_type capacity_for(size_type elements) {
        size_type capacity = min_capacity;
        while (limit(capacity) < elements) {
            if (capacity > allocation_limit() / 2) {
                throw std::length_error("cp::hashmap size too large");
            }
            capacity *= 2;
        }
        return rounded_capacity(capacity);
    }
    void allocate(size_type capacity) {
        auto controls = std::make_unique<std::uint8_t[]>(capacity);
        auto occupied = std::make_unique<std::uint64_t[]>((capacity + 63) / 64);
        std::fill_n(controls.get(), capacity, empty_control);
        allocator_type allocator;
        pointer slots = allocator.allocate(capacity);
        controls_ = std::move(controls);
        occupied_ = std::move(occupied);
        slots_ = slots;
        capacity_ = capacity;
    }
    void release() noexcept {
        if (slots_ == nullptr) return;
        for (size_type i = 0; i < capacity_; ++i) {
            if (controls_[i] < empty_control) slots_[i].~value_type();
        }
        allocator_type allocator;
        allocator.deallocate(slots_, capacity_);
        slots_ = nullptr;
        controls_.reset();
        occupied_.reset();
        capacity_ = size_ = deleted_ = 0;
    }
    std::uint64_t hash(key_type key) const {
        return mix(static_cast<std::uint64_t>(key) + seed_);
    }
    static std::uint8_t fingerprint(std::uint64_t h) {
        return static_cast<std::uint8_t>(h >> 57);
    }
    template <bool WideSearch = true>
    size_type find_index(key_type key, std::uint64_t h) const {
        if (size_ == 0) return npos;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        if constexpr (WideSearch) {
            const size_type start = static_cast<size_type>(h) & (capacity_ - 1);
            const auto tag = fingerprint(h);
            if (controls_[start] == tag && slots_[start].first == key) return start;
            size_type position = start & ~size_type{7};
            std::uint64_t allowed = ~UINT64_C(0) << ((start & 7) * 8);
            constexpr std::uint64_t low = UINT64_C(0x0101010101010101);
            constexpr std::uint64_t high = UINT64_C(0x8080808080808080);
            const auto repeated = low * tag;
            for (;;) {
                std::uint64_t bytes;
                std::memcpy(&bytes, controls_.get() + position, sizeof(bytes));
                const auto empty = bytes & ~(bytes << 1) & high & allowed;
                const auto x = bytes ^ repeated;
                std::uint64_t matches = (x - low) & ~x & high;
                matches &= allowed & (empty ^ (empty - 1));
                while (matches != 0) {
                    const size_type index = position + first_bit64(matches) / 8;
                    if (slots_[index].first == key) return index;
                    matches &= matches - 1;
                }
                if (empty != 0) return npos;
                position = (position + 8) & (capacity_ - 1);
                allowed = ~UINT64_C(0);
            }
        }
#endif
        size_type index = static_cast<size_type>(h) & (capacity_ - 1);
        const auto tag = fingerprint(h);
        for (;;) {
            const auto control = controls_[index];
            if (control == tag && slots_[index].first == key) return index;
            if (control == empty_control) return npos;
            index = (index + 1) & (capacity_ - 1);
        }
    }

    struct search_result {
        size_type index;
        bool found;
    };
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    search_result find_or_slot_deleted(key_type key, std::uint64_t h) const {
        const size_type start = static_cast<size_type>(h) & (capacity_ - 1);
        const auto tag = fingerprint(h);
        size_type position = start & ~size_type{7};
        size_type reusable = npos;
        std::uint64_t allowed = ~UINT64_C(0) << ((start & 7) * 8);
        constexpr std::uint64_t low = UINT64_C(0x0101010101010101);
        constexpr std::uint64_t high = UINT64_C(0x8080808080808080);
        const auto repeated = low * tag;
        for (;;) {
            std::uint64_t bytes;
            std::memcpy(&bytes, controls_.get() + position, sizeof(bytes));
            const auto empty = bytes & ~(bytes << 1) & high & allowed;
            const auto x = bytes ^ repeated;
            std::uint64_t matches = (x - low) & ~x & high;
            matches &= allowed & (empty ^ (empty - 1));
            while (matches != 0) {
                const size_type index = position + first_bit64(matches) / 8;
                if (slots_[index].first == key) return {index, true};
                matches &= matches - 1;
            }
            if (reusable == npos) {
                const auto vacant = bytes & high & allowed;
                if (vacant != 0) reusable = position + first_bit64(vacant) / 8;
            }
            if (empty != 0) return {reusable, false};
            position = (position + 8) & (capacity_ - 1);
            allowed = ~UINT64_C(0);
        }
    }
#endif
    search_result find_or_slot(key_type key, std::uint64_t h) const {
        size_type index = static_cast<size_type>(h) & (capacity_ - 1);
        const auto tag = fingerprint(h);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        const auto first = controls_[index];
        if (first == tag && slots_[index].first == key) return {index, true};
        if (deleted_ != 0) return find_or_slot_deleted(key, h);
        if (first == empty_control) return {index, false};
        for (;;) {
            index = (index + 1) & (capacity_ - 1);
            const auto control = controls_[index];
            if (control == tag && slots_[index].first == key) return {index, true};
            if (control == empty_control) return {index, false};
        }
#else
        size_type reusable = npos;
        for (;;) {
            const auto control = controls_[index];
            if (control == tag && slots_[index].first == key) return {index, true};
            if (control == empty_control) return {reusable == npos ? index : reusable, false};
            if (control == deleted_control && reusable == npos) reusable = index;
            index = (index + 1) & (capacity_ - 1);
        }
#endif
    }

    template <class... Args>
    void construct(size_type index, std::uint64_t h, key_type key, Args&&... args) {
        ::new (static_cast<void*>(slots_ + index)) value_type(
            std::piecewise_construct, std::forward_as_tuple(key),
            std::forward_as_tuple(std::forward<Args>(args)...));
        if (controls_[index] == deleted_control) --deleted_;
        controls_[index] = fingerprint(h);
        occupied_[index / 64] |= UINT64_C(1) << (index % 64);
        ++size_;
    }

    void rebuild(size_type capacity) {
        hashmap replacement(0, seed_);
        replacement.allocate(capacity);
        for (size_type word = 0; word < (capacity_ + 63) / 64; ++word) {
            auto bits = occupied_[word];
            while (bits != 0) {
                const size_type i = word * 64 + first_bit64(bits);
                const auto h = hash(slots_[i].first);
                const auto target = replacement.find_or_slot(slots_[i].first, h).index;
                replacement.construct(target, h, slots_[i].first,
                                      std::move_if_noexcept(slots_[i].second));
                bits &= bits - 1;
            }
        }
        swap(replacement);
    }

    template <class... Args>
    size_type rebuild_and_construct(size_type capacity, std::uint64_t h,
                                    key_type key, Args&&... args) {
        mapped_type staged(std::forward<Args>(args)...);
        rebuild(capacity);
        const auto index = find_or_slot(key, h).index;
        construct(index, h, key, std::move_if_noexcept(staged));
        return index;
    }
    void erase_index(size_type index) {
        assert(index < capacity_ && controls_[index] < empty_control);
        occupied_[index / 64] &= ~(UINT64_C(1) << (index % 64));
        slots_[index].~value_type();
        --size_;
        if (controls_[(index + 1) & (capacity_ - 1)] == empty_control) {
            controls_[index] = empty_control;
            size_type previous = (index - 1) & (capacity_ - 1);
            while (controls_[previous] == deleted_control) {
                controls_[previous] = empty_control;
                --deleted_;
                previous = (previous - 1) & (capacity_ - 1);
            }
        } else {
            controls_[index] = deleted_control;
            ++deleted_;
        }
    }

public:
    template <bool IsConst>
    class iterator_base {
        friend class hashmap;
        template <bool> friend class iterator_base;
        using owner_type = std::conditional_t<IsConst, const hashmap, hashmap>;
        owner_type* owner_ = nullptr;
        size_type index_ = 0;
        iterator_base(owner_type* owner, size_type index) : owner_(owner), index_(index) {}
        void skip_empty() {
            while (index_ < owner_->capacity_) {
                const size_type position = index_ & ~size_type{63};
                const std::uint64_t mask = owner_->occupied_[index_ / 64]
                    & (~UINT64_C(0) << (index_ % 64));
                if (mask != 0) {
                    index_ = position + first_bit64(mask);
                    return;
                }
                index_ = std::min(position + 64, owner_->capacity_);
            }
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename hashmap::value_type;
        using difference_type = typename hashmap::difference_type;
        using reference = std::conditional_t<IsConst, const value_type&, value_type&>;
        using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;
        iterator_base() = default;
        template <bool Enabled = IsConst, std::enable_if_t<Enabled, int> = 0>
        iterator_base(const iterator_base<false>& other)
            : owner_(other.owner_), index_(other.index_) {}
        reference operator*() const { return owner_->slots_[index_]; }
        pointer operator->() const { return owner_->slots_ + index_; }
        iterator_base& operator++() {
            ++index_;
            skip_empty();
            return *this;
        }
        iterator_base operator++(int) {
            auto old = *this;
            ++*this;
            return old;
        }
        template <bool Other>
        bool operator==(const iterator_base<Other>& other) const {
            return owner_ == other.owner_ && index_ == other.index_;
        }
        template <bool Other>
        bool operator!=(const iterator_base<Other>& other) const { return !(*this == other); }
    };

    using iterator = iterator_base<false>;
    using const_iterator = iterator_base<true>;

    hashmap() : hashmap(0, process_seed()) {}
    explicit hashmap(size_type buckets) : hashmap(buckets, process_seed()) {}
    hashmap(size_type buckets, std::uint64_t seed) : seed_(seed) {
        if (buckets != 0) rehash(buckets);
    }
    hashmap(std::initializer_list<value_type> values) : hashmap() {
        reserve(values.size());
        insert(values.begin(), values.end());
    }
    hashmap(const hashmap& other) : seed_(other.seed_) {
        if (other.capacity_ == 0) return;
        allocate(other.capacity_);
        try {
            for (size_type i = 0; i < capacity_; ++i) {
                if (other.controls_[i] < empty_control) {
                    construct(i, hash(other.slots_[i].first), other.slots_[i].first,
                              other.slots_[i].second);
                } else if (other.controls_[i] == deleted_control) {
                    controls_[i] = deleted_control;
                    ++deleted_;
                }
            }
        } catch (...) {
            release();
            throw;
        }
    }
    hashmap(hashmap&& other) noexcept
        : slots_(std::exchange(other.slots_, nullptr)), controls_(std::move(other.controls_)),
          occupied_(std::move(other.occupied_)),
          capacity_(std::exchange(other.capacity_, 0)), size_(std::exchange(other.size_, 0)),
          deleted_(std::exchange(other.deleted_, 0)), seed_(other.seed_) {}
    hashmap& operator=(const hashmap& other) {
        if (this != &other) {
            hashmap replacement(other);
            swap(replacement);
        }
        return *this;
    }
    hashmap& operator=(hashmap&& other) noexcept {
        if (this != &other) {
            hashmap replacement(std::move(other));
            swap(replacement);
        }
        return *this;
    }
    ~hashmap() { release(); }

    void swap(hashmap& other) noexcept {
        using std::swap;
        swap(slots_, other.slots_);
        swap(controls_, other.controls_);
        swap(occupied_, other.occupied_);
        swap(capacity_, other.capacity_);
        swap(size_, other.size_);
        swap(deleted_, other.deleted_);
        swap(seed_, other.seed_);
    }
    friend void swap(hashmap& a, hashmap& b) noexcept { a.swap(b); }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    std::uint64_t hash_seed() const noexcept { return seed_; }
    size_type bucket_count() const noexcept { return capacity_; }
    float load_factor() const noexcept {
        return capacity_ == 0 ? 0.0F : static_cast<float>(size_) / capacity_;
    }
    float max_load_factor() const noexcept {
        return 0.5F;
    }

    iterator begin() noexcept {
        iterator it(this, 0);
        it.skip_empty();
        return it;
    }
    const_iterator begin() const noexcept {
        const_iterator it(this, 0);
        it.skip_empty();
        return it;
    }
    const_iterator cbegin() const noexcept { return begin(); }
    iterator end() noexcept { return iterator(this, capacity_); }
    const_iterator end() const noexcept { return const_iterator(this, capacity_); }
    const_iterator cend() const noexcept { return end(); }

    iterator find(key_type key) {
        const auto index = find_index(key, hash(key));
        return iterator(this, index == npos ? capacity_ : index);
    }
    const_iterator find(key_type key) const {
        const auto index = find_index(key, hash(key));
        return const_iterator(this, index == npos ? capacity_ : index);
    }
    size_type count(key_type key) const { return find_index(key, hash(key)) != npos; }
    bool contains(key_type key) const { return count(key) != 0; }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(key_type key, Args&&... args) {
        if (capacity_ == 0) allocate(min_capacity);
        const auto h = hash(key);
        auto target = find_or_slot(key, h);
        if (target.found) return {iterator(this, target.index), false};
        const bool grow = size_ >= limit(capacity_);
        const auto filled_limit = limit(capacity_) + (capacity_ - limit(capacity_)) / 2;
        const bool compact = controls_[target.index] == empty_control &&
                             size_ + deleted_ >= filled_limit;
        if (grow || compact) {
            const auto next = grow ? capacity_for(size_ + 1) : capacity_;
            const auto index = rebuild_and_construct(next, h, key, std::forward<Args>(args)...);
            return {iterator(this, index), true};
        }
        construct(target.index, h, key, std::forward<Args>(args)...);
        return {iterator(this, target.index), true};
    }
    std::pair<iterator, bool> insert(const value_type& value) {
        return try_emplace(value.first, value.second);
    }
    std::pair<iterator, bool> insert(value_type&& value) {
        return try_emplace(value.first, std::move(value.second));
    }
    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) insert(*first);
    }
    void insert(std::initializer_list<value_type> values) {
        insert(values.begin(), values.end());
    }
    mapped_type& operator[](key_type key) { return try_emplace(key).first->second; }
    mapped_type& at(key_type key) {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("cp::hashmap::at");
        return it->second;
    }
    const mapped_type& at(key_type key) const {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("cp::hashmap::at");
        return it->second;
    }

    size_type erase(key_type key) {
        const auto h = hash(key);
        const auto index = find_index<false>(key, h);
        if (index == npos) return 0;
        erase_index(index);
        return 1;
    }
    iterator erase(const_iterator position) {
        assert(position.owner_ == this);
        const auto index = position.index_;
        erase_index(index);
        iterator next(this, index + 1);
        next.skip_empty();
        return next;
    }
    iterator erase(iterator position) { return erase(const_iterator(position)); }

    void clear() noexcept {
        for (size_type i = 0; i < capacity_; ++i) {
            if (controls_[i] < empty_control) slots_[i].~value_type();
        }
        if (capacity_ != 0) {
            std::fill_n(controls_.get(), capacity_, empty_control);
            std::fill_n(occupied_.get(), (capacity_ + 63) / 64, UINT64_C(0));
        }
        size_ = deleted_ = 0;
    }
    void reserve(size_type elements) {
        if (elements == 0) return;
        const auto capacity = capacity_for(elements);
        if (capacity > capacity_) rebuild(capacity);
    }
    void rehash(size_type buckets) {
        if (buckets == 0 && size_ == 0) {
            release();
            return;
        }
        const auto capacity = std::max(rounded_capacity(buckets), capacity_for(size_));
        rebuild(capacity);
    }

    std::ostream& dump(std::ostream& out = std::cerr) const {
        out << '{';
        bool first = true;
        for (const auto& entry : *this) {
            if (!first) out << ", ";
            first = false;
            out << entry.first << ": " << entry.second;
        }
        return out << '}';
    }
    friend std::ostream& operator<<(std::ostream& out, const hashmap& map) {
        return map.dump(out);
    }
};

}
