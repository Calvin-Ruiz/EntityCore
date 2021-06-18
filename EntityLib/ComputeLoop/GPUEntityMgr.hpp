/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** GPUEntityMgr.hpp
*/

#ifndef GPUENTITYMGR_HPP_
#define GPUENTITYMGR_HPP_

// Transfer 1 (single-use) :
// Wait event 2
// Push new entities
// Signal event 3 after writing updates

// Compute 1 (reused) :
// Wait event 3 before reading entries or transfering data
// Reset event 3
// Querry player datas
// Dispatch collision (bullet/player look at block to damage)
// Barrier : wait health update before updating state
// Dispatch update (except collision test)
// Reset event 2
// Signal event 4 after writing player and output

// Transfer 2 (single-use) :
// Wait event 5
// Querry player datas
// Push new entities
// Signal event 1 after writing updates

// Compute 2 (reused) :
// Wait event 1 before reading entries or transfering data and event 6 before reading entries
// Reset event 1
// Dispatch update (except collision test)
// Reset event 5 on compute stage
// Reset event 6 on compute stage
// Signal event 2 after position update
// Barrier : wait position update completion
// Dispatch collision (bullet/player look at block to damage)
// Signal event 3 after health update

// Local :
// Update game
// Record transfer due to tic update
// Wait event 5/2
// Read back changes
// Update player shield
// Write player changes
// Record transfer due to GPU event
// Submit transfer 1/2 and compute 1/2

// Player : Candy (4 max)
// Player shoot : Candy (280 max)
// Candy shoot : Player, Candy (100 max)
// Special candy shoot (or bonus) : Player (128 max)
// Candy : Player, Candy shoot, Player shoot (512 max)

// Player -> check collision candy shoot and special candy shoot
// Candy -> check if alive, then check collision with player, candy shoot and player shoot
// Player : 4x228 dispatch 1x1 to 4x1 (shifted by 284)
// Candy : 4x384 dispatch 1x1 to 128x1
// Update : 1024 dispatch 1
// Double shoot power if single player

#define BEG_PLAYER 0
#define BEG_PLAYER_SHOOT 4
#define BEG_CANDY_SHOOT 284
#define BEG_BONUS 384
#define BEG_CANDY 512
#define END_ALL 1024

#include <string>
#include <memory>
#include <thread>
#include "EntityCore/SubBuffer.hpp"

class EntityLib;
class VulkanMgr;
class BufferMgr;
class SetMgr;
class Set;
class PipelineLayout;
class ComputePipeline;
class SyncEvent;

// Note : the player is an entity like any other
struct EntityData {
    float posX;
    float posY;
    float velX;
    float velY;
    int health; // Only those datas are querried for player
    int damage;
    float sizeX;
    float sizeY;
    float texX1;
    float texY1;
    float texX2;
    float texY2;
    VkBool32 aliveNow; // MUST BE VK_FALSE ON INSERTION
    VkBool32 newlyInserted; // MUST BE VK_TRUE ON INSERTION
};

struct EntityVertexGroup {
    float X1;
    float Y1;
    float zero1;
    float one1;
    float texX1;
    float texY1;

    float relX2;
    float relY2;
    float zero2;
    float zero3;
    float texX2;
    float texY2;

    float zero4;
    float negRelY2;
    float zero5;
    float zero6;
    float _texX2;
    float _texY1;

    float negRelX2;
    float zero7;
    float zero8;
    float zero9;
    float __texX1;
    float __texY2;
};

struct EntityState {
    float deadX;
    float deadY;
    int health; // negative mean dead
};

struct EntityAttachment {
    unsigned char flag;
    bool alive;
};

class GPUEntityMgr {
public:
    GPUEntityMgr(std::shared_ptr<EntityLib> master);
    virtual ~GPUEntityMgr();
    GPUEntityMgr(const GPUEntityMgr &cpy) = delete;
    GPUEntityMgr &operator=(const GPUEntityMgr &src) = delete;

    void start(void (*update)(void *, GPUEntityMgr &), void (*updatePlayer)(void *, GPUEntityMgr &), void *data);
    void limitFramerate(bool _limit) {limit = _limit;}
    void pause() {active = false;}
    void unpause() {active = true;}
    void stop() {alive = false;}

    // Require index buffer pattern 0 1 2 0 1 3
    SubBuffer &getVertexBuffer() {return gpuVertices;}

    //! Those functions are only valid for write operations, do NEVER read anything from push*()
    //! push*() = EntityData {posX, posY, velX, velY, health, -damage, sizeX, sizeY, texX1, texY1, texX2, texY2, VK_FALSE, VK_TRUE};
    EntityData &pushPlayerShoot(unsigned char flag = 0);
    EntityData &pushCandyShoot(unsigned char flag = 0);
    EntityData &pushBonus(unsigned char flag = 0);
    EntityData &pushCandy(unsigned char flag = 0);
    EntityData &pushPlayer(short idx);

    //! Those methods and variables are only valid for read operations from updatePlayer
    EntityState &readEntity(short idx); // Read entity data at index, index 0-3 are player index
    short nbDead = 0; // number of death this frame
    std::pair<short, unsigned char> deadFlags[1024]; // Index flag
private:
    void mainloop(void (*update)(void *, GPUEntityMgr &), void (*updatePlayer)(void *, GPUEntityMgr &), void *data);
    void updateChanges();
    void buildCompute();
    void resetAll();
    void pushRegion(short idx);
    VulkanMgr &vkmgr;
    BufferMgr &localBuffer;
    std::unique_ptr<std::thread> updater;
    bool limit = true;
    bool alive = false;
    bool active = true;
    bool initDep = true;
    std::unique_ptr<BufferMgr> entityMgr;
    std::unique_ptr<BufferMgr> vertexMgr;
    std::unique_ptr<BufferMgr> readbackMgr;
    SubBuffer readbackBuffer;
    SubBuffer entityPushBuffer;
    SubBuffer gpuEntities;
    SubBuffer gpuVertices;
    EntityState *readback;
    EntityData *entityPush;
    EntityData deft; // Used if there is no available slot
    VkCommandPool computePool;
    VkCommandPool transferPool;
    VkQueue computeQueue;
    VkCommandBuffer cmds[4]; // Transfer 1, Compute 1, Transfer 2, Compute 2
    VkCommandBuffer cmd;
    VkSubmitInfo sinfo[2] {{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 2, cmds, 0, nullptr},
                           {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 2, cmds + 2, 0, nullptr}};
    VkCommandBufferBeginInfo begInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
    SyncEvent *syncExt; // [2]
    SyncEvent *syncInt; // [4] with 1 merged with 3 and 2 merged with 4
    unsigned char frameparity = 0;
    EntityAttachment attachment[1024];
    // 0-3 PLAYER | 4-283 PLAYER SHOOT | 284-383 CANDY SHOOT | 384-511 BONUS/SPECIAL_CANDY_SHOOT | 512-1023 CANDY
    int pidx = BEG_PLAYER;
    int psidx = BEG_PLAYER_SHOOT;
    int csidx = BEG_CANDY_SHOOT;
    int bidx = BEG_BONUS;
    int cidx = BEG_CANDY;
    int lastPush = 0;
    std::unique_ptr<SetMgr> setMgr;
    std::unique_ptr<Set> globalSet;
    std::unique_ptr<Set> updateSet;
    std::unique_ptr<PipelineLayout> collidePLayout;
    std::unique_ptr<PipelineLayout> updatePLayout;
    std::unique_ptr<ComputePipeline> collidePipeline;
    std::unique_ptr<ComputePipeline> pcollidePipeline;
    std::unique_ptr<ComputePipeline> updatePipeline;
    std::vector<VkBufferCopy> regionsBase; // Player copy
    std::vector<VkBufferCopy> regions;
    std::shared_ptr<EntityLib> master;
};

#endif /* GPUENTITYMGR_HPP_ */
