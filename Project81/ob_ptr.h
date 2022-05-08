#ifndef MY_LIB_OB_PTR_HEADER
#define MY_LIB_OB_PTR_HEADER 1

#include <concepts>
#include "Predefs.h"

namespace myutil
{
    template<typename T>
    requires (! _STD is_reference_v<T>)
    class ob_ptr
    {
    public:
        ob_ptr() = default;
        explicit ob_ptr(T* raw) noexcept : raw_ptr(raw) {}
        ob_ptr(_STD nullptr_t) noexcept {}

        friend auto operator<=>(const ob_ptr&, const ob_ptr&) = default;

        template<typename U>
        requires _STD convertible_to<U*, T*>
        ob_ptr(ob_ptr<U> other) noexcept : raw_ptr(other.get()) {}

        bool has_value() const noexcept { return this->get(); }

        explicit operator bool() const noexcept { return this->has_value(); }

        explicit operator T* () const noexcept { return this->get(); }

        T* get() const noexcept { return this->raw_ptr; }

        void reset(T* new_ptr = nullptr) noexcept { this->raw_ptr = new_ptr; }

        T* release() noexcept { auto p = this->get(); this->reset(); return p; }

        T& operator*() const { return *(this->get()); }

        T* operator->() const noexcept { return this->get(); }

    private:
        T* raw_ptr{ nullptr };
    };

    template<typename T>
    auto make_ob(T& target) {
        return ob_ptr{ &target };
    }
}

#endif // !MY_LIB_OB_PTR_HEADER
