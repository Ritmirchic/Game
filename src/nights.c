// Night-by-night design table. Escalation is not just HP: each night adds
// new zombie mixes, entry points and systems (power loss, storms, bosses).
#include "game.h"

static const NightConfig g_nights[11] = {
    {0},   // unused
    // n  total conc  interval prep  dur  waves walker runner crawler brute scream mboss boss money power fog dark subtitle
    {1, 16, 5, 3.4f, 220.f, 0.f, 3, 0.90f, 0.10f, 0.00f, 0.00f, 0.00f, 0, 0, 150, 1, 0.30f, 0,
     "They are still learning the way in. Hold the entrance."},
    {2, 26, 7, 3.0f, 200.f, 0.f, 3, 0.74f, 0.26f, 0.00f, 0.00f, 0.00f, 0, 0, 180, 1, 0.34f, 0,
     "Runners in the lot. They sprint the last stretch."},
    {3, 34, 9, 2.6f, 190.f, 0.f, 4, 0.60f, 0.28f, 0.12f, 0.00f, 0.00f, 0, 0, 210, 1, 0.40f, 1,
     "Something is crawling up the loading ramps."},
    {4, 42, 11, 2.3f, 180.f, 0.f, 4, 0.52f, 0.24f, 0.12f, 0.08f, 0.04f, 0, 0, 240, 1, 0.44f, 1,
     "A Brute is coming. It does not care about wood."},
    {5, 48, 12, 2.2f, 240.f, 0.f, 5, 0.46f, 0.24f, 0.12f, 0.10f, 0.08f, 1, 0, 300, 1, 0.46f, 1,
     "THE BUTCHER is awake. It remembers this store."},
    {6, 56, 14, 2.0f, 170.f, 0.f, 5, 0.42f, 0.26f, 0.12f, 0.10f, 0.10f, 0, 0, 320, 0, 0.52f, 2,
     "The grid is down. Your flashlight is the only light you have."},
    {7, 68, 16, 1.8f, 170.f, 0.f, 6, 0.38f, 0.28f, 0.12f, 0.12f, 0.10f, 0, 0, 360, 0, 0.58f, 2,
     "Storm front. They will use the noise of the rain."},
    {8, 82, 18, 1.6f, 170.f, 0.f, 6, 0.32f, 0.32f, 0.12f, 0.13f, 0.11f, 0, 0, 400, 0, 0.62f, 3,
     "The whole parking lot is moving."},
    {9, 96, 20, 1.45f, 170.f, 0.f, 7, 0.28f, 0.34f, 0.12f, 0.14f, 0.12f, 0, 0, 450, 0, 0.66f, 3,
     "One night left after this. Make it count."},
    {10, 110, 22, 1.3f, 300.f, 0.f, 8, 0.26f, 0.34f, 0.12f, 0.15f, 0.13f, 1, 1, 600, 0, 0.70f, 3,
     "THE STOREKEEPER. Everything that died here answers to it."},
};

const NightConfig *night_config(int night) {
    if (night < 1) night = 1;
    if (night > 10) night = 10;
    return &g_nights[night];
}
