#ifndef SCOPECOUNTER_HPP
#define SCOPECOUNTER_HPP

#include <atomic>

// increments the count on construction, decrements on destruction
// treat it like a reference, and don't destruct the object this was constructed from
// unless this object is destructed first
template<typename type> class ScopeCounter {
    private:

    type* counter;

    public:

    bool valid() const noexcept;
    ScopeCounter() noexcept = default;
    ScopeCounter(type&) noexcept;
    ScopeCounter(ScopeCounter&&) noexcept;
    ScopeCounter(const ScopeCounter&) noexcept;
    ~ScopeCounter() noexcept;

    ScopeCounter& operator=(type&) noexcept;
    ScopeCounter& operator=(ScopeCounter&&) noexcept;
    ScopeCounter& operator=(const ScopeCounter&) noexcept;
};

template<class type> ScopeCounter<type>::ScopeCounter(type& src) noexcept: counter(&src) {
    ++(*counter);
}

template<class type> ScopeCounter<type>::ScopeCounter(ScopeCounter&& src) noexcept: counter(src.counter) {
    src.counter = nullptr;
    if(this->valid()) return;
    ++(*counter);
}

template<class type> ScopeCounter<type>::ScopeCounter(const ScopeCounter& src) noexcept: counter(src.counter) {
    if(this->valid()) return;
    ++(*counter);
}

template<class type> ScopeCounter<type>& ScopeCounter<type>::operator=(type& src) noexcept {
    counter = &src;
    ++(*counter);
    return *this;
}

template<class type> ScopeCounter<type>& ScopeCounter<type>::operator=(ScopeCounter&& src) noexcept {
    counter = src.counter;
    src.counter = nullptr;
    if(this->valid()) return *this;
    ++(*counter);
    return *this;
}

template<class type> ScopeCounter<type>& ScopeCounter<type>::operator=(const ScopeCounter& src) noexcept {
    counter = src.counter;
    if(this->valid()) return *this;
    ++(*counter);
    return *this;
}

template<class type> ScopeCounter<type>::~ScopeCounter() noexcept {
    if(this->valid()) return;
    --(*counter);
}

template<class type> bool ScopeCounter<type>::valid() const noexcept {
    return (this->counter != nullptr);
}

#endif