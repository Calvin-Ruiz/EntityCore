/*
** EntityCore
** C++ Tools - PySaveData
** File description:
** Python binding for SaveData and BigSave
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/

#include "PySaveData.hpp"
#include "BigSave.hpp"
#include <sstream>
#include <cstring>

#define sd (*(SaveData *) self)

static std::string lastDump;
void *dump_function = nullptr;

void *sd_new()
{
    return new SaveData();
}

void sd_delete(void *self)
{
    delete (SaveData *) self;
}

void *sd_get(void *self, int size)
{
    auto &vec = sd.get();
    if (vec.size() < (unsigned int) size)
        vec.resize(size);
    return vec.data();
}

void *sd_strmap(void *self, const char *str)
{
    return &sd[str];
}

void *sd_nbrmap(void *self, uint64_t value)
{
    return &sd[value];
}

void *sd_getraw(void *self, int *size)
{
    auto &vec = sd.get();
    *size = vec.size();
    return vec.data();
}

int sd_size(void *self)
{
    return sd.size();
}

void sd_copy(void *self, void *from)
{
    sd = (SaveData *) from;
}

int sd_push(void *self, void *data)
{
    return sd.push(*(SaveData *) data);
}

void sd_set(void *self, void *data, int size)
{
    auto &vec = sd.get();
    vec.resize(size);
    std::memcpy(vec.data(), data, size);
}

uint64_t sd_computesize(void *self)
{
    return sd.computeSize();
}

void sd_save(void *self, void *out)
{
    sd.save((char *) out);
}

void sd_load(void *self, void *data)
{
    sd.load((char *&) data);
}

void sd_truncate(void *self)
{
    sd.truncate();
}

void sd_reset(void *self)
{
    sd.reset();
}

const char *sd_repr(void *self)
{
    std::ostringstream oss;
    sd.debugDump(oss, 4, (dump_function_t) dump_function);
    lastDump = std::move(oss.str());
    return lastDump.c_str();
}

void *sd_file(void *self, const char *str)
{
    if (str) {
        return &sd.file(str);
    } else {
        return &sd.file();
    }
}

void sd_close(void *self)
{
    sd.close();
}

void *bs_new()
{
    return new BigSave();
}

void bs_delete(void *self)
{
    delete (BigSave *) self;
}

bool bs_open(void *self, const char *saveName, bool _saveAtDestroy, bool _reduceWrite, bool _reducedCheck)
{
    return ((BigSave *) self)->open(saveName, _saveAtDestroy, _reduceWrite, _reducedCheck);
}

bool bs_store(void *self)
{
    return ((BigSave *) self)->store();
}
