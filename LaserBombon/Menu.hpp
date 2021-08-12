#ifndef MENU_HPP_
#define MENU_HPP_

class Game;
class EntityLib;
struct Player;
class MenuMgr;
class MenuButton;
#include <memory>

class Menu {
public:
    enum MenuType {
        GENERAL,
        LOAD,
        EQUIPMENT,
        RESEARCH,
        DISMANTLE
    };
    Menu(Game *master, std::shared_ptr<EntityLib> &core, Player &player1, Player &player2, unsigned short &maxLevel, unsigned int &recursion, unsigned char nbPlayer);
    virtual ~Menu();
    Menu(const Menu &cpy) = delete;
    Menu &operator=(const Menu &src) = delete;

    bool mainloop(int type);
private:
    // BUTTON ACTIONS
    static void selectResearchPage(void *data, int pageShift, int player);
    static void selectEquipmentPage(void *data, int pageShift, int player);
    static void equipButton(void *data, MenuButton *button);
    static void researchButton(void *data, MenuButton *button);
    static void backButton(void *data, MenuButton *button);
    static void mainMenuButton(void *data, MenuButton *button);
    static void equipMenuButton(void *data, MenuButton *button);
    static void dismantleButton(void *data, MenuButton *button);

    // RESOURCES
    Game *master;
    std::shared_ptr<EntityLib> core;
    Player &player1;
    Player &player2;
    std::unique_ptr<MenuMgr> menu;
    unsigned short &maxLevel;
    unsigned int &recursion;
    unsigned char nbPlayer;
    int page1 = 0;
    int page2 = 0;
    float textHeight = 0.05;
};

#endif /* MENU_HPP_ */
