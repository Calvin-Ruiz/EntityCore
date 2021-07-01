#include "Menu.hpp"
#include "Game.hpp"
#include "EntityLib/Core/EntityLib.hpp"

Menu::Menu(Game *master, EntityLib &core, Player &player1, Player &player2, int &maxLevel, unsigned int &recursion, unsigned char nbPlayer) : master(master), core(core), player1(player1), player2(player2), maxLevel(maxLevel), recursion(recursion), nbPlayer(nbPlayer)
{
}

Menu::~Menu()
{
}

// Coloring : black = default | blue = maxed | green = gain | red = loss or can't affroad

bool Menu::mainloop(int type)
{
    switch (type) {
        case GENERAL:
            break;
        case LOAD:
            master->load(0);
            break;
        case EQUIPMENT:
            break;
        case RESEARCH:
            break;
        case DISMANTLE:
            break;
    }
    return true;
}
