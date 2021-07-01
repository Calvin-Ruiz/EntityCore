#ifndef MENU_HPP_
#define MENU_HPP_

class Game;
class EntityLib;
struct Player;

class Menu {
public:
    enum MenuType {
        GENERAL,
        LOAD,
        EQUIPMENT,
        RESEARCH,
        DISMANTLE
    };
    Menu(Game *master, EntityLib &core, Player &player1, Player &player2, int &maxLevel, unsigned int &recursion, unsigned char nbPlayer);
    virtual ~Menu();
    Menu(const Menu &cpy) = delete;
    Menu &operator=(const Menu &src) = delete;

    bool mainloop(int type);
private:
    Game *master;
    EntityLib &core;
    Player &player1;
    Player &player2;
    int &maxLevel;
    unsigned int &recursion;
    unsigned char nbPlayer;
};

#endif /* MENU_HPP_ */
