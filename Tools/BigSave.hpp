/*
** EntityCore
** C++ Tools - BigSave
** File description:
** File-linked SaveData
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/

#ifndef BIG_SAVE_HPP_
#define BIG_SAVE_HPP_

#include "SaveData.hpp"

class BigSave : public SaveData {
public:
    BigSave();
    ~BigSave();
    BigSave(const BigSave &cpy) = delete;
    BigSave &operator=(const BigSave &src) = delete;

    void set(SaveData &src) {
        *(SaveData *) this = src;
    }
    // Return a shared_ptr to a BigSave for a saveName.
    // Two shared pointer acquired this way for the same saveName point to the same BigSave object
    // Otherwise, it is the same as .open(saveName)
    static std::shared_ptr<BigSave> loadShared(const std::string &saveName);
    // _saveAtDestroy : If true, call store() when this object is destroyed
    // reduceWrite : If true, when calling store(), read the file and compare it with the new content. If they are equal, don't write to the file.
    // reducedCheck : If true, assume that content is unchanged if this->get() content is unchanged.
    bool open(const std::string &saveName, bool _saveAtDestroy = true, bool _reduceWrite = true, bool _reducedCheck = false);
    bool store();
    const std::string &getSaveName() const;
private:
    std::string saveName;
    std::vector<char> oldData; // reducedCheck == true, store the attached content at the last open() or store() call
    bool saveAtDestroy = false;
    bool reduceWrite;
    bool reducedCheck;
    static std::map<std::string, std::weak_ptr<BigSave>> subsaves;
};

#endif /* BIG_SAVE_HPP_ */
