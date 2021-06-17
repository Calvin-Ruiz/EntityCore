/*
** EPITECH PROJECT, 2020
** B-OOP-400-MAR-4-1-arcade-calvin.ruiz
** File description:
** Save.hpp
*/

#ifndef SAVE_HPP_
#define SAVE_HPP_

#include <string>
#include <map>
#include <vector>

class Save {
public:
    Save();
    virtual ~Save();
    Save(const Save &cpy) = delete;
    Save &operator=(const Save &src) = delete;

    std::vector<char> &operator[](const std::string &key);
    void open(const std::string &saveName); // Load save datas
    void truncate(); // Remove every stored datas
    int load(const std::string &name, void *data);
    void save(const std::string &name, const void *data, int size);
    void store();
    const std::string &getSaveName() const;
private:
    std::string saveName;
    std::map<std::string, std::vector<char>> datas;
};

#endif /* SAVE_HPP_ */
