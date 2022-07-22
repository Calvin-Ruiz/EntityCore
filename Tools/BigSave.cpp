/*
** EntityCore
** C++ Tools - BigSave
** File description:
** File-linked SaveData
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/
#include "BigSave.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <functional>

std::map<std::string, std::weak_ptr<BigSave>> BigSave::subsaves;
bool BigSave::lsSaveAtDestroy = true;
bool BigSave::lsReducedWrite = true;
bool BigSave::lsReducedCheck = false;

BigSave::BigSave()
{}

BigSave::~BigSave()
{
    if (saveAtDestroy)
        store();
}

bool BigSave::open(const std::string &name, bool _saveAtDestroy, bool _reduceWrite, bool _reducedCheck)
{
    std::ifstream file(name + ".sav", std::ifstream::binary);
    size_t size;
    std::vector<char> data;

    saveName = name;
    saveAtDestroy = _saveAtDestroy;
    reduceWrite = _reduceWrite;
    reducedCheck = _reducedCheck;
    if (!file || !file.read((char *) &size, sizeof(size_t)))
        return false;

    data.resize(size);
    if (!file.read(data.data(), size)) {
        std::cerr << "Warning : Save file is truncated or corrupted\n";
        return false;
    }
    char *pData = data.data();
    load(pData);
    if (pData != data.data() + data.size()) {
        #ifndef NO_SAVEDATA_THROW
        throw std::range_error("Theoric size and bloc size missmatch");
        #endif
    }
    if (reducedCheck)
        oldData = get();
    return true;
}

bool BigSave::store()
{
    std::vector<char> data;
    auto flags = std::ofstream::binary | std::ofstream::trunc;
    if (reduceWrite) {
        if (reducedCheck) {
            if (oldData.size() == get().size() && std::memcmp(oldData.data(), get().data(), oldData.size()) == 0)
                return true; // Assume nothing has changed
            oldData = get();
        }
        std::ifstream file(saveName + ".sav", std::ifstream::binary);
        if (file) {
            save(data);
            size_t oldSize;
            file.read((char *) &oldSize, sizeof(oldSize));
            if (oldSize <= data.size()) {
                flags = std::ofstream::binary;
                if (!reducedCheck && oldSize == data.size()) {
                    oldData.resize(data.size());
                    file.read(oldData.data(), data.size());
                    if (std::memcmp(data.data(), oldData.data(), data.size()) == 0)
                        return true;
                }
            }
        }
    }
    std::ofstream file(saveName + ".sav", flags);
    if (file) {
        if (data.empty())
            save(data);
        const size_t size = data.size();
        file.write((char *) &size, sizeof(size));
        file.write(data.data(), size);
    }
    return (file.good());
}

const std::string &BigSave::getSaveName() const
{
    return saveName;
}

std::shared_ptr<BigSave> BigSave::loadShared(const std::string &saveName)
{
    auto &ptr = subsaves[saveName];
    auto ret = ptr.lock();
    if (!ret) {
        ptr = ret = std::make_shared<BigSave>();
        ret->open(saveName, lsSaveAtDestroy, lsReducedWrite, lsReducedCheck);
    }
    return ret;
}
