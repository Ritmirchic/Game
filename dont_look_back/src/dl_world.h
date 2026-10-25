// ============================================================================
//  dl_world.h : architecture primitives, the apartment complex, props, doors
// ============================================================================
#pragma once
#include "nf_render.h"
#include <memory>

namespace dl {

// ------------------------------------------------------------------ materials
enum MatId {
    MAT_WALLPAPER_STRIPE = 0, MAT_WALLPAPER_DAMASK, MAT_WALLPAPER_FLORAL,
    MAT_PAINT_CREAM, MAT_PAINT_GREEN, MAT_PLASTER_BARE, MAT_BRICK, MAT_CONCRETE_WALL,
    MAT_TILE_BATH, MAT_TILE_KITCHEN, MAT_PANEL_WOOD,
    MAT_PARQUET, MAT_LINO_GREEN, MAT_LINO_BEIGE, MAT_CARPET_RED, MAT_CONCRETE_FLOOR,
    MAT_TILE_LOBBY, MAT_STONE_FLOOR, MAT_WOOD_PLANKS, MAT_DIRT_FLOOR,
    MAT_CEILING, MAT_CEILING_WATER,
    MAT_WOOD_DARK, MAT_WOOD_LIGHT, MAT_METAL_RUST, MAT_METAL_PAINTED, MAT_METAL_STEEL,
    MAT_CERAMIC, MAT_FABRIC_SOFA, MAT_MATTRESS, MAT_CURTAIN, MAT_VELVET,
    MAT_PAINTING_LANDSCAPE, MAT_PAINTING_PORTRAIT, MAT_PAINTING_ABSTRACT, MAT_PHOTO,
    MAT_CLOCK, MAT_RADIO, MAT_DOCUMENT, MAT_BOOKS, MAT_DOOR_PANEL, MAT_DOOR_METAL,
    MAT_GLASS, MAT_LIGHTBASE, MAT_LAMPSHADE, MAT_COBWEB, MAT_EXIT_SIGN, MAT_TV_STATIC,
    MAT_PAPER, MAT_SKIN, MAT_COAT_BLACK, MAT_COAT_WHITE, MAT_HAIR, MAT_FACE,
    MAT_SHADOW, MAT_BLOOD, MAT_WALL_SIGN, MAT_RULES, MAT_NOTE,
    MAT_UNLIT_WHITE, MAT_UNLIT_BLACK, MAT_GRAYBOX,
    MAT_COUNT
};

struct MatLib {
    std::vector<nf::Material> mats;
    void build(nf::Renderer& r, const std::string& texDir);
    const nf::Material& get(int id) const { return mats[id]; }
};

extern const char* kMatNames[MAT_COUNT];

// ------------------------------------------------------------------ geometry builder
struct GeoBuilder {
    nf::Mesh mesh;
    int mat = MAT_PAINT_CREAM;
    float uvScale = 1.0f;      // world units per texture repeat
    nf::V3 uvOffset{0, 0, 0};
    float aoDefault = 1.0f;
    bool flipNormals = false;
    // local frame for props (x right, y forward, z up)
    nf::V3 origin{0, 0, 0};
    float yaw = 0.0f;
    nf::V3 toWorld(const nf::V3& p) const;
    nf::V3 rotDir(const nf::V3& d) const;
    void setFrame(const nf::V3& orig, float y) { origin = orig; yaw = y; }
    void clearFrame() { origin = nf::V3(0, 0, 0); yaw = 0; }
    void lbox(float x0, float y0, float z0, float x1, float y1, float z1,
              bool top = true, bool bottom = false);
    void lboxO(const nf::V3& lo, const nf::V3& hi, bool top = true, bool bottom = false);
    void lcyl(float x, float y, float z, float r, float hgt, int seg, bool capTop = true);
    void lpanel(float x0, float y0, float x1, float y1, float z);   // horizontal panel
    void lquad(const nf::V3& a, const nf::V3& b, const nf::V3& c, const nf::V3& d);

    void quad(const nf::V3& a, const nf::V3& b, const nf::V3& c, const nf::V3& d,
              const nf::V3& n, float u0, float v0, float u1, float v1,
              float aoA = -1, float aoB = -1, float aoC = -1, float aoD = -1);
    // Axis-aligned box with world-space texture projection (seam-correct).
    void box(const nf::Box3& b, bool floorTop = true, bool ceilBottom = true);
    void box(const nf::V3& lo, const nf::V3& hi, bool floorTop = true, bool ceilBottom = true) {
        box(nf::Box3(lo, hi), floorTop, ceilBottom);
    }
    // Box with bottom-left-origin uv scaling for bespoke props.
    void boxSimple(const nf::Box3& b, float uv = 1.0f);
    // A cylinder centred at base point, axis Z.
    void cylinder(const nf::V3& base, float r, float h, int seg, bool capTop = true, bool capBottom = false);
    // Cylinder oriented along an arbitrary axis (for pipes).
    void cylinderAxis(const nf::V3& a, const nf::V3& b, float r, int seg);
    void sphere(const nf::V3& c, float r, int segU, int segV);
    void plane(const nf::V3& center, const nf::V3& right, const nf::V3& upv, float w, float h,
               float uvScale = 1.0f);
    // Wall with a rectangular hole (door or window): builds 4 segments.
    void wallWithHole(const nf::V3& start, const nf::V3& end, float z0, float z1,
                      float holeX0, float holeX1, float holeZ0, float holeZ1,
                      float thickness, bool bothSides = true, int edgeMat = -1);
    void finish(nf::V3* boundsLo = nullptr, nf::V3* boundsHi = nullptr);
};

// ------------------------------------------------------------------ colliders
struct Collider {
    nf::Box3 box;
    int doorId = -1;        // non-negative: collider belongs to a door and is dynamic
    int ownerId = -1;       // prop group id (for anomalies to hide/move props)
    bool blocksSight = true;
    bool blocksWalk = true;
};

// ------------------------------------------------------------------ doors
enum class DoorState { Closed, Open, Locked, Ajar, Barred };

struct Door {
    std::string id;
    int index = -1;
    nf::V3 hinge;          // hinge post position (bottom)
    nf::V3 axis;           // direction from hinge to the far edge along the wall
    float width = 0.85f;
    float height = 2.05f;
    float thickness = 0.05f;
    int swing = 1;         // +1 opens toward positive wall normal
    bool isDouble = false;
    DoorState state = DoorState::Closed;
    float openness = 0.0f;
    float targetOpenness = 0.0f;
    nf::V3 normal;         // wall normal for swing direction
    int meshSlot = -1;
    nf::Box3 closedBox;
    std::string lockedHint;
    std::string opensWith;
    bool anomalyControlled = false;
    float creakTimer = 0.0f;
    int audioHandle = 0;
};

// ------------------------------------------------------------------ interactables
enum class InteractType { Door, Document, Item, Lever, Switch, Report, Elevator, Custom };

struct Interactable {
    std::string id;
    std::string prompt;
    nf::V3 pos;
    float radius = 2.0f;
    InteractType type = InteractType::Custom;
    int index = -1;         // door index / document index / item index
    bool enabled = true;
    bool requiresFlashlight = false;
    std::string denyMessage;
};

// ------------------------------------------------------------------ floor plan rooms
struct RoomInfo {
    std::string id;
    std::string name;
    nf::Box3 bounds;
    nf::V3 lightProbe;
    int floor = 3;
    bool anomalyAllowed = true;
};

// ------------------------------------------------------------------ nav grid
struct NavGrid {
    nf::V3 origin{-14, -14, -4};
    float cell = 0.25f;
    int nx = 0, ny = 0, nz = 0;
    std::vector<uint8_t> blocked;
    void init(const nf::V3& lo, const nf::V3& hi, float cellSize);
    int index(int x, int y, int z) const { return (z * ny + y) * nx + x; }
    bool valid(int x, int y, int z) const { return x >= 0 && y >= 0 && z >= 0 && x < nx && y < ny && z < nz; }
    bool walkable(int x, int y, int z) const { return valid(x, y, z) && !blocked[index(x, y, z)]; }
    void setBox(const nf::Box3& b, bool block, int expand = 0);
    bool pointWalkable(const nf::V3& p) const;
    bool cellOf(const nf::V3& p, int& x, int& y, int& z) const;
    nf::V3 cellCenter(int x, int y, int z) const;
    bool rayWalk(const nf::V3& a, const nf::V3& b, int samples = 0) const;
    bool nearestWalkable(const nf::V3& p, nf::V3& out) const;
    // A* on the layer z
    bool findPath(const nf::V3& from, const nf::V3& to, std::vector<nf::V3>& out, int maxNodes = 9000) const;
};

// ------------------------------------------------------------------ the world
struct PropGroup {
    std::string id;
    std::vector<int> meshIndices;    // indices into World::groups
    nf::Box3 bounds;
    bool visible = true;
};

class World {
public:
    nf::TextureSet* textures = nullptr;
    MatLib mats;

    // static geometry, grouped for anomaly visibility toggling
    struct Group {
        nf::Mesh mesh;
        int mat = MAT_PAINT_CREAM;
        bool visible = true;
        bool castsShadow = true;
        nf::V3 tint{1, 1, 1};
        int id = -1;
        nf::M4 xf;                 // anomaly transforms (move / scale / rotate)
    };
    std::vector<Group> groups;
    std::vector<Collider> colliders;
    std::vector<Door> doors;
    std::vector<Interactable> interactables;
    std::vector<RoomInfo> rooms;
    std::vector<nf::PointLight> lights;      // dynamic mirror of light fixtures
    struct LightFixture {
        nf::V3 pos;
        nf::V3 color{1.0f, 0.86f, 0.66f};
        float baseIntensity = 1.6f;
        float range = 7.0f;
        bool on = true;
        float flicker = 0.0f;
        std::string roomId;
        float phase = 0.0f;
        bool emergency = false;
        int groupId = -1;        // bulb mesh group
        bool dead = false;
    };
    std::vector<LightFixture> fixtures;
    NavGrid nav;
    // colliders that existed when the building was authored: anomalies may add more,
    // but those never reshape the AI graph.
    int structuralColliders = -1;

    // ---- build
    void build(nf::Renderer& r, const std::string& texDir);
    void buildNav();
    void addCollider(const nf::Box3& b, int owner = -1, bool sight = true);
    int beginGroup(int mat, const std::string& name);
    void endGroup(int groupIdx);
    void addLight(const nf::V3& pos, const nf::V3& color, float intensity, float range,
                  const std::string& room, bool emergency = false);
    int addDoor(const std::string& id, const nf::V3& hinge, const nf::V3& axis, float width,
                float height, int swing, const nf::V3& normal, const std::string& lockedHint = "");
    void addInteract(const Interactable& it);
    RoomInfo* roomAt(const nf::V3& p);
    RoomInfo* roomById(const std::string& id);
    Door* doorById(const std::string& id);
    int doorIndex(const std::string& id);
    struct PropRef;
    PropRef* prop(const std::string& id);
    void setGroupVisible(int groupIdx, bool vis);
    void rebuildDoorBox(int doorIdx);
    nf::V3 playerSpawn{0, 0.4f, 1.62f};

    // doors: shared leaf meshes (cached per size) + swing transform
    std::unordered_map<int, nf::Mesh> leafCache;
    const nf::Mesh& doorLeaf(float w, float h);
    nf::M4 doorTransform(const Door& d) const;
    void buildDoorFrame(int doorIdx, int wallMat);
    void finalizeDoors();

    // room / floor bookkeeping
    int currentFloor = 3;
    float floorHeight = 3.4f;

    // documents discovered
    struct Document {
        std::string id;
        std::string title;
        std::string body;
        nf::V3 pos;
        bool found = false;
        int groupId = -1;
        std::string room;
    };
    std::vector<Document> documents;
    int addDocument(const std::string& id, const std::string& title, const std::string& body,
                    const nf::V3& pos, const std::string& room);

    // props (movable furniture) registry used by anomalies
    struct PropRef {
        std::string id;
        int groupA = -1;      // primary visual
        int groupB = -1;      // alternate visual (anomaly)
        nf::V3 pos;
        nf::Box3 collider;
        std::string room;
        bool canMove = true;
    };
    std::vector<PropRef> props;
    int addProp(const std::string& id, const nf::V3& pos, const nf::Box3& collider, const std::string& room);

    // interior builder entry points
    void buildApartment(const nf::V3& origin, float yaw, int variant, const std::string& label, bool playerApt);
    void buildKitchenCorner(const nf::V3& origin, int facing);
    void buildBathroom(const nf::V3& origin, int facing);
    void buildBedroom(const nf::V3& origin, int facing, bool childRoom);
    void buildStairwell();
    void buildBasement();
    void buildLobby();
    void buildMainCorridor();
    void buildFixturesAndDetails();

private:
    int curGroup_ = -1;
    void wallSegment(const nf::V3& a, const nf::V3& b, float z0, float z1, int mat,
                     float thickness = 0.14f, bool bothSides = true);
    void floorSlab(const nf::Box3& b, int floorMat, int ceilMat);
};

// ------------------------------------------------------------------ prop library
namespace props {
void chair(GeoBuilder& g, const nf::V3& base, float yaw, int woodMat);
void table(GeoBuilder& g, const nf::V3& base, float w, float d, float h, int woodMat);
void bed(GeoBuilder& g, const nf::V3& base, float yaw, bool unmade);
void wardrobe(GeoBuilder& g, const nf::V3& base, float yaw, int mat);
void sofa(GeoBuilder& g, const nf::V3& base, float yaw, int fabricMat);
void armchair(GeoBuilder& g, const nf::V3& base, float yaw, int fabricMat);
void kitchenRun(GeoBuilder& g, const nf::V3& base, float len, int facing);
void fridge(GeoBuilder& g, const nf::V3& base, float yaw);
void stove(GeoBuilder& g, const nf::V3& base, float yaw);
void sink(GeoBuilder& g, const nf::V3& base, float yaw);
void toilet(GeoBuilder& g, const nf::V3& base, float yaw);
void bathtub(GeoBuilder& g, const nf::V3& base, float yaw);
void washbasin(GeoBuilder& g, const nf::V3& base, float yaw);
void mirror(GeoBuilder& g, const nf::V3& base, float yaw, float w, float h);
void bookshelf(GeoBuilder& g, const nf::V3& base, float yaw, int shelves, float w);
void desk(GeoBuilder& g, const nf::V3& base, float yaw);
void lamp(GeoBuilder& g, const nf::V3& base, bool floor, bool lit);
void ceilingLight(GeoBuilder& g, const nf::V3& pos, bool fluorescent);
void wallLamp(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal);
void painting(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal, float w, float h, int texMat);
void radiator(GeoBuilder& g, const nf::V3& base, float yaw, float w);
void pipeRun(GeoBuilder& g, const nf::V3& a, const nf::V3& b, float r);
void doorFrame(GeoBuilder& g, const nf::V3& hinge, const nf::V3& axis, const nf::V3& normal,
               float width, float height);
void doorLeaf(GeoBuilder& g, const nf::V3& hinge, const nf::V3& axis, const nf::V3& normal,
              float width, float height, float thickness, float openAngle, int mat);
void windowOpening(GeoBuilder& g, const nf::V3& center, const nf::V3& normal, float w, float h);
void curtain(GeoBuilder& g, const nf::V3& topCenter, const nf::V3& normal, float w, float h);
void rug(GeoBuilder& g, const nf::V3& center, float w, float d, int texMat, float yaw = 0);
void cabinet(GeoBuilder& g, const nf::V3& base, float yaw, float w, float h);
void crate(GeoBuilder& g, const nf::V3& base, float yaw, float s);
void barrel(GeoBuilder& g, const nf::V3& base, float yaw, float r, float h);
void lockers(GeoBuilder& g, const nf::V3& base, float yaw, int count);
void washingMachine(GeoBuilder& g, const nf::V3& base, float yaw);
void boilerTank(GeoBuilder& g, const nf::V3& base, float r, float h);
void boilerPanel(GeoBuilder& g, const nf::V3& base, const nf::V3& normal);
void elevatorDoors(GeoBuilder& g, const nf::V3& center, const nf::V3& normal, float w, float h);
void elevatorFrame(GeoBuilder& g, const nf::V3& center, const nf::V3& normal, float w, float h);
void stairs(GeoBuilder& g, const nf::V3& base, int steps, float rise, float run, float width,
            float yaw, int mat);
void railing(GeoBuilder& g, const nf::V3& a, const nf::V3& b, float h);
void sign(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal, float w, float h, int mat);
void switchPlate(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal);
void socket(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal);
void cobwebMesh(GeoBuilder& g, const nf::V3& corner, const nf::V3& normal, float size);
void tv(GeoBuilder& g, const nf::V3& base, float yaw);
void radio(GeoBuilder& g, const nf::V3& base, float yaw);
void clock(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal);
void plantPot(GeoBuilder& g, const nf::V3& base, float scale);
void suitcase(GeoBuilder& g, const nf::V3& base, float yaw);
void toyBlocks(GeoBuilder& g, const nf::V3& base);
void shoeRack(GeoBuilder& g, const nf::V3& base, float yaw);
void coatHooks(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal);
void fireExtinguisher(GeoBuilder& g, const nf::V3& base);
void mopBucket(GeoBuilder& g, const nf::V3& base);
void drainPipe(GeoBuilder& g, const nf::V3& base, float h);
void wallCrackDecal(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal, float s);
void bloodDecal(GeoBuilder& g, const nf::V3& pos, const nf::V3& normal, float s);
}

} // namespace dl
