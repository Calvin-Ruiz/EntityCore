/*
** EPITECH PROJECT, 2020
** G-JAM-001-MAR-0-2-jam-killian.moudery
** File description:
** DataPack.hpp
*/

#ifndef DATAPACK_HPP_
#define DATAPACK_HPP_

#include <memory>
#include <vector>

template <class T>
class DataPack {
public:
    DataPack(std::vector<char> &datas) : datas(datas), data(reinterpret_cast<T *>(datas.data())), sz(datas.size() / sizeof(T)) {}
    virtual ~DataPack() = default;
    DataPack(const DataPack &cpy) = default;
    DataPack &operator=(const DataPack &src) = default;

    // Extract data, return true if datas have been written to ptr
    bool pop(T &ptr) {
        if (sz-- <= 0)
            return false;
        ptr = *(data++);
        return true;
    }
    // Insert data
    void push(const T &data) {
        const int tmp = datas.size();
        datas.resize(tmp + sizeof(T));
        *reinterpret_cast<T *>(datas.data() + tmp) = data;
    }
    // return full data size
    int size() const {return sz;}
private:
    std::vector<char> &datas;
    T *data;
    int sz;
};

#endif /* DATAPACK_HPP_ */
