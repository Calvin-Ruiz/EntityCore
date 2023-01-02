/*
** EntityCore
** C++ Tools - VString
** File description:
** Virtual String / Constant String
** Allow direct use of a chain of characters without allocation or copy
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/
#ifndef VSTRING_HPP_
#define VSTRING_HPP_

#include <cstring>

struct CString;

// Constant string
struct CString {
    constexpr CString(const char *_str, int id = 0) :
        str(_str), size(-1), id(id)
    {
        while (_str[++size]);
    }
    const char *str = nullptr;
    unsigned int size = 0; // Size of the string
    unsigned int id; // Might be externally used for fast identification

    bool operator<(const CString &i2) const {
        if (size < i2.size)
            return (std::memcmp(str, i2.str, size) <= 0);
        return (std::memcmp(str, i2.str, i2.size) < 0);
    }
    bool operator==(const CString &i2) const {
        if (size != i2.size)
            return false;
        return (std::memcmp(str, i2.str, size) == 0);
    }
    bool endWith(const CString &i2) const {
        if (size < i2.size)
            return false;
        return (std::memcmp(str + (size - i2.size), i2.str, i2.size) == 0);
    }
    bool endWith(const char *str2, unsigned int size2) const {
        if (size < size2)
            return false;
        return (std::memcmp(str + (size - size2), str2, size2) == 0);
    }
};

// Virtual string
struct VString {
    char *str = nullptr;
    unsigned int size = 0; // Size of the string
    unsigned int id; // Might be externally used for fast identification

    bool operator<(const CString &i2) const {
        if (size < i2.size)
            return (std::memcmp(str, i2.str, size) <= 0);
        return (std::memcmp(str, i2.str, i2.size) < 0);
    }
    bool operator==(const CString &i2) const {
        if (size != i2.size)
            return false;
        return (std::memcmp(str, i2.str, size) == 0);
    }
    bool operator<(unsigned int i2) const {
        return id < i2;
    }
    bool operator==(unsigned int i2) const {
        return id == i2;
    }
    bool endWith(const CString &i2) const {
        if (size < i2.size)
            return false;
        return (std::memcmp(str + (size - i2.size), i2.str, i2.size) == 0);
    }
    bool endWith(const char *str2, unsigned int size2) const {
        if (size < size2)
            return false;
        return (std::memcmp(str + (size - size2), str2, size2) == 0);
    }
    operator const CString &() const {
        return *(CString *) this;
    }
};

#endif /* VSTRING_HPP_ */
