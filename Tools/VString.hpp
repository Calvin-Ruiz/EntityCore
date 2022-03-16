/*
** EPITECH PROJECT, 2020
** B-MAT-500-MAR-5-1-305construction-calvin.ruiz
** File description:
** VString.hpp
*/

#ifndef VSTRING_HPP_
#define VSTRING_HPP_

#include <cstring>

struct VString {
    char *str = nullptr;
    unsigned int size = 0; // Size of the string
    unsigned int id; // Might be externally used for fast identification

    bool operator<(const VString &i2) const {
        if (size < i2.size)
            return (std::memcmp(str, i2.str, size) <= 0);
        return (std::memcmp(str, i2.str, i2.size) < 0);
    }
    bool operator==(const VString &i2) const {
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
};

#endif /* VSTRING_HPP_ */
