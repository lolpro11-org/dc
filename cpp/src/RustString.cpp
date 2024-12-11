#include "RustString.hpp"
#include "../../my_header.h"

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
// may throw as it calls the C++ string constructor
std::string RustString::cpp_str() const {
    return std::string(this->valid() ? this->c_str() : "");
}