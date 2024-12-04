#ifndef serialize_hpp
#define serialize_hpp
#include <vector>
#include <string>
#include <sstream>
#include <tuple>
#include <functional>

#define SERIALIZE_BASIC_TYPEHEADER(type) static void serialize(std::ostream&, const type&); static void deserialize(std::istream&, type&);
namespace serial {
    SERIALIZE_BASIC_TYPEHEADER(char)
    SERIALIZE_BASIC_TYPEHEADER(int)
    SERIALIZE_BASIC_TYPEHEADER(unsigned int)
    SERIALIZE_BASIC_TYPEHEADER(long)
    SERIALIZE_BASIC_TYPEHEADER(unsigned long)
    SERIALIZE_BASIC_TYPEHEADER(long long)
    SERIALIZE_BASIC_TYPEHEADER(unsigned long long)
    SERIALIZE_BASIC_TYPEHEADER(float)
    SERIALIZE_BASIC_TYPEHEADER(double)
    static void serialize(std::ostream&, const std::string&);
    static void deserialize(std::istream&, std::string&);
    // allows both serialize(ostream, vector<pair>) and serialize(ostream, pair<vector>) to work (and other functions)
    // this has to be a class for forward declarations to work, otherwise it fails to compile
    class internal {
        public:
        template<typename... Args> static void serialize(std::ostream&, const Args&...);
        template<typename... Args> static void deserialize(std::istream&, Args&...);
        template<typename... Args> static void canSerialize(const Args&...);
        template<typename... Args> static void canDeserialize(Args&...);
    };
    // intended for use with C++20 concepts, to determine if a type can be serialized
    template<typename type> static void canSerialize(const type& obj) {
        serial::internal::canSerialize(obj);
    }
    template<typename type> static void canDeserialize(type& obj) {
        serial::internal::canDeserialize(obj);
    }
}
#define SERIALIZE_BASIC_TYPE(type) static void serial::serialize(std::ostream& os, const type& obj) {os << obj;} static void serial::deserialize(std::istream& is, type& obj) {is >> obj;}

SERIALIZE_BASIC_TYPE(char)
SERIALIZE_BASIC_TYPE(int)
SERIALIZE_BASIC_TYPE(unsigned int)
SERIALIZE_BASIC_TYPE(long)
SERIALIZE_BASIC_TYPE(unsigned long)
SERIALIZE_BASIC_TYPE(long long)
SERIALIZE_BASIC_TYPE(unsigned long long)
SERIALIZE_BASIC_TYPE(float)
SERIALIZE_BASIC_TYPE(double)

static void serial::serialize(std::ostream& os, const std::string& obj) {
    serial::serialize(os, obj.length());
    os << " "; 
    for(const char& c : obj) {
        serial::serialize(os, c);
    }
}
static void serial::deserialize(std::istream& is, std::string& obj) {
    obj = std::string("");
    size_t len = 0;
    serial::deserialize(is, len);
    obj.resize(len);
    for(char& c : obj) {
        serial::deserialize(is, c);
        is >> std::noskipws;
    }
    is >> std::skipws;
}
namespace serial {
    // insert abstract class serialization/deserialization implementations here
    template<typename type> static void serialize(std::ostream& os, const std::vector<type>& obj) {
        serial::internal::serialize(os, obj.size());
        for(const type& elem : obj) {
            os << " ";
            serial::internal::serialize(os, elem);
        }
    }
    template<typename type> static void deserialize(std::istream& is, std::vector<type>& obj) {
        size_t vectorSize = 0;
        serial::internal::deserialize(is, vectorSize);
        obj = std::vector<type>(vectorSize);
        for(size_t i=0; i<vectorSize; i++) {
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

namespace serial {
    static void serialize(std::ostream& os) {
        (void)os;
    }
    static void deserialize(std::istream& is) {
        (void)is;
    }
    template<typename type, typename type2, typename... Args> static void serialize(std::ostream& os, const type& obj, const type2& obj2, const Args&... args) {
        serial::internal::serialize(os, obj);
        serial::internal::serialize(os, obj2, args...);
    }
    template<typename type, typename type2, typename... Args> static void deserialize(std::istream& is, type& obj, type2& obj2, Args&... args) {
        serial::internal::deserialize(is, obj);
        serial::internal::deserialize(is, obj2, args...);
    }
    template<typename... Args> static std::string serializeToString(const Args&... args) {
        std::ostringstream str;
        serial::internal::serialize(str, args...);
        return str.str();
    }
    template<typename type> static type deserializeFromStream(std::istream& is) {
        type obj;
        serial::internal::deserialize(is, obj);
        return obj;
    }
    template<typename type> static type deserializeFromString(const std::string& objstr) {
        std::istringstream is(objstr);
        return serial::deserializeFromStream<type>(is);
    }
}

template<typename... Args> void serial::internal::serialize(std::ostream& os, const Args&... args) {
    return serial::serialize(os, args...);
}
template<typename... Args> void serial::internal::deserialize(std::istream& is, Args&... args) {
    return serial::deserialize(is, args...);
}
template<typename... Args> void serial::internal::canSerialize(const Args&... args) {
    // this is needed as os needs to have memory allocated for it
    std::ostringstream os;
    serial::serialize(os, args...);
}
template<typename... Args> void serial::internal::canDeserialize(Args&... args) {
    std::istringstream is;
    serial::deserialize(is, args...);
}

#endif