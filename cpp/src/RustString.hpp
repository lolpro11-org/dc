#ifndef RUSTSTRING_HPP
#define RUSTSTRING_HPP

#include <string>

class RustString {
    private:
    const char* str;

    public:
    bool valid() const noexcept;
    RustString() noexcept;
    RustString(const char*) noexcept;
    RustString(RustString&&) noexcept;
    ~RustString() noexcept;
    RustString& operator=(RustString&&) noexcept;
    const char* c_str() const noexcept;
    std::string cpp_str() const;

    RustString(const RustString&) = delete;
    RustString& operator=(const RustString&) = delete;
};


#endif