/*
** EntityCore
** C++ Tools - SaveData
** File description:
** Serializable arborescent binary container
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/

#include "BigSave.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <functional>
#include <exception>

SaveData::SaveData()
{
}

SaveData::SaveData(const std::string &content)
{
    raw.resize(content.size());
    content.copy(raw.data(), raw.size());
}

SaveData::~SaveData() {}

SaveData &SaveData::operator[](const std::string &key)
{
    switch (type) {
        case SaveSection::UNDEFINED:
            type = SaveSection::STRING_MAP;
            [[fallthrough]];
        case SaveSection::STRING_MAP:
            break;
        case SaveSection::POINTER:
            return (*ptr)[key];
        default:
            #ifndef NO_SAVEDATA_THROW
            throw std::bad_function_call()
            #endif
            ;
    }
    return str[key];
}

SaveData &SaveData::operator[](uint64_t address)
{
    switch (type) {
        case SaveSection::UNDEFINED:
            type = SaveSection::SHORT_MAP;
            [[fallthrough]];
        case SaveSection::SHORT_MAP:
            if (address > UINT16_MAX)
                type = SaveSection::ADDRESS_MAP;
            [[fallthrough]];
        case SaveSection::ADDRESS_MAP:
            break;
        case SaveSection::LIST:
        case SaveSection::WIDE_LIST:
            return arr[address];
        case SaveSection::POINTER:
            return (*ptr)[address];
        default:
            #ifndef NO_SAVEDATA_THROW
            throw std::bad_function_call()
            #endif
            ;
    }
    return addr[address];
}

int SaveData::push(const SaveData &data)
{
    switch (type) {
        case SaveSection::UNDEFINED:
            type = SaveSection::LIST;
            [[fallthrough]];
        case SaveSection::LIST:
        case SaveSection::WIDE_LIST:
            arr.push_back(data);
            break;
        case SaveSection::POINTER:
            return ptr->push(data);
        default:
            #ifndef NO_SAVEDATA_THROW
            throw std::bad_function_call()
            #endif
            ;
    }
    return arr.size() - 1;
}

void SaveData::truncate()
{
    str.clear();
    addr.clear();
    arr.clear();
}

void SaveData::reset()
{
    truncate();
    raw.clear();
    type = SaveSection::UNDEFINED;
}

bool SaveData::nonEmpty() const
{
    if (raw.empty()) {
        switch (type) {
            case SaveSection::UNDEFINED:
                return false;
            case SaveSection::STRING_MAP:
                return !str.empty();
            case SaveSection::ADDRESS_MAP:
            case SaveSection::SHORT_MAP:
                return !addr.empty();
            case SaveSection::LIST:
            case SaveSection::WIDE_LIST:
                return !arr.empty();
            case SaveSection::POINTER:
                return ptr->nonEmpty();
        }
    }
    return true;
}

bool SaveData::empty() const
{
    if (raw.empty()) {
        switch (type) {
            case SaveSection::UNDEFINED:
                return true;
            case SaveSection::STRING_MAP:
                return str.empty();
            case SaveSection::ADDRESS_MAP:
            case SaveSection::SHORT_MAP:
                return addr.empty();
            case SaveSection::LIST:
            case SaveSection::WIDE_LIST:
                return arr.empty();
            case SaveSection::POINTER:
                return ptr->empty();
        }
    }
    return false;
}

void SaveData::load(char *&data)
{
    size_t size = 0;
    type = *(data++);
    sizeType = type & SIZE_MASK;
    specialType = type & SPECIAL_MASK;
    type &= TYPE_MASK;
    switch (sizeType) {
        case SaveSection::CHAR_SIZE:
            size = *((uint8_t *&) data)++;
            break;
        case SaveSection::SHORT_SIZE:
            size = *((uint16_t *&) data)++;
            break;
        case SaveSection::INT_SIZE:
            size = *((uint32_t *&) data)++;
            break;
        default:
            goto NO_DATA; // There is no attached data
    }
    raw.resize(size);
    memcpy(raw.data(), data, size);
    data += size;
    NO_DATA:
    switch (type) {
        case SaveSection::UNDEFINED:
            break;
        case SaveSection::STRING_MAP:
        {
            uint16_t nbEntry = *(((uint16_t *&) data)++);
            while (nbEntry--) {
                std::string s(data + 1, *data);
                data += *((unsigned char *) data) + 1;
                str[s].load(data);
            }
            break;
        }
        case SaveSection::ADDRESS_MAP:
        {
            uint16_t nbEntry = *(((uint16_t *&) data)++);
            while (nbEntry--)
                addr[*((uint64_t *&) data)++].load(data);
            break;
        }
        case SaveSection::SHORT_MAP:
        {
            uint16_t nbEntry = *(((uint16_t *&) data)++);
            while (nbEntry--)
                addr[*((uint16_t *&) data)++].load(data);
            break;
        }
        case SaveSection::LIST:
        {
            uint16_t nbEntry = *(((uint16_t *&) data)++);
            arr.resize(nbEntry);
            for (uint16_t i = 0; i < nbEntry; ++i)
                arr[i].load(data);
            break;
        }
        case SaveSection::WIDE_LIST:
        {
            uint32_t nbEntry = *(((uint32_t *&) data)++);
            arr.resize(nbEntry);
            for (uint32_t i = 0; i < nbEntry; ++i)
                arr[i].load(data);
            break;
        }
    }
}

void SaveData::save(char *data)
{
    *(data++) = type | sizeType | specialType;
    switch (sizeType) {
        case SaveSection::CHAR_SIZE:
            *(((uint8_t *&) data)++) = raw.size(); // -Wstrict-aliasing with data var
            break;
        case SaveSection::SHORT_SIZE:
            *(((uint16_t *&) data)++) = raw.size(); // -Wstrict-aliasing with data var
            break;
        case SaveSection::INT_SIZE:
            *(((uint32_t *&) data)++) = raw.size(); // -Wstrict-aliasing with data var
            break;
    }
    memcpy(data, raw.data(), raw.size());
    data += raw.size();
    switch (type) {
        case SaveSection::UNDEFINED:
            break;
        case SaveSection::STRING_MAP:
        {
            uint16_t &nbEntry = *((uint16_t *&) data)++; // -Wstrict-aliasing with data var
            nbEntry = 0;
            for (auto &v : str) {
                if (v.second.nonEmpty()) {
                    ++nbEntry;
                    *(((uint8_t *&) data)++) = v.first.size(); // -Wstrict-aliasing with data var
                    memcpy(data, v.first.c_str(), v.first.size());
                    data += v.first.size();
                    v.second.save(data);
                    data += v.second.getSize();
                }
            }
            break;
        }
        case SaveSection::SHORT_MAP:
        {
            uint16_t &nbEntry = *((uint16_t *&) data)++; // -Wstrict-aliasing with data var
            nbEntry = 0;
            for (auto &v : addr) {
                if (v.second.nonEmpty()) {
                    ++nbEntry;
                    *(((uint16_t *&) data)++) = v.first; // -Wstrict-aliasing with data var
                    v.second.save(data);
                    data += v.second.getSize();
                }
            }
            break;
        }
        case SaveSection::ADDRESS_MAP:
        {
            uint16_t &nbEntry = *((uint16_t *&) data)++; // -Wstrict-aliasing with data var
            nbEntry = 0;
            for (auto &v : addr) {
                if (v.second.nonEmpty()) {
                    ++nbEntry;
                    *(((uint64_t *&) data)++) = v.first; // -Wstrict-aliasing with data var
                    v.second.save(data);
                    data += v.second.getSize();
                }
            }
            break;
        }
        case SaveSection::LIST:
        {
            *(((uint16_t *&) data)++) = arr.size(); // -Wstrict-aliasing with data var
            for (auto &v : arr) {
                v.save(data);
                data += v.getSize();
            }
            break;
        }
        case SaveSection::WIDE_LIST:
        {
            *(((uint32_t *&) data)++) = arr.size(); // -Wstrict-aliasing with data var
            for (auto &v : arr) {
                v.save(data);
                data += v.getSize();
            }
            break;
        }
    }
}

size_t SaveData::computeSize()
{
    dataSize = raw.size();
    #ifdef NO_SAVEDATA_SMART_SIZE
    sizeType = SaveSection::INT_SIZE;
    dataSize += 4;
    #else
    if (dataSize > 0) {
        if (dataSize > UINT8_MAX) {
            if (dataSize > UINT16_MAX) {
                sizeType = SaveSection::INT_SIZE;
                dataSize += 4;
            } else {
                sizeType = SaveSection::SHORT_SIZE;
                dataSize += 2;
            }
        } else {
            sizeType = SaveSection::CHAR_SIZE;
            dataSize += 1;
        }
    } else
        sizeType = 0;
    #endif
    switch (type) {
        case SaveSection::UNDEFINED:
            break;
        case SaveSection::STRING_MAP:
            for (auto &v : str) {
                if (v.second.nonEmpty())
                    dataSize += v.first.size() + 1 + v.second.computeSize();
            }
            dataSize += 2;
            break;
        case SaveSection::SHORT_MAP:
            #ifndef NO_SAVEDATA_ADVANCED_TYPES
            for (auto &v : addr) {
                if (v.second.nonEmpty())
                    dataSize += 2 + v.second.computeSize();
            }
            dataSize += 2;
            break;
            #else
            type = SaveSection::ADDRESS_MAP;
            [[fallthrough]];
            #endif
        case SaveSection::ADDRESS_MAP:
            for (auto &v : addr) {
                if (v.second.nonEmpty())
                    dataSize += 8 + v.second.computeSize();
            }
            dataSize += 2;
            break;
        case SaveSection::LIST:
        case SaveSection::WIDE_LIST:
            for (auto &v : arr) {
                dataSize += v.computeSize();
            }
            if (arr.size() > UINT16_MAX) {
                type = SaveSection::WIDE_LIST;
                dataSize += 4;
            } else {
                type = SaveSection::LIST;
                dataSize += 2;
            }
            break;
    }
    return ++dataSize;
}

void SaveData::save(std::vector<char> &data)
{
    data.resize(computeSize());
    char *tmp = data.data();
    save(tmp);
}

BigSave &SaveData::file(const std::string &filename)
{
    if (raw.empty()) {
        specialType = SaveSection::SUBFILE;
        raw.resize(filename.size());
        memcpy(raw.data(), filename.data(), filename.size());
    }
    return file();
}

BigSave &SaveData::file()
{
    #ifndef NO_SAVEDATA_THROW
    if (raw.empty())
        throw std::bad_function_call();
    #endif
    if (!subsave)
        subsave = BigSave::loadShared(std::string(raw.data(), raw.size()));
    return *subsave;
}

void SaveData::close()
{
    subsave = nullptr;
}

size_t SaveData::size()
{
    switch (type) {
        case SaveSection::STRING_MAP:
            return str.size();
        case SaveSection::LIST:
        case SaveSection::WIDE_LIST:
            return arr.size();
        case SaveSection::POINTER:
            return ptr->size();
        default:
            return addr.size();
    }
}

void SaveData::debugDump(std::ostream &out, int spacing, dump_function_t dumpContent, int level, dump_override_t dumpOverride)
{
    const char *spaces = "                                                                                ";
    switch (specialType) {
        case SaveSection::SUBFILE:
            out << "BigSave file ";
            break;
    }
    if (!raw.empty()) {
        if (!dumpOverride || !dumpOverride(raw, out, spacing, level))
            genericDumpContent(raw, out, dumpContent);
    }
    if (type) {
        level += spacing;
        switch (type) {
            case SaveSection::STRING_MAP:
                out << "{\n";
                for (auto &v : str) {
                    out.write(spaces, level);
                    out << '"';
                    out << v.first;
                    out << "\" = ";
                    v.second.debugDump(out, spacing, dumpContent, level, dumpOverride);
                }
                level -= spacing;
                out.write(spaces, level);
                out << '}';
                break;
            case SaveSection::ADDRESS_MAP:
            case SaveSection::SHORT_MAP:
                out << "{\n";
                for (auto &v : addr) {
                    out.write(spaces, level);
                    out << (void *) v.first;
                    out << " = ";
                    v.second.debugDump(out, spacing, dumpContent, level, dumpOverride);
                }
                level -= spacing;
                out.write(spaces, level);
                out << '}';
                break;
            case SaveSection::LIST:
            case SaveSection::WIDE_LIST:
                out << "[\n";
                for (auto &v : arr) {
                    out.write(spaces, level);
                    v.debugDump(out, spacing, dumpContent, level, dumpOverride);
                }
                level -= spacing;
                out.write(spaces, level);
                out << ']';
                break;
            case SaveSection::POINTER:
                out << "-> ";
                ptr->debugDump(out, spacing, dumpContent, level, dumpOverride);
                break;
        }
    }
    out << '\n';
}

void SaveData::debugDump(std::ostream &out, dump_override_t dumpOverride, int spacing)
{
    debugDump(out, spacing, nullptr, 0, dumpOverride);
}

void SaveData::genericDumpContent(const std::vector<char> &data, std::ostream &out, dump_function_t specializedDumpContent)
{
    if (data.size() > 2) {// Assume no string is smaller than 2 characters
        bool isString = true;
        size_t size = 0;
        for (; size < data.size(); ++size) {
            switch (data[size]) {
                case '\n':
                case '\r':
                    break;
                case '\t':
                    continue;
                default:
                    if ((data[size] + 1) > ' ') // This cause DEL to be negative
                        continue;
                    isString = false;
            }
            break;
        }
        if (isString) {
            out << '"';
            out.write(data.data(), size);
            out << "\" ";
            return;
        }
    }
    if (specializedDumpContent && specializedDumpContent(data, out))
        return;
    switch (data.size()) {
        case 1:
            out << "(char) " << (int) data.front() << " ";
            return;
        case 2:
            out << "(short) " << *(short *) data.data() << " ";
            return;
        case 4:
            out << "(int) " << *(int *) data.data() << " ";
            return;
        case 8:
            out << "(long) " << *(int64_t *) data.data() << " ";
            return;
        default:
            out << "Bloc of " << data.size() << " bytes ";
    }
}

const std::string &SaveData::operator=(const std::string &content)
{
    if (type == SaveSection::POINTER)
        return *ptr = content;
    raw.resize(content.size());
    content.copy(raw.data(), raw.size());
    return content;
}
