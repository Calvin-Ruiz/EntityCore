/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** Game.cpp
*/
#include "Game.hpp"
#include "EntityLib/Core/EntityLib.hpp"
#include "EntityLib/Core/GPUDisplay.hpp"
#include "EntityLib/Core/GPUEntityMgr.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>

Game::Game(const std::string &name, uint32_t version, int width, int height)
{
    bool enableDebugLayers = false;
    bool drawLogs = false;
    bool saveLogs = false;

    std::ifstream config("config.txt");
    std::string str1, unused;
    while (config) {
        int value = -1;
        config >> str1 >> unused >> value;
        if (value == -1)
            break;
        if (str1 == "debug")
            drawLogs = value;
        if (str1 == "writeLog")
            saveLogs = value;
        if (str1 == "debugLayer")
            enableDebugLayers = value;
    }
    core = std::make_shared<EntityLib>(name.c_str(), version, width, height, enableDebugLayers, drawLogs, saveLogs);
    compute = std::make_unique<GPUEntityMgr>(core);
    display = std::make_unique<GPUDisplay>(core, *compute);
    compute->init();
}

Game::~Game()
{}

void Game::init()
{
    core->loadFragment(LASERIFIER, 32, 128, 96, 148, 1, 1, 0, 0, F_OTHER);
    core->loadFragment(MIEL0, 0, 0, 16, 16, 6, 1, 0, 0, F_CANDY);
    core->loadFragment(MIEL1, 16, 0, 32, 16, 8, 1, 0, 0, F_MIEL1);
    core->loadFragment(MIEL2, 32, 0, 48, 16, 10, 1, 0, 0, F_MIEL2);
    core->loadFragment(MIEL3, 48, 0, 64, 16, 12, 1, 0, 0, F_MIEL3);
    core->loadFragment(MIEL4, 64, 0, 80, 16, 14, 1, 0, 0, F_MIEL4);
    core->loadFragment(MIEL5, 80, 0, 96, 16, 16, 1, 0, 0, F_MIEL5);
    core->loadFragment(LINE1, 0, 16, 16, 32, 2, 1, 0, 0, F_LINE);
    core->loadFragment(LINE2, 16, 16, 32, 32, 2, 1, 0, 0, F_LINE);
    core->loadFragment(LINE3, 32, 16, 48, 32, 2, 1, 0, 0, F_LINE);
    core->loadFragment(LINE4, 48, 16, 64, 32, 2, 1, 0, 0, F_LINE);
    core->loadFragment(MULTICOLOR, 64, 16, 80, 32, 2, 1, 0, 0, F_MULTICOLOR);
    core->loadFragment(CHOCOLAT, 80, 16, 96, 32, 1000000, 2, 0, 0, F_OTHER);
    core->loadFragment(CANDY1, 0, 32, 16, 48, 1, 1, 0, 0, F_CANDY);
    core->loadFragment(CANDY2, 16, 32, 32, 48, 1, 1, 0, 0, F_CANDY);
    core->loadFragment(CANDY3, 32, 32, 48, 48, 1, 1, 0, 0, F_CANDY);
    core->loadFragment(CANDY4, 48, 32, 64, 48, 1, 1, 0, 0, F_CANDY);
    core->loadFragment(BOMB1, 0, 48, 16, 64, 5, 1, 0, 0, F_CANDY);
    core->loadFragment(BOMB2, 16, 48, 32, 64, 5, 1, 0, 0, F_CANDY);
    core->loadFragment(BOMB3, 32, 48, 48, 64, 5, 1, 0, 0, F_CANDY);
    core->loadFragment(BOMB4, 48, 48, 64, 64, 5, 1, 0, 0, F_CANDY);
    core->loadFragment(BLOCK1, 64, 32, 80, 48, 600, 1, 0, 0, F_BLOCK);
    core->loadFragment(BLOCK2, 80, 32, 96, 48, 600, 1, 0, 0, F_BLOCK);
    core->loadFragment(BLOCK3, 64, 48, 80, 64, 600, 1, 0, 0, F_BLOCK);
    core->loadFragment(BLOCK4, 80, 48, 96, 64, 600, 1, 0, 0, F_BLOCK);
    core->loadFragment(FISH1, 0, 64, 16, 72, 2, 1, 0, 0, F_CANDY);
    core->loadFragment(FISH2, 16, 72, 32, 72, 2, 1, 0, 0, F_CANDY);
    core->loadFragment(FISH3, 0, 64, 16, 80, 2, 1, 0, 0, F_CANDY);
    core->loadFragment(FISH4, 16, 72, 32, 80, 2, 1, 0, 0, F_CANDY);
    core->loadFragment(BIG_LASER, 32, 64, 64, 80, 7, 1, 0, 0, F_OTHER);
    core->loadFragment(BIG_LASER2, 64, 64, 96, 80, 100, 4, 64, 32, F_OTHER);
    core->loadFragment(CLASSIC, 0, 80, 16, 96, 1, 1, 0, 0, F_PLAYER);
    core->loadFragment(BETTER, 16, 80, 32, 96, 1, 1, 0, 0, F_PLAYER);
    core->loadFragment(AERODYNAMIC, 32, 80, 48, 96, 1, 1, 0, 0, F_PLAYER);
    core->loadFragment(OPTIMAL, 48, 80, 64, 96, 1, 1, 0, 0, F_PLAYER);
    core->loadFragment(ENERGISED, 64, 80, 80, 96, 1, 1, 0, 0, F_PLAYER);
    core->loadFragment(BOOSTED, 80, 80, 96, 96, 1, 1, 0, 0, F_PLAYER);
    core->loadFragment(LASER, 0, 96, 16, 112, 1, 1, 0, 0, F_OTHER);
    core->loadFragment(LASER2, 16, 96, 32, 112, 30, 1, 0, 0, F_OTHER);
    core->loadFragment(ECLAT0, 32, 96, 48, 112, 1, 1, 0, 0, F_OTHER);
    core->loadFragment(ECLAT1, 48, 96, 64, 112, 1, 1, 0, 0, F_OTHER);
    core->loadFragment(ECLAT2, 64, 96, 80, 112, 1, 1, 0, 0, F_OTHER);
    core->loadFragment(ECLAT3, 80, 96, 96, 112, 1, 1, 0, 0, F_OTHER);
    core->loadFragment(BONUS0, 0, 112, 8, 120, 1, 0, 0, 0, F_PIECE_1);
    core->loadFragment(BONUS1, 8, 112, 16, 120, 1, 0, 0, 0, F_PIECE_5);
    core->loadFragment(BONUS2, 16, 112, 24, 120, 1, 0, 0, 0, F_PIECE_10);
    core->loadFragment(BONUS3, 24, 112, 32, 120, 1, 0, 0, 0, F_PIECE_25);
    core->loadFragment(BONUS4, 32, 112, 48, 120, 1, 0, 0, 0, F_SHIELD_BOOST);
    core->loadFragment(BONUS5, 0, 120, 16, 128, 1, 0, 0, 0, F_SPECIAL_BOOST);
    core->loadFragment(BONUS6, 16, 120, 32, 128, 1, 0, 0, 0, F_COOLANT_BOOST);
    core->loadFragment(BLASTER, 32, 120, 48, 128, 2, 1, 0, 0, F_OTHER);
    core->loadFragment(ECLATANT, 48, 112, 64, 128, 1, 1, 0, 0, F_ECLATANT);
    core->loadFragment(PROTECTO, 64, 112, 80, 128, 4, 1, 0, 0, F_OTHER);
    core->loadFragment(STRONG_WAVE, 80, 112, 96, 128, 2, 1, 0, 0, F_OTHER);
    core->loadFragment(WAVE, 0, 128, 8, 144, 1, 1, 0, 0, F_OTHER);
    core->loadFragment(BIG_WAVE, 16, 128, 32, 160, 4, 1, 0, 0, F_OTHER);
    core->loadFragment(MINI_TRANS, 8, 128, 16, 134, 1000000, 1, 0, 0, F_OTHER);
    core->loadFragment(TRANS, 0, 144, 16, 160, 1000000, 1, 0, 0, F_OTHER);
    core->loadFragment(SUPER_TRANS, 0, 160, 32, 184, 1000000, 1, 0, 0, F_OTHER);
    display->pause();
    display->start();
    float ldiag = cos(3.1415926/8+3.1415926/4)*7;
    float diag = cos(3.1415926/4)*7;
    float bdiag = cos(3.1415926/8)*7;
    circle[0] = 7;
    circle[1] = bdiag;
    circle[2] = diag;
    circle[3] = ldiag;
    circle[4] = 0;
    circle[5] = -ldiag;
    circle[6] = -diag;
    circle[7] = -bdiag;
    circle[8] = -7;
    circle[9] = -bdiag;
    circle[10] = -diag;
    circle[11] = -ldiag;
    circle[12] = 0;
    circle[13] = ldiag;
    circle[14] = diag;
    circle[15] = bdiag;
    player1.ptr = display->getJaugePtr();
    player2.ptr = player1.ptr + 9;
}

void Game::mainloop()
{
    SDL_Event event;
    auto delay = std::chrono::duration<int, std::ratio<1,1000000>>(1000000/100);
    auto clock = std::chrono::system_clock::now();
    while (notQuitting) {
        gameStart();
        while (alive & someone) {
            clock += delay;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        alive = false;
                        break;
                    case SDL_KEYDOWN:
                        switch (event.key.keysym.scancode) {
                            case SDL_SCANCODE_F4:
                                alive = false;
                                break;
                            case SDL_SCANCODE_ESCAPE:
                            case SDL_SCANCODE_Z:
                            case SDL_SCANCODE_N:
                            case SDL_SCANCODE_P:
                                compute->pause();
                                break;
                            case SDL_SCANCODE_Q:
                                compute->limitFramerate(false);
                                break;
                            case SDL_SCANCODE_A:
                                player1.velX = -1;
                                break;
                            case SDL_SCANCODE_D:
                                player1.velX = 1;
                                break;
                            case SDL_SCANCODE_W:
                                player1.velY = -1;
                                break;
                            case SDL_SCANCODE_S:
                                player1.velY = 1;
                                break;
                            case SDL_SCANCODE_SPACE:
                                compute->unpause();
                                player1.shooting = true;
                                break;
                            case SDL_SCANCODE_TAB:
                                player1.useSpecial = true;
                                break;
                            case SDL_SCANCODE_KP_4:
                                player2.velX = -1;
                                break;
                            case SDL_SCANCODE_KP_6:
                                player2.velX = 1;
                                break;
                            case SDL_SCANCODE_KP_8:
                                player2.velY = -1;
                                break;
                            case SDL_SCANCODE_KP_5:
                                player2.velY = 1;
                                break;
                            case SDL_SCANCODE_KP_0:
                            case SDL_SCANCODE_RSHIFT:
                                player2.shooting = true;
                                break;
                            case SDL_SCANCODE_KP_PLUS:
                                player2.useSpecial = true;
                                break;
                            default:;
                        }
                        break;
                    case SDL_KEYUP:
                        switch (event.key.keysym.scancode) {
                            case SDL_SCANCODE_Q:
                                compute->limitFramerate(true);
                                break;
                            case SDL_SCANCODE_A:
                                if (player1.velX == -1)
                                    player1.velX = 0;
                                break;
                            case SDL_SCANCODE_D:
                                if (player1.velX == 1)
                                    player1.velX = 0;
                                break;
                            case SDL_SCANCODE_W:
                                if (player1.velY == -1)
                                    player1.velY = 0;
                                break;
                            case SDL_SCANCODE_S:
                                if (player1.velY == 1)
                                    player1.velY = 0;
                                break;
                            case SDL_SCANCODE_SPACE:
                                player1.shooting = false;
                                break;
                            case SDL_SCANCODE_KP_4:
                                if (player2.velX == -1)
                                    player2.velX = 0;
                                break;
                            case SDL_SCANCODE_KP_6:
                                if (player2.velX == 1)
                                    player2.velX = 0;
                                break;
                            case SDL_SCANCODE_KP_8:
                                if (player2.velY == -1)
                                    player2.velY = 0;
                                break;
                            case SDL_SCANCODE_KP_5:
                                if (player2.velY == 1)
                                    player2.velY = 0;
                                break;
                            case SDL_SCANCODE_RSHIFT:
                            case SDL_SCANCODE_KP_0:
                                player2.shooting = false;
                                break;
                            default:;
                        }
                        break;
                    default:;
                }
            }
            std::this_thread::sleep_until(clock);
        }
        gameEnd();
    }
}

void Game::gameStart()
{
    alive = true;
    someone = true;
    first = true;
    level = std::max(1, maxLevel - 7);
    candyTypeProbScale = (level < 4) ? 1 : level * 0.08 + 0.68;
    difficultyCoef = pow((float) level, 1.7f);
    tic = 0;

    initPlayer(player1, 0);
    if (nbPlayer == 2)
        initPlayer(player2, 1);
    display->section2 = (nbPlayer == 2);

    display->unpause();
    compute->start((void (*)(void *, GPUEntityMgr &)) &updateS, (void (*)(void *, GPUEntityMgr &)) &updatePlayerS, this);
}

void Game::gameEnd()
{
    if (level > maxLevel)
        maxLevel = level;
    compute->stop();
    display->pause();
    save();
}

void Game::update(GPUEntityMgr &engine)
{
    someone = false;
    if (first) {
        first = false;
        if (player1.alive) {
            auto &tmp = core->getFragment(player1.vesselAspect, player1.x, player1.y, 0, 0);
            tmp.health = player1.lastHealth;
            compute->pushPlayer(0) = tmp;
        }
        if (player2.alive) {
            auto &tmp = core->getFragment(player2.vesselAspect, player2.x, player2.y, 0, 0);
            tmp.health = player2.lastHealth;
            compute->pushPlayer(1) = tmp;
        }
    }
    if (tic == 1500) { // 30s
        tic = 0;
        level += 1;
        difficultyCoef = pow((float) level, 1.7f);
        candyTypeProbScale = (level < 4) ? 1 : level * 0.07 + 0.72;
    }
    if (!(tic & 15)) {
        for (int i = (level < 4 ? level : 2 + level / 2); i > 0; --i) {
            int rdm = percentDist(rdevice) / candyTypeProbScale;
            int j = 0;
            while (spawnability[j].second < rdm)
                rdm -= spawnability[j++].second;
            const float vel = 0.5 + normDist(rdevice)*(1.f + level / 6.f);
            const int localPos = candyPosDist(rdevice);
            switch (spawnability[j].first) {
                case LINE1:
                    engine.pushCandy(F_LINE) = core->getFragment(LINE1 + (localPos & 3), 1000, localPos, -vel);
                    break;
                case MULTICOLOR:
                    engine.pushCandy(F_MULTICOLOR) = core->getFragment(MULTICOLOR, 1000, localPos, -vel);
                    break;
                case BLOCK1:
                    engine.pushCandy(F_BLOCK) = core->getFragment(BLOCK1 + (localPos & 3), 1000, localPos, -vel);
                    break;
                case FISH1:
                    engine.pushCandy() = core->getFragment(FISH1 + (localPos & 3), 1000, localPos, -vel, 2048);
                    break;
                default:
                    engine.pushCandy() = core->getFragment(spawnability[i].first + (localPos & 3), 1000, localPos, -vel);
            }
        }
    }
    if (player1.alive) {
        updatePlayer(player1, 0);
        someone = true;
    }
    if (player2.alive) {
        updatePlayer(player2, 1);
        someone = true;
    }
    ++tic;
    display->score1Max = player1.saved.maxScore;
    display->score2Max = player2.saved.maxScore;
    display->score1 = player1.score;
    display->score2 = player2.score;
    display->level1 = level;
    display->level1Max = maxLevel;
}

void Game::updatePlayer(Player &p, int idx)
{
    if (p.energy < p.energyMax && p.coolant >= p.energyHeatCost) {
        p.energy += p.energyRate;
        p.coolant -= p.energyRate;
    }
    if ((p.velX | p.velY) && p.energy >= p.moveEnergyCost) {
        p.energy -= p.moveEnergyCost;
        p.coolant -= p.moveHeatCost;
        p.x += p.velX * p.moveSpeed;
        p.y += p.velY * p.moveSpeed;
    }
    if (p.coolant < p.coolantMax)
        p.coolant += p.coolantRate;
    if (p.shield < p.shieldMax && p.energy > (p.energyMax * 4) && p.coolant > (p.coolantMax * 8)) {
        p.shield += p.shieldRate;
        p.energy -= p.shieldEnergyCost;
        p.coolant -= p.shieldHeatCost;
    }
    if (p.boost) {
        if (p.special >= 1) {
            p.special -= 1;
            p.shield += 1;
            if (p.shield > p.shieldMax)
                p.shield = p.shieldMax;
        } else
            p.boost = false;
    }
    // update position
    p.posX = p.x * core->worldScaleX * 2 - 1;
    p.posY = p.y * core->worldScaleX * 2 - 1;

    if (!(tic % ((p.saved.weapon == 4) ? 2 : 10)) && p.shooting)
        shoot(p);
    if (p.useSpecial) {
        useSpecial(p);
        p.useSpecial = false;
    }
    // x -0.025 y -0.0333333 + .00416666666666 * [0:3]
    auto &tmp = compute->pushPlayer(idx);
    tmp.posX = p.posX;
    float y = tmp.posY = p.posY;
    tmp.health = p.lastHealth;
    y -= 0.0333333333333333;
    p.ptr[0].x = p.ptr[1].x = p.ptr[2].x = p.ptr[3].x = p.ptr[4].x = p.posX - 0.025;
    p.ptr[0].y = p.ptr[1].y = y;
    y += 0.0041666666666666;
    p.ptr[2].y = y;
    y += 0.0041666666666666;
    p.ptr[3].y = y;
    y += 0.0041666666666666;
    p.ptr[4].y = y;
    p.ptr[5].y = (p.energy > p.energyMax) ? p.energy / p.energyMax * 0.05 : 0.05;
    p.ptr[6].y = (p.shield > 0) ? p.shield / p.shieldMax * 0.05 : 0;
    p.ptr[7].y = (p.coolant < p.coolantMax) ? ((p.coolant > 0) ? p.coolant / p.coolantMax * 0.05 : 0) : 0.05;
    p.ptr[8].y = (p.special < p.specialMax) ? p.special / p.specialMax * 0.05 : 0.05;
}

void Game::shoot(Player &p)
{
    auto &w = weaponList[p.saved.weapon];

    const int cons = w.baseEnergyConsumption << p.saved.weaponLevel;
    if (p.energy < cons)
        return;
    p.energy -= cons;
    // play shoot sound
    switch (p.saved.weapon) {
        case 0: // laser
        {
            auto &e = core->getFragment(LASER);
            const float dmg = e.damage;
            switch (p.saved.weaponLevel) { // level go from 0 to 8 both included
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                    e.damage = -1 - p.saved.weaponLevel;
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 24, p.y, 7);
                    break;
                case 5:
                case 6:
                case 7:
                    e.damage = -p.saved.weaponLevel;
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 24, p.y - 16, 7);
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 24, p.y + 16, 7);
                    --e.damage;
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 28, p.y, 7);
                    break;
                case 8:
                    e.damage = -6;
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 24, p.y - 32, 7);
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 24, p.y + 32, 7);
                    e.damage = -7;
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 28, p.y - 16, 7);
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 28, p.y + 16, 7);
                    e.damage = -8;
                    compute->pushPlayerShoot() = core->getFragment(LASER, p.x + 32, p.y, 7);
                    break;
            }
            e.damage = dmg;
        }
            break;
        case 1: // blaster
        {
            const int x = p.x + 24;
            int y = p.y - 6 * p.saved.weaponLevel;
            for (int i = p.saved.weaponLevel; i > 0; --i) {
                compute->pushPlayerShoot() = core->getFragment(BLASTER, x, y, 10);
                y += 12;
            }
        }
            break;
        case 2: // wave
            switch (p.saved.weaponLevel) {
                default:;
            }
            break;
        case 3: // eclatant
            switch (p.saved.weaponLevel) {
                default:;
            }
            break;
        case 4: // laserifier
            switch (p.saved.weaponLevel) {
                default:;
            }
            break;
    }
}

void Game::useSpecial(Player &p)
{
    auto &w = specialList[p.saved.special];

    if (p.special < w.specialCost)
        return;
    p.special -= w.specialCost;
    switch (p.saved.special) {
        case 1:
            compute->pushPlayerShoot() = core->dropFragment(BIG_LASER, p.posX, p.posY, 15);
            break;
        case 2:
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, -1, -1);
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, 0, -1);
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, 1, -1);
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, 1, 0);
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, 1, 1);
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, 0, 1);
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, -1, 1);
            compute->pushPlayerShoot() = core->dropFragment(PROTECTO, p.posX, p.posY, -1, 0);
            break;
        case 3: // trans
            compute->pushPlayerShoot() = core->dropFragment(MINI_TRANS, p.posX, p.posY, 15);
            break;
        case 4:
            compute->pushPlayerShoot() = core->dropFragment(TRANS, p.posX, p.posY, 15);
            break;
        case 5:
            compute->pushPlayerShoot() = core->dropFragment(SUPER_TRANS, p.posX, p.posY, 15);
            break;
        case 6:
        case 7:
            p.boost = true;
            // play shield boost sound
            break;
        default:
            break;
    }
}

void Game::updatePlayer(GPUEntityMgr &engine)
{
    updatePlayerState(player1, 0);
    updatePlayerState(player2, 1);
    for (int i = engine.nbDead; i-- > 0;) {
        switch (engine.deadFlags[i].second) {
            case F_OTHER:
                continue;
            case F_PIECE_1:
            {
                auto &p = getClosest(engine.deadFlags[i].first);
                p.score += 1 * difficultyCoef;
                BONUS_EFFECT;
            }
            case F_PIECE_5:
            {
                auto &p = getClosest(engine.deadFlags[i].first);
                p.score += 5 * difficultyCoef;
                BONUS_EFFECT;
            }
            case F_PIECE_10:
            {
                auto &p = getClosest(engine.deadFlags[i].first);
                p.score += 10 * difficultyCoef;
                BONUS_EFFECT;
            }
            case F_PIECE_25:
            {
                auto &p = getClosest(engine.deadFlags[i].first);
                p.score += 25 * difficultyCoef;
                BONUS_EFFECT;
            }
            case F_SHIELD_BOOST:
            {
                auto &p = getClosest(engine.deadFlags[i].first);
                if (p.shield >= p.shieldMax) {
                    p.score += 100 * difficultyCoef;
                } else {
                    p.shield = 2 + p.shieldMax / 2;
                    p.energy = p.energyMax;
                }
                BONUS_EFFECT;
            }
            case F_SPECIAL_BOOST:
            {
                auto &p = getClosest(engine.deadFlags[i].first);
                p.shield += p.shieldMax / 6;
                p.energy = p.energyMax;
                p.special = p.specialMax;
                BONUS_EFFECT;
            }
            case F_COOLANT_BOOST:
            {
                auto &p = getClosest(engine.deadFlags[i].first);
                p.shield += p.shieldMax / 6;
                p.energy = p.energyMax;
                p.coolant = p.coolantRate * 500;
                BONUS_EFFECT;
            }
            case F_PLAYER:
                continue; // Why this flag ?
            case F_ECLATANT:
            {
                EntityState e = engine.readEntity(engine.deadFlags[i].first);
                engine.pushPlayerShoot() = core->dropFragment(ECLAT0, e.deadX, e.deadY, 1.5, 1.5);
                engine.pushPlayerShoot() = core->dropFragment(ECLAT1, e.deadX, e.deadY, -1.5, 1.5);
                engine.pushPlayerShoot() = core->dropFragment(ECLAT2, e.deadX, e.deadY, -1.5, -1.5);
                engine.pushPlayerShoot() = core->dropFragment(ECLAT3, e.deadX, e.deadY, 1.5, -1.5);
                continue;
            }
            case F_MULTICOLOR:
            {
                EntityState e = engine.readEntity(engine.deadFlags[i].first);
                for (int i = 0; i < 16; ++i) {
                    engine.pushCandyShoot() = core->dropFragment(LASER, e.deadX, e.deadY, circle[i], circle[(i - 4) & 15]);
                }
                continue;
            }
            case F_LINE:
            {
                EntityState e = engine.readEntity(engine.deadFlags[i].first);
                engine.pushCandyShoot() = core->dropFragment(BIG_LASER, e.deadX, e.deadY, -15);
                engine.pushCandyShoot() = core->dropFragment(BIG_LASER, e.deadX, e.deadY, 15);
                continue;
            }
            case F_BLOCK:
            {
                EntityState e = engine.readEntity(engine.deadFlags[i].first);
                for (int i = 0; i < 16; ++i) {
                    if (!(i & 1)) {
                        engine.pushBonus() = core->dropFragment(BONUS3, e.deadX, e.deadY, circle[i] / 28 - 2, circle[(i - 4) & 15] / 28);
                    }
                    engine.pushBonus() = core->dropFragment(LASER2, e.deadX, e.deadY, circle[i], circle[(i - 4) & 15]);
                }
                engine.pushBonus() = core->dropFragment(BONUS2, e.deadX, e.deadY, -2);
                engine.pushBonus() = core->dropFragment(BIG_LASER2, e.deadX, e.deadY, -18);
                engine.pushBonus() = core->dropFragment(BIG_LASER2, e.deadX, e.deadY, 18);
                continue;
            }
            case F_CANDY:
            {
                int r = bonusDist(rdevice);
                if (r < 650)
                    continue;
                EntityState e = engine.readEntity(engine.deadFlags[i].first);
                if (r < 675)
                    engine.pushBonus(F_PIECE_1) = core->dropFragment(BONUS0, e.deadX, e.deadY, -2);
                else if (r < 690)
                    engine.pushBonus(F_PIECE_5) = core->dropFragment(BONUS1, e.deadX, e.deadY, -2);
                else if (r < 695)
                    engine.pushBonus(F_PIECE_10) = core->dropFragment(BONUS2, e.deadX, e.deadY, -2);
                else if (r < 698)
                    engine.pushBonus(F_PIECE_25) = core->dropFragment(BONUS3, e.deadX, e.deadY, -2);
                else if (r < 699)
                    engine.pushBonus(F_SHIELD_BOOST) = core->dropFragment(BONUS4, e.deadX, e.deadY, -2);
                else if (r < 700)
                    engine.pushBonus(F_SPECIAL_BOOST) = core->dropFragment(BONUS5, e.deadX, e.deadY, -2);
                else
                    engine.pushBonus(F_COOLANT_BOOST) = core->dropFragment(BONUS6, e.deadX, e.deadY, -2);
                continue;
            }
            default: // MIEL1-5 useless while there is no level loaded
                continue;
        }
    }
}

void Game::updatePlayerState(Player &p, int i)
{
    if (!p.alive)
        return;
    const int newShield = compute->readEntity(i).health;
    if (newShield != p.lastHealth) {
        if (newShield < 0) {
            p.alive = false;
            p.score -= p.score / generatorList[p.saved.generator].deathLossRatio;
            if (p.score < p.saved.maxScore) {
                p.score = p.saved.maxScore;
            } else {
                p.saved.maxScore = p.score;
            }
            // play death sound
            return;
        }
        // play shield sound
        p.shield -= newShield - p.lastHealth;
    }
    if (p.shield < p.shieldMax / 5 && p.highPOverclocker) {
        float loc_recover = std::min({p.shieldMax/5.f - p.shield, p.special/1.5f, p.coolant/5000.f});
        p.coolant -= 5000*loc_recover;
        p.special -= 1.5*loc_recover;
        p.shield += loc_recover;
    }
    p.lastHealth = (int) p.shield + 1;
}

Player &Game::getClosest(short idx)
{
    if (player1.alive && player2.alive) {
        auto &readback = compute->readEntity(idx);
        float x1 = readback.deadX;
        float y1 = readback.deadY;
        float x2 = x1;
        float y2 = y1;
        x1 -= player1.posX;
        y1 -= player1.posY;
        x2 -= player2.posX;
        y2 -= player2.posY;
        x1 *= x1;
        y1 *= y1;
        x2 *= x2;
        y2 *= y2;
        return (x1 + y1 <= x2 + y2) ? player1 : player2;
    }
    return (player1.alive) ? player1 : player2;
}

void Game::load(int slot, int playerCount)
{
    usedSlot = slot;

    std::ifstream file(std::string("slot") + std::to_string(slot) + ".dat", std::ifstream::binary);
    if (file) {
        file.read((char *) &maxLevel, 9);
        file.read((char *) &player1, 17);
        player1.score = player1.saved.maxScore;
        if (nbPlayer == 2) {
            file.read((char *) &player2, 17);
            player2.score = player2.saved.maxScore;
        }
    } else {
        std::cout << "Slot not found, creating one\n";
        nbPlayer = playerCount;
        for (int i = 0; i < nbPlayer; ++i) {
            SavedDatas &s = (i == 0) ? player1.saved : player2.saved;
            s = SavedDatas({0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0});
        }
    }
    recursionGainFactor = (nbPlayer == 1) ? 1 : 0.75;
}

void Game::save()
{
    std::ofstream file(std::string("slot") + std::to_string(usedSlot) + ".dat", std::ofstream::binary);

    if (file) {
        file.write((char *) &maxLevel, 9);
        file.write((char *) &player1, 17);
        if (nbPlayer == 2)
            file.write((char *) &player2, 17);
    } else {
        std::cout << "Failed to create save\n";
    }
}

void Game::initPlayer(Player &p, int idx)
{
    p.x = 540 * (idx + 1) / (nbPlayer + 1);
    p.y = 48;
    auto &v = vesselList[p.saved.vessel];
    auto &g = generatorList[p.saved.generator];
    auto &r = recoolerList[p.saved.recooler];
    auto &s = shieldList[p.saved.shield];
    p.moveSpeed = v.speed;
    p.moveEnergyCost = v.energyConsumption;
    p.moveHeatCost = v.energyConsumption * 2;
    p.energyMax = g.energyCapacity;
    p.energy = p.energyMax / 4;
    p.energyRate = g.energyRate;
    p.energyHeatCost = g.energyRate;
    p.shield = level * 2.4;
    p.shieldRate = s.shieldRate;
    p.shieldEnergyCost = s.energyConsumption;
    p.shieldHeatCost = s.heat;
    p.shieldMax = s.shieldCapacity;
    if (p.shield > p.shieldMax)
        p.shield = p.shieldMax;
    p.coolantMax = r.recoolingCapacity;
    p.coolant = p.coolantMax / 8;
    p.coolantRate = r.recoolingRate;
    p.uniconvert = (p.saved.special == 9);
    p.special = 50;
    p.specialMax = (p.uniconvert) ? 160 : 100;
    p.velX = 0;
    p.velY = 0;
    p.alive = true;
    p.lastHealth = p.shield + 1;
    p.highPOverclocker = p.uniconvert | (p.saved.special == 8);
    p.shooting = false;
    p.useSpecial = false;
    p.boost = false;
    p.vesselAspect = vesselList[p.saved.vessel].aspect;
}
