// FRACTURE — arcade-but-physical drivable vehicles with voxel interaction.
#pragma once
#include "core/engine.h"
#include "world/physics.h"
#include <vector>
#include <string>

enum VehicleKind {
    VK_SEDAN = 0, VK_PICKUP, VK_TRUCK, VK_BULLDOZER, VK_FORKLIFT, VK_ARMORED, VK_BOAT, VK_EXCAVATOR, VK_COUNT
};

struct VehicleDef {
    const char* name;
    float length, width, height;
    float mass;
    float topSpeed;       // m/s
    float accel;
    float turnRate;
    float grip;
    float wheelRadius;
    int wheelCount;
    float carveStrength;  // how much it destroys on impact
    bool boat;
};

const VehicleDef& vehicleDef(int kind);

struct Vehicle {
    VehicleKind kind = VK_SEDAN;
    vec3 pos{0, 1, 0};
    float yaw = 0, yawRate = 0;
    vec3 vel{0, 0, 0};
    float pitch = 0, roll = 0;
    float speed = 0;
    float engineRPM = 0;
    float steer = 0;
    float health = 100;
    bool occupied = false;
    bool destroyed = false;
    float wheelSpin = 0;
    float wheelDrop[4] = {0, 0, 0, 0};
    float wheelGround[4] = {0, 0, 0, 0};
    float bob = 0;
    float forkLift = 0;
    float armAngle = 0.35f;
    float armExtend = 0;
    float idleTimer = 0;
    int engineLoop = -1;
    float headlights = 1.0f;
    bool lightsOn = true;
    float hornTimer = 0;
    float crashCooldown = 0;
    float dustTimer = 0;
    float carveCooldown = 0;
    float openDoor = 0;
    vec3 lastPos;
    // stats
    float odo = 0;
};

class VehicleManager {
public:
    std::vector<Vehicle> vehicles;
    int maxVehicles = 24;
    World* world = nullptr;
    Physics* physics = nullptr;

    void init(World* w, Physics* p);
    void clear();
    int spawn(VehicleKind kind, const vec3& pos, float yaw);
    void update(float dt, int drivenIndex);
    void drive(Vehicle& v, float dt, InputState& in);
    bool tryEnter(const vec3& playerPos, int& outIndex);
    void exitVehicle(Vehicle& v);
    Vehicle* nearest(const vec3& p, float maxDist, int* indexOut = nullptr);
    void draw(IRenderer& r, const vec3& camPos);
    void addLocalBox(std::vector<BoxInstance>& out, const mat4& base, vec3 pos, vec3 size, vec3 color,
                     float emis = 0, float metal = 0.3f, float rough = 0.5f);
    void buildModel(Vehicle& v, std::vector<BoxInstance>& out);
    int count() const { return (int)vehicles.size(); }
};
