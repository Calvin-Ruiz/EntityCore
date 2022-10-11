/*
** EntityCore
** C++ Tools - PySaveData
** File description:
** Python binding for SaveData and BigSave
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/

#ifndef PY_SAVEDATA_HPP_
#define PY_SAVEDATA_HPP_

#include <cinttypes>

#ifdef WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// Python interface for SaveData
extern "C" {
    EXPORT void *sd_new();
    EXPORT void sd_delete(void *self);
    EXPORT void *sd_get(void *self, int size);
    EXPORT void *sd_strmap(void *self, const char *str);
    EXPORT void *sd_nbrmap(void *self, uint64_t value);
    EXPORT void *sd_getraw(void *self, int *size);
    EXPORT void *sd_file(void *self, const char *str);
    EXPORT void sd_close(void *self);
    EXPORT int sd_size(void *self);
    EXPORT void sd_copy(void *self, void *from);
    EXPORT int sd_push(void *self, void *sd);
    EXPORT void sd_set(void *self, void *data, int size);
    EXPORT uint64_t sd_computesize(void *self);
    EXPORT void sd_save(void *self, void *out);
    EXPORT void sd_load(void *self, void *data);
    EXPORT void sd_truncate(void *self);
    EXPORT void sd_reset(void *self);
    EXPORT const char *sd_repr(void *self);

    EXPORT void *bs_new();
    EXPORT void bs_delete(void *self);
    EXPORT bool bs_open(void *self, const char *saveName, bool _saveAtDestroy, bool _reduceWrite, bool _reducedCheck);
    EXPORT bool bs_store(void *self);

    extern void *dump_function;
};

#endif /* end of include guard: PY_SAVEDATA_HPP_ */
