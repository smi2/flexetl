#include <ctime>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// inline Value::Value(const Value &val) : _type(val._type)
// {
//     if (_type == CT_STR)
//         _string = new std::string(*val._string);
//     else if (_type == CT_ARRAY)
//         _vector = new std::vector<Value>(*val._vector);
//     else
//         _integer = val._integer;
// }

// inline Value &Value::operator=(const Value &val)
// {
//     if (_type == val._type)
//     {
//         if (_type == CT_STR)
//             *_string = *val._string;
//         else if (_type == CT_ARRAY)
//             *_vector = *val._vector;
//         else
//             _integer = val._integer;
//     }
//     else
//     {
//         if (_type == CT_STR)
//             delete _string;
//         else if (_type == CT_ARRAY)
//             delete _vector;

//         _type = val._type;
//         if (_type == CT_STR)
//             _string = new std::string(*val._string);
//         else if (_type == CT_ARRAY)
//             _vector = new std::vector<Value>(*val._vector);
//         else
//             _integer = val._integer;
//     }
//     return *this;
// }

inline Value::~Value()
{
    reset();
}
inline Value::Value(ClickType type) : _type(type), _integer(0)
{
    if (_type == CT_STR)
        _string = new std::string();
    else if (_type == CT_ARRAY)
        _vector = new std::vector<Value>();
}

inline void Value::reset()
{
    if (_type == CT_STR && _string != 0)
        delete _string;
    else if (_type == CT_ARRAY && _vector != 0)
        delete _vector;
    _type = CT_UNKNOWN;
}

inline Value::Value(Value&& val):_type(val._type),_uinteger(0)
{
    switch (_type)
    {
    case CT_INT:
        _integer = val._integer;
        break;
    case CT_UINT:
        _uinteger = val._uinteger;
        break;
    case CT_DOUBLE:
        _double = val._double;
        break;
    case CT_BOOL:
        _bool = val._bool;
        break;
    case CT_STR:
        std::swap(_string,val._string);
        break;
    case CT_ARRAY:
        std::swap(_vector,val._vector);
        break;
    }
}

template <class T>
Value::Value(T val) : _type(TypeInfo<T>::Type)
{
    switch (_type)
    {
    case CT_INT:
        _integer = val;
        break;
    case CT_UINT:
        _uinteger = val;
        break;
    case CT_DOUBLE:
        _double = val;
        break;
    case CT_BOOL:
        _bool = val;
        break;
    }
}
inline Value::Value(char *val) : _type(TypeInfo<std::string>::Type)
{
    _string = new std::string(val);
}
inline Value::Value(const char *val) : _type(TypeInfo<std::string>::Type)
{
    _string = new std::string(val);
}
inline Value::Value(std::string&& val) : _type(TypeInfo<std::string>::Type)
{
    _string = new std::string(std::move(val));
}
inline Value::Value(std::vector<Value>&& val) : _type(TypeInfo<std::vector<Value>>::Type)
{
    _vector = new std::vector<Value>(std::move(val));
}
inline std::string Value::getString()
{
    switch (_type)
    {
    case CT_STR:
        return *_string;
    case CT_UINT:
    {
        std::stringstream sstream;
        sstream << _uinteger;
        return sstream.str();
    }
    case CT_INT:
    {
        std::stringstream sstream;
        sstream << _integer;
        return sstream.str();
    }
    case CT_DOUBLE:
    {
        std::stringstream sstream;
        sstream << _double;
        return sstream.str();
    }
    case CT_BOOL:
    {
        return _bool ? "true" : "false";
    }
    case CT_UNKNOWN:
        return std::string("null");
    }
    return "";
}
inline time_t Value::getDateTime()
{
    switch (_type)
    {
    case CT_STR:
    {
        struct tm tm;
        strptime(_string->c_str(), "%Y-%m-%d %H:%M:%S", &tm);
        return mktime(&tm) - timezone;
    }
    case CT_INT:
    case CT_UINT:
        return _uinteger;
    case CT_UNKNOWN:
        return std::time(0);
    }
    return std::time(0);
}
inline time_t Value::getDate()
{
    switch (_type)
    {
    case CT_STR:
    {
        struct tm tm = {0};
        strptime(_string->c_str(), "%Y-%m-%d", &tm);
        return mktime(&tm) - timezone;
    }
    case CT_INT:
    case CT_UINT:
        return _uinteger;
    case CT_UNKNOWN:
        return time_t(0);
    }
    return time_t(0);
}
inline double Value::getDouble()
{
    switch (_type)
    {
    case CT_STR:
    {
        double res = 0;
        sscanf(_string->c_str(), "%lf", &res);
        return res;
    }
    case CT_UINT:
        return _uinteger;
    case CT_INT:
        return _integer;
    case CT_DOUBLE:
        return _double;
    case CT_BOOL:
        return _bool ? 1.0 : 0.0;
    case CT_UNKNOWN:
        return 0.0;
    }
    return 0.0;
}
inline float Value::getFloat() { return getDouble(); }

inline uint128_t Value::getUInt128()
{
    switch (_type)
    {
    case CT_STR:
    {
        uint64_t u1 = 0;
        uint64_t u2 = 0;
        uint64_t u3 = 0;
        uint64_t u4 = 0;
        uint64_t u5 = 0;
        if (5 != sscanf(_string->c_str(), "%lx-%lx-%lx-%lx-%lx", &u1, &u2, &u3, &u4, &u5))
            break;
        return {(u1 << 32) | (u2 << 16) | u3, (u4 << 48) | u5};
    }
    case CT_UINT:
        return {0, _uinteger};
    case CT_INT:
        return {0, _integer};
    case CT_DOUBLE:
        return {0, 0};
    case CT_BOOL:
        return _bool ? uint128_t({0, 1}) : uint128_t({0, 0});
    case CT_UNKNOWN:
        return {0, 0};
    }
    return {0, 0};
}

inline uint64_t Value::getUInt64()
{
    switch (_type)
    {
    case CT_STR:
    {
        uint64_t res = 0;
        sscanf(_string->c_str(), "%ld", &res);
        return res;
    }
    case CT_UINT:
        return _uinteger;
    case CT_INT:
        return _integer;
    case CT_DOUBLE:
        return _double;
    case CT_BOOL:
        return _bool ? 1 : 0;
    case CT_UNKNOWN:
        return 0;
    }
    return 0;
}
inline int64_t Value::getInt64() { return getUInt64(); }

inline uint32_t Value::getUInt32()
{
    switch (_type)
    {
    case CT_STR:
    {
        uint32_t dotCount = 0;
        for (int i = 0; i < _string->size(); i++)
        {
            if (_string->operator[](i) == '.')
                dotCount++;
        }
        if (dotCount == 3) // format ipv4
        {
            return ntohl(inet_addr(_string->c_str()));
        }
        uint32_t res = 0;
        sscanf(_string->c_str(), "%d", &res);
        return res;
    }
    case CT_UINT:
        return _uinteger;
    case CT_INT:
        return _integer;
    case CT_DOUBLE:
        return _double;
    case CT_BOOL:
        return _bool ? 1 : 0;
    case CT_UNKNOWN:
        return 0;
    }
    return 0;
}
inline int32_t Value::getInt32() { return getUInt32(); }

inline uint16_t Value::getUInt16()
{
    switch (_type)
    {
    case CT_STR:
    {
        uint32_t res = 0;
        sscanf(_string->c_str(), "%d", &res);
        return res;
    }
    case CT_UINT:
        return _uinteger;
    case CT_INT:
        return _integer;
    case CT_DOUBLE:
        return _double;
    case CT_BOOL:
        return _bool ? 1 : 0;
    case CT_UNKNOWN:
        return 0;
    }
    return 0;
}
inline int16_t Value::getInt16() { return getUInt16(); }

inline uint8_t Value::getUInt8()
{
    switch (_type)
    {
    case CT_STR:
    {
        uint32_t res = 0;
        sscanf(_string->c_str(), "%d", &res);
        return res;
    }
    case CT_UINT:
        return _uinteger;
    case CT_INT:
        return _integer;
    case CT_DOUBLE:
        return _double;
    case CT_BOOL:
        return _bool ? 1 : 0;
    case CT_UNKNOWN:
        return 0;
    }
    return 0;
}
inline int8_t Value::getInt8() { return getUInt8(); }
inline bool Value::isNull() { return _type == CT_UNKNOWN; }

inline void Value::push( Value &&val)
{
    if (CT_ARRAY != _type)
        throw std::runtime_error("push value for non array type");

    _vector->emplace_back(std::move(val));
}
inline ClickType Value::type()
{
    return _type;
}
