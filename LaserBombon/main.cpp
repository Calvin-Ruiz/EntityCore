/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** main.cpp
*/
#include <iostream>
#include <string>
#include "Game.hpp"
#include "Menu.hpp"

int main(int argc, char const *argv[])
{
    Game game("Laser Bombon", 1, 1280, 720);

    game.init();
    if (game.openMenu(Menu::LOAD))
        game.mainloop();
    return 0;
}
