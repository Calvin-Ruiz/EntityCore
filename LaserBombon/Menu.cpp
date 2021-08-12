#include "Menu.hpp"
#include "Game.hpp"
#include "EntityLib/Core/EntityLib.hpp"
#include "EntityLib/Core/MenuMgr.hpp"
#include "EntityLib/Core/MenuButton.hpp"
#include "EntityLib/Core/MenuImage.hpp"
#include "EntityLib/Core/MenuText.hpp"

Menu::Menu(Game *master, std::shared_ptr<EntityLib> &core, Player &player1, Player &player2, unsigned short &maxLevel, unsigned int &recursion, unsigned char nbPlayer) : master(master), core(core), player1(player1), player2(player2), maxLevel(maxLevel), recursion(recursion), nbPlayer(nbPlayer)
{
}

Menu::~Menu()
{
}

// Coloring : black = default | blue = maxed | green = gain | red = loss or can't affroad

bool Menu::mainloop(int type)
{
    menu = std::make_unique<MenuMgr>(core, this, (type == RESEARCH) ? selectResearchPage : nullptr);
    std::vector<MenuText *> texts;
    std::vector<MenuImage *> images;
    std::vector<MenuButton *> buttons;
    images.push_back(new MenuImage(*menu));
    images[0]->addDependency({nullptr, 0, 0, 0, Align::LEFT | Align::UP});
    images[0]->addDependency({nullptr, 1, 1, 0, Align::RIGHT | Align::DOWN});
    images[0]->load("menu.png");
    // dual equipment submenu or single research menu : NAME IMAGE STATISTICS COST EQUIP/UPGRADE BACK -- all aligned to the name
    switch (type) {
        case GENERAL:
            break;
        case LOAD:
            master->load(0); // load menu is the first mandatory menu
            return true;
            break;
        case EQUIPMENT:
            // dual from here
            break;
        case RESEARCH:
            break;
        case DISMANTLE:
            texts.push_back(new MenuText(*menu));
            texts.back()->setString(std::string("Are you sure to dismantle for research ?\nYour score, level and equipment will be resetted.\nResearch gain depend on your equipment and score.\nYou will get ") + EntityLib::toText(master->getRecursionGain()) + "/" + EntityLib::toText(master->getMaxedRecursionGain()) + "research points.\nAfter dismantle, your score will be of " + EntityLib::toText(master->getScoreAfterRecursion()) + ".");
            texts.back()->addDependency({texts.back(), 0, 0.6, Align::UP, Align::DOWN});
            texts.back()->addDependency({nullptr, 0.5, 0.4, 0, Align::CENTER});
            buttons.push_back(new MenuButton(*menu, this, &backButton, "Cancel", "button.png", 0.01));
            buttons.back()->setColor({240, 64, 64, 255});
            buttons.back()->addDependency({buttons.back(), 0.3, 0.1, Align::UP | Align::LEFT, Align::DOWN | Align::RIGHT});
            buttons.back()->addDependency({nullptr, 0.3, 0.85, 0, Align::CENTER});
            buttons.push_back(new MenuButton(*menu, this, &dismantleButton, "Dismantle", "button.png", 0.01));
            buttons.back()->setColor({64, 240, 64, 255});
            buttons.back()->addDependency({buttons.back(), 0.3, 0.1, Align::UP | Align::LEFT, Align::DOWN | Align::RIGHT});
            buttons.back()->addDependency({buttons[0], 0.7, 0, Align::UP | Align::DOWN, Align::CENTER});
            break;
    }
    return menu->mainloop(type != LOAD);
}

void Menu::backButton(void *data, MenuButton *button)
{
    reinterpret_cast<Menu *>(data)->menu->close();
}

void Menu::dismantleButton(void *data, MenuButton *button)
{
}

void Menu::selectResearchPage(void *data, int pageShift, int player)
{
}
