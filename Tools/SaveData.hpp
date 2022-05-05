/*
** EPITECH PROJECT, 2021
** Global Tools
** File description:
** SaveData.hpp
*/

#ifndef SAVE_DATA_HPP_
#define SAVE_DATA_HPP_

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <ostream>
#include <cassert>

enum SaveSection {
    UNDEFINED,
    // Container type, UNDEFINED if none of them
    STRING_MAP = 0x01,
    ADDRESS_MAP = 0x02,
    LIST = 0x03,
    // Attached data size length, UNDEFINED for no attached data
    CHAR_SIZE = 0x04,
    SHORT_SIZE = 0x08,
    INT_SIZE = 0x0c,
    // Attached data type, for special data only
    SUBFILE = 0x10,
    SAVABLE_OBJECT = 0x20,
};

#define TYPE_MASK 0x03
#define SIZE_MASK 0x0c
#define SPECIAL_MASK 0x30

class BigSave;
class SaveData;

class SavedObject {
public:
    virtual ~SavedObject();
    // Ask the SavedObject to save datas he need to reconstruct him in the SaveData he come from, which is given as argument
    virtual void save(SaveData *from) = 0;
};

class SaveData {
public:
    SaveData();
    SaveData(const std::string &content);
    template <typename T> requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    SaveData(const T &value) {
        raw.resize(sizeof(T));
        *reinterpret_cast<T *>(raw.data()) = value;
    }
    ~SaveData();

    const std::string &operator=(const std::string &content);
    template <typename T> requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    const T &operator=(const T &value) {
        raw.resize(sizeof(T));
        *reinterpret_cast<T *>(raw.data()) = value;
    }
    SaveData &operator[](const std::string &key);
    SaveData &operator[](uint64_t address);
    int push(const SaveData &data = {}); // return new index
    BigSave &file(const std::string &filename);
    BigSave &file();
    void close();
    // Get a savable object, load it if it doesn't exist yet
    template<typename T>
    std::shared_ptr<T> &obj() {
        if (!object) {
            object = std::make_shared<T>(this);
        }
        return object;
    }
    std::vector<SaveData> &getList() {return arr;}
    void truncate(); // Discard content attached to it (except raw)
    void reset(); // Discard content, type and attached datas
    SaveSection getType() const {return (SaveSection) type;}
    bool nonEmpty() const;
    bool empty() const;
    std::vector<char> &get() {return raw;}
    template<typename T> requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    T &get(const T &defaultValue = {}) {
        if (raw.empty()) {
            raw.resize(sizeof(T));
            *reinterpret_cast<T *>(raw.data()) = defaultValue;
        }
        return *reinterpret_cast<T *>(raw.data());
    }
    operator std::string() {
        return std::string(raw.data(), raw.size());
    }
    operator std::vector<SaveData>&() {return arr;}
    template <typename T> requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    operator T&() {
        if (raw.empty()) {
            raw.resize(sizeof(T));
            *reinterpret_cast<T *>(raw.data()) = T{};
        }
        assert(raw.size() == sizeof(T));
        return *reinterpret_cast<T *>(raw.data());
    }
    void load(char *&data);
    void save(std::vector<char> &data);
    size_t computeSize();
    // Recursively dump the content of a SaveData for debug purpose, may attach a debug dumper
    // Spacing : number of spaces per level
    // dumpContent : specialized dumpContent for non-string entries, return true if writing to out, false to use default interpretation
    void debugDump(std::ostream &out, int spacing = 2, bool (*dumpContent)(const std::vector<char> &data, std::ostream &out) = nullptr, int level = 0);
private:
    static void genericDumpContent(const std::vector<char> &data, std::ostream &out, bool (*specializedDumpContent)(const std::vector<char> &data, std::ostream &out));
    void save(char *data);
    inline size_t getSize() const {return dataSize;}
    unsigned char type = SaveSection::UNDEFINED;
    unsigned char sizeType = SaveSection::UNDEFINED;
    unsigned char specialType = SaveSection::UNDEFINED;
    size_t dataSize;

    std::map<std::string, SaveData> str;
    std::map<uint64_t, SaveData> addr;
    std::vector<SaveData> arr;

    std::vector<char> raw; // Can hold raw data or big save data
    std::shared_ptr<SavedObject> object;
    std::shared_ptr<BigSave> subsave;
};

#endif /* SAVE_DATA_HPP_ */
