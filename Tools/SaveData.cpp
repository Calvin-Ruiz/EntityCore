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
        default:
            throw std::bad_function_call();
    }
    return str[key];
}

SaveData &SaveData::operator[](uint64_t address)
{
    switch (type) {
        case SaveSection::UNDEFINED:
            type = SaveSection::ADDRESS_MAP;
            [[fallthrough]];
        case SaveSection::ADDRESS_MAP:
            break;
        case SaveSection::LIST:
            return arr[address];
        default:
            throw std::bad_function_call();
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
            arr.push_back(data);
            break;
        default:
            throw std::bad_function_call();
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
                return !addr.empty();
            case SaveSection::LIST:
                return !arr.empty();
            case SaveSection::SUBFILE:
                return true;
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
                return addr.empty();
            case SaveSection::LIST:
                return arr.empty();
            case SaveSection::SUBFILE:
                return false;
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
        case SaveSection::LIST:
        {
            uint16_t nbEntry = *(((uint16_t *&) data)++);
            arr.resize(nbEntry);
            for (uint16_t i = 0; i < nbEntry; ++i)
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
            *(((uint8_t *&) data)++) = raw.size();
            break;
        case SaveSection::SHORT_SIZE:
            *(((uint16_t *&) data)++) = raw.size();
            break;
        case SaveSection::INT_SIZE:
            *(((uint32_t *&) data)++) = raw.size();
            break;
    }
    memcpy(data, raw.data(), raw.size());
    data += raw.size();
    switch (type) {
        case SaveSection::UNDEFINED:
            break;
        case SaveSection::STRING_MAP:
        {
            uint16_t &nbEntry = *((uint16_t *&) data)++;
            nbEntry = 0;
            for (auto &v : str) {
                if (v.second.nonEmpty()) {
                    ++nbEntry;
                    *(((uint8_t *&) data)++) = v.first.size();
                    memcpy(data, v.first.c_str(), v.first.size());
                    data += v.first.size();
                    v.second.save(data);
                    data += v.second.getSize();
                }
            }
            break;
        }
        case SaveSection::ADDRESS_MAP:
        {
            uint16_t &nbEntry = *((uint16_t *&) data)++;
            nbEntry = 0;
            for (auto &v : addr) {
                if (v.second.nonEmpty()) {
                    ++nbEntry;
                    *(((uint64_t *&) data)++) = v.first;
                    v.second.save(data);
                    data += v.second.getSize();
                }
            }
            break;
        }
        case SaveSection::LIST:
        {
            *(((uint16_t *&) data)++) = arr.size();
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
    if (object)
        object->save(this);
    dataSize = raw.size();
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
    }
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
        case SaveSection::ADDRESS_MAP:
            for (auto &v : addr) {
                if (v.second.nonEmpty())
                    dataSize += 8 + v.second.computeSize();
            }
            dataSize += 2;
            break;
        case SaveSection::LIST:
            for (auto &v : arr) {
                dataSize += v.computeSize();
            }
            dataSize += 2;
            break;
    }
    return ++dataSize;
}

void SaveData::save(std::vector<char> &data)
{
    data.resize(computeSize());
    char *ptr = data.data();
    save(ptr);
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
    if (raw.empty())
        throw std::bad_function_call();
    if (!subsave)
        subsave = BigSave::loadShared(std::string(raw.data(), raw.size()));
    return *subsave;
}

void SaveData::close()
{
    subsave = nullptr;
}
void SaveData::debugDump(std::ostream &out, int spacing, bool (*dumpContent)(const std::vector<char> &data, std::ostream &out), int level)
{
    const char *spaces = "                                                                                ";
    switch (specialType) {
        case SaveSection::SUBFILE:
            out << "BigSave file ";
            break;
        case SaveSection::SAVABLE_OBJECT:
            out << "<object> ";
            break;
    }
    if (!raw.empty())
        genericDumpContent(raw, out, dumpContent);
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
                    v.second.debugDump(out, spacing, dumpContent, level);
                }
                level -= spacing;
                out.write(spaces, level);
                out << '}';
                break;
            case SaveSection::ADDRESS_MAP:
                out << "{\n";
                for (auto &v : addr) {
                    out.write(spaces, level);
                    out << (void *) v.first;
                    out << " = ";
                    v.second.debugDump(out, spacing, dumpContent, level);
                }
                level -= spacing;
                out.write(spaces, level);
                out << '}';
                break;
            case SaveSection::LIST:
                out << "[\n";
                for (auto &v : arr) {
                    out.write(spaces, level);
                    v.debugDump(out, spacing, dumpContent, level);
                }
                level -= spacing;
                out.write(spaces, level);
                out << ']';
                break;
        }
    }
    out << '\n';
}

void SaveData::genericDumpContent(const std::vector<char> &data, std::ostream &out, bool (*specializedDumpContent)(const std::vector<char> &data, std::ostream &out))
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
            out << "(long) " << *(long *) data.data() << " ";
            return;
        default:
            out << "Bloc of " << data.size() << " bytes ";
    }
}

const std::string &SaveData::operator=(const std::string &content)
{
    raw.resize(content.size());
    content.copy(raw.data(), raw.size());
    return content;
}