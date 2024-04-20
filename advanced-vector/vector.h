#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

using namespace std::string_view_literals;

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) { }

    RawMemory(const RawMemory&) = delete;

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_) {
            other.buffer_ = nullptr;
            other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
            rhs.Deallocate(rhs.buffer_);
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size) , size_(size) {
            std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_), size_(other.size_) {
            std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        if (this != &other) {
            Swap(other);
        }
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if (rhs.size_ < size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
            }
            size_ = rhs.size_;
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
        }

        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(begin() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(begin() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(std::forward<const T&>(value));
    }

    void PushBack(T&& value) {
        EmplaceBack(std::forward<T&&>(value));
    }

    void PopBack() noexcept {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        --size_;
    };

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* result = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            result = new (new_data + size_) T(std::forward <Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            result = new (data_ + size_) T(std::forward <Args>(args)...);
        }
        ++size_;

        return *result;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t pos_ind = pos - begin();
        iterator result = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            result = new (new_data + pos_ind) T(std::forward <Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), pos_ind, new_data.GetAddress());
                std::uninitialized_move_n(begin() + pos_ind, size_ - pos_ind, new_data.GetAddress() + pos_ind + 1);
            } else { 
                std::uninitialized_copy_n(begin(), pos_ind, new_data.GetAddress());
                std::uninitialized_copy_n(begin() + pos_ind, size_ - pos_ind, new_data.GetAddress() + pos_ind + 1);
            }

            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        } else {
            if (size_ > 0) {
                new (data_ + size_) T(std::move(*(end() - 1)));
                std::move_backward(begin() + pos_ind, end(), end() + 1);
                std::destroy_at(begin() + pos_ind);
            }
            result = new (data_ + pos_ind) T(std::forward<Args>(args)...);
        }
        ++size_;

        return result;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        size_t pos_ind = pos - begin();
        std::move(begin() + pos_ind + 1, end(), begin() + pos_ind);
        PopBack();
        return begin() + pos_ind;
    }; 

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, std::forward<const T&>(value));
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::forward<T&&>(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};