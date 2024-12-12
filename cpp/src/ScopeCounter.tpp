#ifndef SCOPECOUNTER_HPP
#define SCOPECOUNTER_HPP

#include <atomic>

// increments the count on construction, decrements on destruction
template<typename type> class ScopeCounter {
    private:

    type& counter;

    public:

    ScopeCounter(type&) noexcept;
    ~ScopeCounter() noexcept;
};

template<class type> ScopeCounter<type>::ScopeCounter(type& src) noexcept: counter(src) {
    counter++;
}

template<class type> ScopeCounter<type>::~ScopeCounter() noexcept {
    counter--;
}

#endif