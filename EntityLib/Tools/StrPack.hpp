/*
** EPITECH PROJECT, 2020
** G-JAM-001-MAR-0-2-jam-killian.moudery
** File description:
** StrPack.hpp
*/

#ifndef STRPACK_HPP_
#define STRPACK_HPP_

#include <memory>
#include <vector>
#include <string>

template <class T = unsigned char>
class StrPack {
public:
    StrPack(std::vector<char> &datas) : datas(datas), sz(datas.size() / sizeof(T)) {}
    virtual ~StrPack() = default;
    StrPack(const StrPack &cpy) = default;
    StrPack &operator=(const StrPack &src) = default;

    // Extract the next string, return string length (string is not null-terminated) or -1 if no more string can be extracted
    T pop(char &*ptr) {
        if (offset == datas.size())
            return -1;
        T value = *reinterpret_cast<T *>(datas.data() + offset);
        ptr = datas.data() + offset + sizeof(T);
        offset += value + sizeof(T);
        return value;
    }
    // Extract the next string and return it
    // If there is no more entries, return an empty string
    std::string pop() {
        if (offset == datas.size())
            return std::string();
        T value = *reinterpret_cast<T *>(datas.data() + offset);
        std::string str(datas.data() + offset + sizeof(T), value);
        offset += value + sizeof(T);
        return str;
    }
    // plan inserting datas, return address were a data of length bytes is reserved
    char *prepush(int length) {
        const int tmp = datas.size();
        datas.resize(tmp + sizeof(T) + length);
        *reinterpret_cast<T *>(datas.data() + tmp) = length;
        table.push_back(tmp);
        return datas.data() + tmp + sizeof(T);
    }
    // Insert data
    void push(char *str, int length) {
        const int tmp = datas.size();
        datas.resize(tmp + sizeof(T) + length);
        *reinterpret_cast<T *>(datas.data() + tmp) = length;
        memcpy(datas.data() + tmp + sizeof(T), str, length);
        table.push_back(tmp);
    }
    // Insert data
    void push(const std::string &str) {
        const int tmp = datas.size();
        datas.resize(tmp + sizeof(T) + str.size());
        *reinterpret_cast<T *>(datas.data() + tmp) = str.size();
        memcpy(datas.data() + tmp + sizeof(T), str.c_str(), str.size());
        table.push_back(tmp);
    }
    // Scan content to allow any operation other than pop and push
    // Call once after loading datas in the handled data vector
    void scan() {
        register int pos = 0;
        const register int fullSize = datas.size();
        table.clear();
        while (pos != fullSize) {
            table.push_back(pos);
            pos += *reinterpret_cast<T *>(datas.data() + pos) + sizeof(T);
        }
    }
    // Return element at this index
    T at(char &*ptr, int index) {
        T value = *reinterpret_cast<T *>(datas.data() + table[index]);
        ptr = datas.data() + table[index] + sizeof(T);
        return value;
    }
    // Rewrite content at index with a content of different length
    // This is a heavy operation, if the string length is unchanged, use at() and write you new string directly
    void rewrite(const std::string &str, int index) {
        if (index == table.size() - 1) {
            datas.resize(table.back());
            table.pop_back();
            push(str);
            return;
        }
        int tmp = str.size() - table[index + 1] + table[index];
        datas.resize(datas.size() + tmp);
        memmove(datas.data() + table[index + 1] + tmp, datas.data() + table[index + 1], datas.size() - tmp - table[index + 1]);
        *reinterpret_cast<T *>(datas.data() + table[index]) = str.size();
        memcpy(datas.data() + sizeof(T), str.c_str(), str.size());
        while (index < table.size())
            table[index++] += tmp;
    }
    // Rewrite content at index with a content of different length
    // This is a heavy operation, if the string length is unchanged, use at() and write you new string directly
    void rewrite(char *ptr, int size, int index) {
        if (index == table.size() - 1) {
            datas.resize(table.back());
            table.pop_back();
            push(ptr, size);
            return;
        }
        int tmp = size - table[index + 1] + table[index];
        datas.resize(datas.size() + tmp);
        memmove(datas.data() + table[index + 1] + tmp, datas.data() + table[index + 1], datas.size() - tmp - table[index + 1]);
        *reinterpret_cast<T *>(datas.data() + table[index]) = size;
        memcpy(datas.data() + sizeof(T), ptr, size);
        while (index < table.size())
            table[index++] += tmp;
    }
    // return the number of string hold
    int size() const {return table.size();}
private:
    std::vector<char> &datas;
    std::vector<unsigned int> table;
    int offset = 0;
};

#endif /* STRPACK_HPP_ */
