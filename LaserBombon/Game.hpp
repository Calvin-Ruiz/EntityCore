/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** Game.hpp
*/

#ifndef GAME_HPP_
#define GAME_HPP_

#include <string>
#include <memory>
#include <random>

class EntityLib;
class GPUDisplay;
class GPUEntityMgr;
class Tracer;

struct EntityData;
struct EntityState;
struct JaugeVertex;

#define BONUS_EFFECT \
p->special += 0.2; \
p->coolant += 200; \
p->shield += 0.0625; \
if (p->shield > p->shieldMax) { \
    p->special += p->shield - p->shieldMax; \
    p->shield = p->shieldMax; \
} \
continue

enum EntityTypes : unsigned char {
    MIEL0,
    MIEL1,
    MIEL2,
    MIEL3,
    MIEL4,
    MIEL5,
    LINE1,
    LINE2,
    LINE3,
    LINE4,
    MULTICOLOR,
    CHOCOLAT,
    CANDY1,
    CANDY2,
    CANDY3,
    CANDY4,
    BOMB1,
    BOMB2,
    BOMB3,
    BOMB4,
    BLOCK1,
    BLOCK2,
    BLOCK3,
    BLOCK4,
    FISH1,
    FISH2,
    FISH3,
    FISH4,
    BIG_LASER,
    BIG_LASER2,
    CLASSIC,
    BETTER,
    AERODYNAMIC,
    OPTIMAL,
    ENERGISED,
    BOOSTED,
    LASER,
    LASER2,
    ECLAT0,
    ECLAT1,
    ECLAT2,
    ECLAT3,
    BONUS0,
    BONUS1,
    BONUS2,
    BONUS3,
    BONUS4,
    BONUS5,
    BONUS6,
    BLASTER,
    ECLATANT,
    PROTECTO,
    WAVE,
    STRONG_WAVE,
    BIG_WAVE,
    MINI_TRANS,
    TRANS,
    SUPER_TRANS,
    LASERIFIER
};

enum EntityFlags : unsigned char {
    F_OTHER,
    F_PIECE_1,
    F_PIECE_5,
    F_PIECE_10,
    F_PIECE_25,
    F_SHIELD_BOOST,
    F_SPECIAL_BOOST,
    F_COOLANT_BOOST,
    F_PLAYER,
    F_ECLATANT,
    F_MULTICOLOR,
    F_LINE,
    F_BLOCK,
    F_CANDY,
    F_MIEL1,
    F_MIEL2,
    F_MIEL3,
    F_MIEL4,
    F_MIEL5,
};

enum SpecialWeapon : unsigned char {
    SW_NONE,
    SW_BIG_LASER,
    SW_PROTECTO,
    SW_MINI_TRANS,
    SW_TRANS,
    SW_SUPER_TRANS,
    SW_SHIELD_BOOSTER,
    SW_ENHANCED_SHIELD_BOOSTER,
    SW_HIGHP_SHIELD_OVERCLOCKER,
    SW_UNIVERSAL_CONVERTOR
};

struct VesselAttributes {
    std::string name;
    int cost;
    int speed;
    int energyConsumption;
    unsigned char aspect;
};

struct SpecialWeaponAttributes {
    std::string name;
    std::string desc;
    int cost;
    float specialCost;
};

struct ShieldAttributes {
    std::string name;
    int cost;
    float shieldRate;
    float shieldCapacity;
    float energyConsumption;
    float heat;
};

struct WeaponAttributes {
    std::string name;
    int cost;
    int baseEnergyConsumption;
};

struct GeneratorAttributes {
    std::string name;
    int cost;
    float energyCapacity;
    float energyRate;
    int deathLossRatio;
};

struct RecoolerAttributes {
    std::string name;
    int cost;
    float recoolingRate;
    float recoolingCapacity;
};

struct SavedDatas { // 17
    unsigned int maxScore;
    unsigned char vessel;
    unsigned char weapon;
    unsigned char weaponLevel;
    unsigned char special;
    unsigned char generator;
    unsigned char shield;
    unsigned char recooler;
    unsigned char vesselUnlock;
    unsigned char weaponUnlock;
    unsigned char specialUnlock;
    unsigned char generatorUnlock;
    unsigned char shieldUnlock;
    unsigned char recoolerUnlock;
};

struct Player {
    SavedDatas saved;
    JaugeVertex *ptr;
    int x;
    int y;
    int velX; // 0
    int velY; // 0
    int score;
    int lastHealth;
    float moveSpeed;
    float moveEnergyCost;
    float moveHeatCost;
    float energy;
    float energyRate;
    float energyHeatCost;
    float energyMax;
    float shield;
    float shieldRate;
    float shieldEnergyCost;
    float shieldHeatCost;
    float shieldMax;
    float coolant;
    float coolantRate;
    float coolantMax;
    float special;
    float specialMax;
    float posX;
    float posY;
    bool alive;
    bool uniconvert;
    bool highPOverclocker;
    bool shooting;
    bool useSpecial;
    bool boost;
    unsigned char vesselAspect;
};

class Game {
public:
    Game(const std::string &name, uint32_t version, int width, int height);
    virtual ~Game();
    Game(const Game &cpy) = delete;
    Game &operator=(const Game &src) = delete;

    void init();
    void mainloop();
    void load(int slot, int playerCount = 2);
private:
    void save();
    void gameStart();
    void gameEnd();
    void initPlayer(Player &p, int idx);
    void update(GPUEntityMgr &engine);
    void updatePlayer(GPUEntityMgr &engine);
    void updatePlayer(Player &p, int idx);
    void updatePlayerState(Player &p, int idx);
    void shoot(Player &p);
    void useSpecial(Player &p);
    void spawn(GPUEntityMgr &engine);
    static std::string toText(long nbr);
    static void updateS(Game *self, GPUEntityMgr &engine) {
        self->update(engine);
    }
    static void updatePlayerS(Game *self, GPUEntityMgr &engine) {
        self->updatePlayer(engine);
    }
    Player *getClosest(short idx);
    std::unique_ptr<GPUDisplay> display;
    std::unique_ptr<GPUEntityMgr> compute;
    std::unique_ptr<Tracer> tracer;
    std::shared_ptr<EntityLib> core;
    std::random_device rdevice;
    std::uniform_int_distribution<int> bonusDist {0, 700};
    std::uniform_int_distribution<int> candyPosDist {20, 520};
    std::uniform_real_distribution<float> normDist {0.f, 1.f};
    std::uniform_real_distribution<float> percentDist {0.f, 100.f};
    int level = 1;
    int maxLevel = 1;
    unsigned int recursion = 0;
    unsigned char nbPlayer = 2;
    bool alive = false;
    bool first = false;
    bool notQuitting = true;
    bool someone = false;
    Player player1 {};
    Player player2 {};
    float difficultyCoef;
    float candyTypeProbScale;
    float circle[16];
    const int startScore = 8500; // Start equipment value
    const float recursionBaseScoreRatio = 0.0625;
    float recursionGainFactor = 1; // 0.75 for 2-players
    int tic = 0;
    int usedSlot;

    // Configurations
    // pair of (entity/spawn chance)
    std::pair<unsigned char, unsigned char> spawnability[6] {{BLOCK1, 1}, {LINE1, 7}, {MULTICOLOR, 3}, {BOMB1, 10}, {FISH1, 4}, {CANDY1, 75}};
    WeaponAttributes weaponList[5] {{"laser", 500, 23}, {"blaster", 800, 16}, {"wave", 1200, 18}, {"eclatant", 3000, 25}, {"laserifier", 20000, 60}};
    VesselAttributes vesselList[6] {{"classic", 1000, 2, 2, CLASSIC}, {"better", 4000, 3, 3, BETTER}, {"aerodynamic", 6000, 3, 1, AERODYNAMIC}, {"energised", 10000, 5, 12, ENERGISED}, {"optimal", 32000, 5, 2, OPTIMAL}, {"boosted", 56000, 10, 30, BOOSTED}};
    GeneratorAttributes generatorList[6] {{"minimal", 500, 1200, 3, 50}, {"basic", 3000, 200, 5, 30}, {"good", 10000, 600, 20, 20}, {"highly_performant", 34000, 3000, 100, 10}, {"excellent", 96000, 10000, 500, 5}, {"perfect", 600000, 30000, 3000, 4}};
    RecoolerAttributes recoolerList[10] {{"vent", 2000, 4, 100}, {"advanced vent", 4000, 8, 120}, {"simple water-cooling", 10000, 18, 1000}, {"water-based Closed-Loop Recooling System", 20000, 35, 2000}, {"coolant-based CLRS", 50000, 140, 10000}, {"heat exchanger", 120000, 270, 12000}, {"uncredible heat exchanger", 250000, 450, 18000}, {"heat-to-ray", 800000, 600, 300000}, {"hellium fission", 2400000, 1200, 1800000}, {"Xtreme Hellium Fission", 30000000, 2000, 3000000}};
    ShieldAttributes shieldList[11] {{"better than nothing", 2000, 0.00025, 1, 0.625, 2.5}, {"maybe good", 4000, 0.0005, 2, 1.25, 5}, {"the stainless steel shield !", 8000, 0.001, 4, 2.5, 10}, {"auto-cleanable upgraded shield", 16000, 0.002, 8, 5, 20}, {"micro-technology shield", 32000, 0.004, 16, 10, 40}, {"electro-pulsed forcefield", 64000, 0.008, 32, 20, 80}, {"microfusion shield system", 128000, 0.016, 64, 40, 160}, {"quantum-based shield system", 256000, 0.04, 24, 100, 400}, {"multi-dimensionnal quantum-shield system", 512000, 0.08, 32, 200, 800}, {"useless shield", 1024000, 0.1, 40, 250, 1000}, {"The UseLess Shield !", 2048000, 0.12, 48, 300, 1200}};
    SpecialWeaponAttributes specialList[10] {{"None", "Nothing", 0, 0}, {"big_laser", "powerfull", 3000, 20},
    {"protecto", "destroy candies at proximity of you in all directions\nIt's my favorite special weapon in OpenTyrian", 6000, 10},
    {"mini_transperseur", "unstoppable", 10000, 12}, {"transperseur", "unstoppable", 15000, 20}, {"super_transperseur", "unstoppable", 100000, 30},
    {"Shield Booster", "Convert special point into shield energy at 50 units/s\nThis effect end when out of special point", 3000000, 0},
    {"Enhanced Shield Booster", "Convert special point into shield energy at 50 units/s\nThis effect end when out of special point\nEnabled when shield is getting low", 10000000, 0},
    {"HighP Shield Overclocker", "Convert special point into shield energy to maintain 20% shield energy\nProduce 5k heat and consume 1.5 special_point for each shield point generated", 25000000, 0},
    {"Universal converter", "HighP Shield Overclocker functionnality\nconvert special point into coolant to maintain 25% coolant with 1:25k ratio\nconvert energy over 100% into special point at 20k:1 ratio\nIncrease special point limit from 100 to 160\nSpecial points over 150 are immediately consumed\n", 50000000, 0}};
};

#endif /* GAME_HPP_ */
