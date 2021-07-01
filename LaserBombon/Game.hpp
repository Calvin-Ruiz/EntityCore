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

#define RESPAWN_TIME 600

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
    bool openMenu(int type);
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
    void revive(Player &target, Player &saver, int idx);
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
    std::uniform_int_distribution<int> candyPosDist;
    std::uniform_real_distribution<float> normDist {0.f, 1.f};
    std::uniform_real_distribution<float> percentDist {0.f, 100.f};
    int level = 1;
    int maxLevel = 1;
    unsigned int recursion = 0;
    unsigned char nbPlayer = 0;
    bool alive = false;
    bool first = false;
    bool notQuitting = true;
    bool someone = false;
    Player player1 {};
    Player player2 {};
    float difficultyCoef;
    float candyTypeProbScale;
    float circle[16];
    int tic = 0;
    int usedSlot;
    const int width;
    const int height;
public:
    // Configurations
    const int startScore = 8500; // Start equipment value
    const float recursionBaseScoreRatio = 0.0625;
    float recursionGainFactor = 1; // 0.75 for 2-players
    // pair of (entity/spawn chance)
    static const std::pair<unsigned char, unsigned char> spawnability[6];
    static const WeaponAttributes weaponList[5];
    static const VesselAttributes vesselList[6];
    static const GeneratorAttributes generatorList[6];
    static const RecoolerAttributes recoolerList[10];
    static const ShieldAttributes shieldList[11];
    static const SpecialWeaponAttributes specialList[10];
};

#endif /* GAME_HPP_ */
