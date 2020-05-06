#include "clicktbl.h"
static const int KEY = 0x6264632e;
static const int MASK = 0;

class Buffer
{
    char *_data = 0;
    size_t _size = 0;

public:
    ~Buffer()
    {
        if (_data)
            free(_data);
        _data = 0;
        _size = 0;
    }
    char *reserve(size_t size)
    {
        if (_size >= size)
            return _data;
        if (_size == 0)
            _data = (char *)malloc(size);
        else
            _data = (char *)realloc(_data, size);
        _size = size;
        return _data;
    }
    char *get() { return _data; }
    char &operator[](int i) { return _data[i]; }
};

void ClickTable::flushData()
{
    m_rowCount = 0;
    for (auto &itr : m_tbl)
    {
        itr.second.revert(0);
    }
}
void ClickTable::flushTbl()
{
    m_rowCount = 0;
    m_tbl.clear();
}

bool storeValue(FILE *fl, const Value &value)
{
    fwrite(&value._type, sizeof(value._type), 1, fl);
    switch (value._type)
    {
    case CT_STR:
    {
        int size = value._string->size();
        fwrite(&size, sizeof(size), 1, fl);
        fwrite(value._string->c_str(), 1, size, fl);
        break;
    }
    case CT_ARRAY:
    {
        int size = value._vector->size();
        fwrite(&size, sizeof(size), 1, fl);
        for (const auto &el : *value._vector)
        {
            storeValue(fl, el);
        }
        break;
    }
    case CT_INT:
    case CT_UINT:
    case CT_DOUBLE:
    {
        fwrite(&value._integer, sizeof(value._integer), 1, fl);
        break;
    }
    case CT_BOOL:
        fwrite(&value._bool, sizeof(value._bool), 1, fl);
        break;
    case CT_UNKNOWN:
        break;
    default:
        throw std::runtime_error("Save unknown value type");
    }
    return true;
}

bool restoreValue(FILE *fl, Value &value, Buffer &buffer)
{
    assert1(value._type == CT_ARRAY, "Value type mast be CT_ARRAY");

    ClickType type = CT_UNKNOWN;
    assert1(1 == fread(&type, sizeof(type), 1, fl), "Read type value failed");
    switch (type)
    {
    case CT_STR:
    {
        int size = 0;
        assert1(1 == fread(&size, sizeof(size), 1, fl), "Read string value length failed");
        assert1(size == fread(buffer.reserve(size + 1), 1, size, fl), "Read string value failed");
        buffer[size] = 0;
        value._vector->emplace_back(Value(buffer.get()));
        break;
    }
    case CT_ARRAY:
    {
        int size = 0;
        assert1(1 == fread(&size, sizeof(size), 1, fl), "Read array length failed");

        Value val = std::vector<Value>();
        for (int i = 0; i < size; i++)
        {
            restoreValue(fl, val, buffer);
        }

        value._vector->emplace_back(std::move(val));
        break;
    }
    case CT_INT:
    case CT_UINT:
    case CT_DOUBLE:
    {
        Value data(type);
        assert1(1 == fread(&data._uinteger, sizeof(data._uinteger), 1, fl), "Read value failed");
        value._vector->emplace_back(std::move(data));
        break;
    }
    case CT_BOOL:
    {
        Value data(type);
        assert1(1 == fread(&data._bool, sizeof(data._bool), 1, fl), "Read value failed");
        value._vector->emplace_back(std::move(data));
        break;
    }
    case CT_UNKNOWN:
    {
        value._vector->emplace_back(Value());
        break;

    }

    default:
        throw std::runtime_error("Read unknown value type");
    }
    return true;
}

void ClickTable::save(const std::string &fileName) const
{
    FILE *fl = fopen(fileName.c_str(), "wb");
    if (!fl)
    {
        std::string mes = std::string("Error at open file \"") + fileName + std::string("\" for writing");
        throw std::runtime_error(mes);
    }
    fwrite(&KEY, sizeof(KEY), 1, fl);
    fwrite(&MASK, sizeof(MASK), 1, fl);

    int columnCount = m_tbl.size();
    int rowCount = m_rowCount;
    fwrite(&columnCount, sizeof(columnCount), 1, fl);
    fwrite(&rowCount, sizeof(rowCount), 1, fl);
    try
    {
        for (const auto &itr : m_tbl)
        {
            //int type = itr.second._type;
            //fwrite(&type, sizeof(type), 1, fl);
            // column name
            int size = itr.first.size();
            fwrite(&size, sizeof(size), 1, fl);
            fwrite(itr.first.c_str(), 1, size, fl);

            for (const auto &el : *itr.second._data._vector)
            {
                storeValue(fl, el);
            }
        }
    }
    catch (const std::exception &pExecpt)
    {
        fclose(fl);
        std::string mes = std::string("Error at save \"") + fileName + std::string("\": ") + std::string(pExecpt.what());
        throw std::runtime_error(mes);
    }

    fclose(fl);
}
void ClickTable::load(const std::string &fileName)
{

    Buffer buffer;
    FILE *fl = fopen(fileName.c_str(), "rb");
    if (!fl)
    {
        std::stringstream what;
        what << "Error at open file '" << fileName << "'";
        throw std::runtime_error(what.str());
    }

    try
    {
        int _KEY = 0;
        int _MASK = 0;

        assert1(1 == fread(&_KEY, sizeof(_KEY), 1, fl), "Read 'KEY' failed");
        assert1(1 == fread(&_MASK, sizeof(_MASK), 1, fl), "Read 'MASK' failed");
        assert1(_KEY == KEY, "invalid KEY");
        assert1(_MASK == MASK, "invalid MASK");

        int columnCount = 0;
        int rowCount = 0;
        assert1(1 == fread(&columnCount, sizeof(columnCount), 1, fl), "Read 'columnCount' failed");
        assert1(1 == fread(&rowCount, sizeof(rowCount), 1, fl), "Read 'rowCount' failed");
        for (int col = 0; col < columnCount; col++)
        {
            // column name
            int nameSize = 0;
            assert1(1 == fread(&nameSize, sizeof(nameSize), 1, fl), "Read 'nameSize' failed");
            assert1(nameSize == fread(buffer.reserve(nameSize + 1), 1, nameSize, fl), "Read 'name' failed");
            buffer[nameSize] = 0;

            auto &el = m_tbl[buffer.get()];
            el._data._vector->reserve(rowCount);

            for (int row = 0; row < rowCount; row++)
            {
                restoreValue(fl, el._data, buffer);
            }
        }
        m_rowCount = rowCount;
    }
    catch (const std::exception &pExecpt)
    {
        fclose(fl);
        flushTbl();
        std::stringstream what;
        what << "Error at load '" << fileName << "': " << pExecpt.what();
        throw std::runtime_error(what.str());
    }
    fclose(fl);
}
