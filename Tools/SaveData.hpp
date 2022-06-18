/*
** EntityCore
** C++ Tools - SaveData
** File description:
** Serializable arborescent binary container
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/

#ifndef SAVE_DATA_HPP_
#define SAVE_DATA_HPP_

// #define NO_SAVEDATA_CONCEPT
// Disable the use of C++20 concepts
// #define NO_SAVEDATA_IMPLICIT
// Disable implicits assignments, heavily recommended when using NO_SAVEDATA_CONCEPT

// #define NO_SAVEDATA_THROW
// Remove all throw, mostly usefull to compile on Android

// #define NO_SAVEDATA_ADVANCED_TYPES
// Disable advanced type, so :
// The only types used on save will be STRING_MAP, ADDRESS_MAP and LIST
// This flag is usefull when backward compatibility or compatibility with partial ports of SaveData is required

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
    CHAR_MAP = 0x40,
    SHORT_MAP = 0x41,
    INT_MAP = 0x42,
    WIDE_LIST = 0x43,
    // Attached data size length, UNDEFINED for no attached data
    CHAR_SIZE = 0x04,
    SHORT_SIZE = 0x08,
    INT_SIZE = 0x0c,
    // Attached data type, for special data only
    SUBFILE = 0x10,
    SAVABLE_OBJECT = 0x20,
    // Use an extended type
    EXTENDED_TYPE = 0x80,
};

#define TYPE_MASK 0x43
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
    template <typename T>
    #ifndef NO_SAVEDATA_CONCEPT
    requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    #endif
    explicit SaveData(const T &value) {
        raw.resize(sizeof(T));
        *reinterpret_cast<T *>(raw.data()) = value;
    }
    ~SaveData();

    const std::string &operator=(const std::string &content);
    #ifndef NO_SAVEDATA_IMPLICIT
    template <typename T>
    #ifndef NO_SAVEDATA_CONCEPT
    requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    #endif
    const T &operator=(const T &value) {
        raw.resize(sizeof(T));
        *reinterpret_cast<T *>(raw.data()) = value;
        return value;
    }
    #endif
    SaveData &operator[](const std::string &key);
    SaveData &operator[](uint64_t address);
    int push(const SaveData &data = {}); // return new index
    #ifndef NO_SAVEDATA_IMPLICIT
    template <typename T>
    #ifndef NO_SAVEDATA_CONCEPT
    requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    #endif
    int push(const T &value) {
        return push(SaveData(value));
    }
    #endif
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
    template<typename T>
    #ifndef NO_SAVEDATA_CONCEPT
    requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    #endif
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
    #ifndef NO_SAVEDATA_IMPLICIT
    template <typename T>
    #ifndef NO_SAVEDATA_CONCEPT
    requires std::is_trivially_destructible_v<T> && std::is_copy_assignable_v<T>
    #endif
    operator T&() {
        if (raw.empty()) {
            raw.resize(sizeof(T));
            *reinterpret_cast<T *>(raw.data()) = T{};
        }
        assert(raw.size() == sizeof(T));
        return *reinterpret_cast<T *>(raw.data());
    }
    #endif
    void load(char *&data);
    void save(std::vector<char> &data);
    // Return the number of elements directly attached to this SaveData
    size_t size();
    // Compute and return the serialized size of this SaveData (include every SaveData attached to this one)
    size_t computeSize();
    // Recursively dump the content of a SaveData for debug purpose, may attach a debug dumper
    // Spacing : number of spaces per level
    // dumpContent : specialized dumpContent for non-string entries, return true if writing to out, false to use default interpretation
    // dumpOverride : specialized dumpContent, return true if writing to out, overriding the implicit dump content deduction
    void debugDump(std::ostream &out, int spacing = 2, bool (*dumpContent)(const std::vector<char> &data, std::ostream &out) = nullptr, int level = 0, bool (*dumpOverride)(const std::vector<char> &data, std::ostream &out, int spacing, int level) = nullptr);
    // Recursively dump the content of a SaveData for debug purpose, must attach a debug dumper
    // dumpOverride : specialized dumpContent for every entries, return true if writing to out, false to use default interpretation
    // Spacing : number of spaces per level
    void debugDump(std::ostream &out, bool (*dumpOverride)(const std::vector<char> &data, std::ostream &out, int spacing, int level), int spacing = 2);
    // Serialize this SaveData at this location
    // Will cause UNDEFINED BEHAVIOUR if computeSize() have not been called after the last modification
    void save(char *data);
private:
    static void genericDumpContent(const std::vector<char> &data, std::ostream &out, bool (*specializedDumpContent)(const std::vector<char> &data, std::ostream &out));
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
