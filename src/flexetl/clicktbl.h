#pragma once

#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <stdint.h>

#define assert1(expr, message)                 \
    {                                          \
        if ((expr) == true)                    \
            ;                                  \
        else                                   \
            throw std::runtime_error(message); \
    }

enum ClickType
{
    CT_UNKNOWN,
    CT_STR,
    CT_INT,
    CT_UINT,
    CT_DOUBLE,
    CT_ARRAY,
    CT_BOOL
};

template <class T>
struct TypeInfo
{
    static const ClickType Type = CT_UNKNOWN;
};
#define DEFCLICKTYPE(type, CT)            \
    template <>                           \
    struct TypeInfo<type>                 \
    {                                     \
        static const ClickType Type = CT; \
    };

#define CLICKTYPE(X) TypeInfo<X>::Type

using uint128_t = std::pair<uint64_t, uint64_t>;

struct Value
{
    ClickType _type;
    union {
        bool _bool;
        int64_t _integer;
        uint64_t _uinteger;
        double _double;
        std::string *_string;
        std::vector<Value> *_vector;
    };
    Value() : _uinteger(0), _type(CT_UNKNOWN) {}
    Value(ClickType type);

    Value(const Value &val) = delete;
    Value &operator=(const Value &val) = delete;

    Value &operator=(const std::string &val) = delete;
    Value &operator=(const std::vector<Value> &val) = delete;

    ~Value();
    void reset();

    Value(Value &&val);

    template <class T>
    Value(T val);

    Value(char *val);
    Value(const char *val);

    Value(std::string &&val);
    Value(std::vector<Value> &&val);

    std::string getString();
    time_t getDateTime();
    time_t getDate();
    double getDouble();
    float getFloat();

    uint128_t getUInt128();
    uint64_t getUInt64();
    int64_t getInt64();

    uint32_t getUInt32();
    int32_t getInt32();

    uint16_t getUInt16();
    int16_t getInt16();
    uint8_t getUInt8();
    int8_t getInt8();

    bool isNull();
    ClickType type();

    void push(Value &&val);
};
DEFCLICKTYPE(bool, CT_BOOL)
DEFCLICKTYPE(int, CT_INT)
DEFCLICKTYPE(unsigned, CT_UINT)
DEFCLICKTYPE(int64_t, CT_INT)
DEFCLICKTYPE(uint64_t, CT_UINT)
DEFCLICKTYPE(double, CT_DOUBLE)
DEFCLICKTYPE(std::string, CT_STR)
DEFCLICKTYPE(std::vector<Value>, CT_ARRAY)

struct ClickColumn
{
    Value _data = std::vector<Value>();

    template <class ValueT>
    bool append(size_t at, ValueT &&val)
    {
        if (_data._vector->size() != at)
        {
            _data._vector->resize(at);
        }
        _data._vector->emplace_back(std::move(val));
        return true;
    }
    bool append(size_t at, Value &&val)
    {
        if (_data._vector->size() != at)
        {
            _data._vector->resize(at);
        }
        _data._vector->emplace_back(std::move(val));
        return true;
    }

    void revert(size_t at)
    {
        if (at == 0)
            _data._vector->clear();
        else
            _data._vector->resize(at);
    }
    size_t size()
    {
        return _data._vector->size();
    }
};

class ClickTable
{
public:
    std::map<std::string, ClickColumn> m_tbl;
    size_t m_rowCount = 0;

public:
    size_t rowCount() const { return m_rowCount; }

    template <class ValueT>
    bool append(const std::string &column, ValueT &&val)
    {
        return m_tbl[std::move(column)].append(m_rowCount, std::move(val));
    }

    void finishAppend(bool bSucceeded)
    {
        if (bSucceeded)
        {
            m_rowCount++;
            //пробегамся по столбццам и для не заданных
            //элементов добавляем значение null
            for (auto &el : m_tbl)
            {
                for (size_t i = el.second.size(); i < m_rowCount; i++)
                {
                    el.second.append(i, Value());
                }
            }
        }
        else
        {
            // error at parse json str, flush write data
            for (auto &itr : m_tbl)
            {
                itr.second.revert(m_rowCount);
            }
        }
    }

    void flushData();
    void flushTbl();
    void save(const std::string &fileName) const;
    void load(const std::string &fileNamse);
};

#include "clicktbl.inl"