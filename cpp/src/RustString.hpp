#ifndef RUSTSTRING_HPP
#define RUSTSTRING_HPP

#include <string>
#include "../../my_header.h"

class RustString {
    private:
    const char* str;

    public:
    inline bool valid() const noexcept;
    inline constexpr RustString() noexcept;
    inline RustString(const char*) noexcept;
    inline RustString(RustString&&) noexcept;
    inline ~RustString() noexcept;
    inline RustString& operator=(RustString&&) noexcept;
    inline const char* c_str() const noexcept;
    inline std::string cpp_str() const;

    RustString(const RustString&) = delete;
    RustString& operator=(const RustString&) = delete;
};

constexpr RustString::RustString() noexcept: str(nullptr) {}
RustString::RustString(const char* rstr) noexcept: str(rstr) {}
RustString::RustString(RustString&& ruststr) noexcept: str(ruststr.str) {
    ruststr.str = nullptr; // transfers ownership to this string
}
bool RustString::valid() const noexcept {
    return this->str!=nullptr;
}
RustString& RustString::operator=(RustString&& ruststr) noexcept {
    if(this==&ruststr) return *this;
    this->str = ruststr.str;
    ruststr.str = nullptr;
    return *this;
}
RustString::~RustString() noexcept {
    if(!(this->valid())) return;
    free_string(this->str);
    this->str = nullptr;
}
const char* RustString::c_str() const noexcept {
    return this->str;
}

// may throw std::bad_alloc or std::length_error
std::string RustString::cpp_str() const {
    return std::string(this->valid() ? this->c_str() : "");
}

#endif