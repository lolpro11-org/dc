// don't include this directly
#ifndef SERIALIZE_CUSTOM_HPP
#define SERIALIZE_CUSTOM_HPP

#include "serialize.hpp"

namespace serial {
    static void serialize(std::ostream& os, const std::string& obj) {
        serial::serialize(os, obj.length());
        os << " "; 
        for(const char& c : obj) {
            serial::serialize(os, c);
        }
    }
    static void deserialize(std::istream& is, std::string& obj) {
        obj = std::string("");
        std::size_t len = 0;
        serial::deserialize(is, len);
        obj.resize(len);
        for(char& c : obj) {
            serial::deserialize(is, c);
            is >> std::noskipws;
        }
        is >> std::skipws;
    }
    // insert abstract class serialization/deserialization implementations here
    template<typename type> static void serialize(std::ostream& os, const std::vector<type>& obj) {
        serial::internal::serialize(os, obj.size());
        for(const type& elem : obj) {
            os << " ";
            serial::internal::serialize(os, elem);
        }
    }
    template<typename type> static void deserialize(std::istream& is, std::vector<type>& obj) {
        std::size_t vectorSize = 0;
        serial::internal::deserialize(is, vectorSize);
        obj = std::vector<type>(vectorSize);
        for(std::size_t i=0; i<vectorSize; i++) {
            serial::internal::deserialize(is, obj[i]);
        }
    }
    template<typename typeA, typename typeB> static void serialize(std::ostream& os, const std::pair<typeA, typeB>& obj) {
        serial::internal::serialize(os, obj.first);
        os << " ";
        serial::internal::serialize(os, obj.second);
    }
    template<typename typeA, typename typeB> static void deserialize(std::istream& is, std::pair<typeA, typeB>& obj) {
        serial::internal::deserialize(is, obj.first);
        serial::internal::deserialize(is, obj.second);
    }
    template<typename... Args> static void serialize(std::ostream& os, const std::tuple<Args...>& obj) {
        // this "unpacks" the tuple and serializes it element by element
        std::apply(
            // bind_front but for C++17, should make a macro for this
            [&os](auto&... args)->void {
                return serial::internal::serialize(os, args...);
            }, obj
        );
    }
    // this function is the inverse of serialize (for tuples)
    template<typename... Args> static void deserialize(std::istream& is, std::tuple<Args...>& obj) {
        std::apply(
            [&is](auto&... args)->void {
                return serial::internal::deserialize(is, args...);
            }, obj
        );
    }
}

#endif
