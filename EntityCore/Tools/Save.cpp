/*
** EPITECH PROJECT, 2020
** B-OOP-400-MAR-4-1-arcade-calvin.ruiz
** File description:
** Save.cpp
*/
#include "Save.hpp"
#include <fstream>
#include <iostream>
#include <cstring>

Save::Save()
{}

Save::~Save()
{}

void Save::open(const std::string &name)
{
    std::ifstream file(name + ".sav", std::ifstream::binary);
    unsigned short size = 0;
    unsigned short entryCount = 0;
    unsigned short i = 0;
    std::vector<char> data;

    datas.clear();
    saveName = name;
    if (file)
        file.read((char *) &entryCount, 2);
    while (file && i++ < entryCount) {
        file.read((char *) &size, 2);
        data.resize(size);
        file.read(data.data(), size);
        const std::string key(data.data(), size);

        file.read((char *) &size, 2);
        data.resize(size);
        data.shrink_to_fit();
        file.read(data.data(), size);
        datas[key].swap(data);
    }
    if (i < entryCount)
        std::cerr << "Warning : Save file is truncated (expected " << entryCount << " entries, got " << i << ")\n";
}

void Save::truncate()
{
    datas.clear();
}

int Save::load(const std::string &name, void *data)
{
    if (data)
        memcpy(data, datas[name].data(), datas[name].size());
    return datas[name].size();
}

void Save::save(const std::string &name, const void *data, int size)
{
    datas[name].resize(size);
    memcpy(datas[name].data(), data, size);
}

void Save::store()
{
    std::ofstream file(saveName + ".sav", std::ofstream::binary | std::ofstream::trunc);
    unsigned short size = 0;

    if (file) {
        size = datas.size();
        file.write((char *) &size, 2);
        for (auto &d : datas) {
            size = d.first.size();
            file.write((char *) &size, 2);
            file.write(d.first.c_str(), size);
            size = d.second.size();
            file.write((char *) &size, 2);
            file.write((char *) d.second.data(), size);
        }
    }
}

std::vector<char> &Save::operator[](const std::string &key)
{
    return datas[key];
}

const std::string &Save::getSaveName() const
{
    return saveName;
}
