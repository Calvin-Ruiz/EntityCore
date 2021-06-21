/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** main.cpp
*/
#include <iostream>
#include <string>
#include "Game.hpp"

int main(int argc, char const *argv[])
{
    Game game("Laser Bombon", 1, 960, 540);

    game.init();
    game.load(0);
    game.mainloop();
    return 0;
}
