// LAN multiplayer: host-authoritative UDP replication.
//   host  — simulates everything, broadcasts snapshots at 20 Hz
//   client— sends input, applies authoritative snapshots, predicts own movement
// There is no fakery: if no packets arrive the client shows a disconnected state.
#include "game.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#define sock_nonblock(s) do { u_long nb = 1; ioctlsocket(s, FIONBIO, &nb); } while (0)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close close
#define sock_nonblock(s) do { int fl = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, fl | O_NONBLOCK); } while (0)
#endif

#define NET_MAGIC 0x314E4C54u   /* 'TLN1' little-endian */
#define NET_PORT 27015
#define MAX_CLIENTS 3
#define SNAP_RATE 0.05f
#define INPUT_RATE 0.0333f

enum { PKT_HELLO = 1, PKT_WELCOME, PKT_INPUT, PKT_SNAPSHOT, PKT_BYE, PKT_CHAT };

typedef struct Net {
    sock_t sock;
    struct sockaddr_in peers[4];
    int peer_count;
    int mode;                  // 1 host, 2 client
    int my_index;
    double last_recv;
    int connected;
    uint32_t seq;
    // pending client input (host side)
    float in_yaw[4], in_pitch[4];
    Vec3 in_move[4];
    uint32_t in_buttons[4];
    int have_input[4];
} Net;

static Net g_net;

typedef struct __attribute__((packed)) Hdr {
    uint32_t magic;
    uint8_t type;
    uint8_t from;
    uint16_t len;
    uint32_t seq;
} Hdr;

typedef struct __attribute__((packed)) PktInput {
    Hdr h;
    float yaw, pitch;
    float mx, mz;
    uint16_t buttons;
    uint16_t weapon;
} PktInput;

typedef struct __attribute__((packed)) PktPlayer {
    float x, y, z, yaw, pitch;
    float hp;
    uint8_t alive, weapon, downed, index;
    int32_t money;
} PktPlayer;

typedef struct __attribute__((packed)) PktZombie {
    uint8_t type, state;
    float x, z, yaw, hp;
    uint16_t flags;
} PktZombie;

typedef struct __attribute__((packed)) PktBarricade {
    uint8_t slot, level, active;
    float hp;
} PktBarricade;

#define MAX_SNAP_Z 96
#define MAX_SNAP_B 24
typedef struct __attribute__((packed)) PktSnapshot {
    Hdr h;
    uint8_t player_count;
    uint8_t zombie_count;
    uint8_t barricade_count;
    uint8_t night;
    uint8_t state;
    uint16_t zombies_remaining;
    uint16_t spawned;
    float night_timer;
    float boss_hp;
    PktPlayer players[MAX_PLAYERS];
    PktZombie zombies[MAX_SNAP_Z];
    PktBarricade barricades[MAX_SNAP_B];
} PktSnapshot;

static int g_ping_frames = 0;
static void update_ping(Game *g, float dt) {
    (void)g; (void)dt;
    g_ping_frames++;
}

static void net_close_socket(void) {
    if (g_net.sock != SOCK_INVALID) {
        sock_close(g_net.sock);
        g_net.sock = SOCK_INVALID;
    }
}

static int net_open(int bind_port) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    g_net.sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_net.sock == SOCK_INVALID) return 0;
    int yes = 1;
    setsockopt(g_net.sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
    if (bind_port) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((unsigned short)NET_PORT);
        if (bind(g_net.sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
            net_close_socket();
            return 0;
        }
    }
    sock_nonblock(g_net.sock);
    return 1;
}

static void net_send_to(const void *data, int len, const struct sockaddr_in *to) {
    if (g_net.sock == SOCK_INVALID || !to) return;
    sendto(g_net.sock, (const char *)data, len, 0, (const struct sockaddr *)to, sizeof *to);
}

void net_start(Game *g, int mode, const char *addr) {
    World *w = g->w;
    memset(&g_net, 0, sizeof g_net);
    g_net.sock = SOCK_INVALID;
    g_net.mode = mode;
    g_net.my_index = 0;
    w->net.mode = mode;
    w->net.port = NET_PORT;
    w->net.running = 1;
    w->net.peer_count = 0;
    if (!net_open(mode == 1)) {
        snprintf(w->net.status, sizeof w->net.status, "SOCKET ERROR - MULTIPLAYER UNAVAILABLE");
        w->net.running = 0;
        w->net.mode = 0;
        return;
    }
    if (mode == 1) {
        snprintf(w->net.status, sizeof w->net.status, "HOSTING ON UDP %d - WAITING FOR PLAYERS", NET_PORT);
        w->player_count = 1;
        LOGI("net: hosting on port %d", NET_PORT);
    } else {
        struct sockaddr_in dst;
        memset(&dst, 0, sizeof dst);
        dst.sin_family = AF_INET;
        dst.sin_port = htons((unsigned short)NET_PORT);
        const char *ip = addr && addr[0] ? addr : "127.0.0.1";
#ifdef _WIN32
        dst.sin_addr.s_addr = inet_addr(ip);
#else
        if (inet_pton(AF_INET, ip, &dst.sin_addr) != 1) dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#endif
        g_net.peers[0] = dst;
        Hdr h = {NET_MAGIC, PKT_HELLO, 0, 0, 1};
        net_send_to(&h, sizeof h, &dst);
        snprintf(w->net.status, sizeof w->net.status, "CONNECTING TO %s:%d", ip, NET_PORT);
        LOGI("net: connecting to %s:%d", ip, NET_PORT);
    }
}

void net_stop(Game *g) {
    World *w = g->w;
    if (g_net.mode == 1) {
        Hdr h = {NET_MAGIC, PKT_BYE, 0, 0, 0};
        for (int i = 0; i < g_net.peer_count; i++) net_send_to(&h, sizeof h, &g_net.peers[i]);
    }
    net_close_socket();
    memset(&g_net, 0, sizeof g_net);
    g_net.sock = SOCK_INVALID;
    w->net.running = 0;
    w->net.mode = 0;
    w->net.status[0] = 0;
#ifdef _WIN32
    WSACleanup();
#endif
}

// ---------------------------------------------------------------------------
static void apply_snapshot(Game *g, PktSnapshot *s) {
    World *w = g->w;
    w->night = s->night;
    w->night_timer = s->night_timer;
    w->zombies_remaining = s->zombies_remaining;
    w->spawned_this_night = s->spawned;
    w->boss_hp_frac = s->boss_hp;
    w->boss_active = s->boss_hp > 0.f;
    GameState st = (GameState)s->state;
    if (w->state == GS_NIGHT && st == GS_DAY) {
        // the host advanced to the next day
        game_start_day(g, w->night + 1);
    }
    for (int i = 0; i < s->player_count && i < MAX_PLAYERS; i++) {
        PktPlayer *pp = &s->players[i];
        Player *p = &w->players[i];
        p->active = 1;
        if (i != w->local_player) {
            p->pos = v3(pp->x, pp->y, pp->z);
            p->yaw = pp->yaw;
            p->pitch = pp->pitch;
        } else {
            // reconcile gently: correct large disagreements only
            Vec3 sv = v3(pp->x, pp->y, pp->z);
            float d = vdist(sv, p->pos);
            w->net.rep_delta = d;
            if (d > 1.2f) p->pos = vlerp(p->pos, sv, 0.5f);
        }
        p->hp = pp->hp;
        p->alive = pp->alive;
        p->downed = pp->downed;
        p->weapon = pp->weapon;
        p->money = pp->money;
    }
    w->player_count = s->player_count;
    // zombies: rebuild the local array from the snapshot
    memset(w->zombies, 0, sizeof w->zombies);
    for (int i = 0; i < s->zombie_count && i < MAX_ZOMBIES; i++) {
        PktZombie *pz = &s->zombies[i];
        Zombie *z = &w->zombies[i];
        z->alive = 1;
        z->type = (ZombieType)pz->type;
        z->state = (ZombieState)pz->state;
        z->pos = v3(pz->x, 0, pz->z);
        z->yaw = pz->yaw;
        z->hp = pz->hp;
        z->max_hp = pz->hp > 0 ? pz->hp : 1.f;
        z->height = pz->type == ZB_CRAWLER ? 0.65f : (pz->type == ZB_BRUTE ? 2.2f : 1.8f);
        z->radius = 0.35f;
        z->scale = (float)(pz->flags & 0xFF) / 100.f;
        if (z->scale < 0.5f) z->scale = 1.f;
        z->limb_seed = (float)((pz->flags >> 8) & 0xFF) / 40.f;
    }
    for (int i = 0; i < MAX_BARRICADES; i++) w->barricades[i].active = 0;
    for (int i = 0; i < s->barricade_count && i < MAX_BARRICADES; i++) {
        PktBarricade *pb = &s->barricades[i];
        int slot = pb->slot;
        if (slot < 0 || slot >= w->level.slot_count) continue;
        Barricade *b = &w->barricades[i];
        b->active = pb->active;
        b->slot = slot;
        b->level = pb->level;
        b->hp = pb->hp;
        b->max_hp = pb->level == 1 ? 110.f : (pb->level == 2 ? 240.f : 430.f);
        b->pos = w->level.slots[slot].pos;
        b->yaw = w->level.slots[slot].yaw;
        b->width = w->level.slots[slot].width;
        b->build_t = 1.f;
        w->level.slots[slot].barricade = i;
    }
}

static void send_snapshot(Game *g) {
    static PktSnapshot s;
    World *w = g->w;
    memset(&s, 0, sizeof s);
    s.h.magic = NET_MAGIC;
    s.h.type = PKT_SNAPSHOT;
    s.h.from = 0;
    s.h.seq = ++g_net.seq;
    s.h.len = sizeof s;
    s.night = (uint8_t)w->night;
    s.state = (uint8_t)w->state;
    s.zombies_remaining = (uint16_t)maxi(0, w->zombies_remaining);
    s.spawned = (uint16_t)w->spawned_this_night;
    s.night_timer = w->night_timer;
    s.boss_hp = w->boss_hp_frac;
    s.player_count = (uint8_t)w->player_count;
    for (int i = 0; i < w->player_count && i < MAX_PLAYERS; i++) {
        Player *p = &w->players[i];
        PktPlayer *pp = &s.players[i];
        pp->x = p->pos.x; pp->y = p->pos.y; pp->z = p->pos.z;
        pp->yaw = p->yaw; pp->pitch = p->pitch;
        pp->hp = p->hp;
        pp->alive = (uint8_t)p->alive;
        pp->downed = (uint8_t)p->downed;
        pp->weapon = (uint8_t)p->weapon;
        pp->index = (uint8_t)i;
        pp->money = p->money;
    }
    int zi = 0;
    for (int i = 0; i < MAX_ZOMBIES && zi < MAX_SNAP_Z; i++) {
        Zombie *z = &w->zombies[i];
        if (!z->alive || z->state == ZS_DEAD) continue;
        PktZombie *pz = &s.zombies[zi++];
        pz->type = (uint8_t)z->type;
        pz->state = (uint8_t)z->state;
        pz->x = z->pos.x;
        pz->z = z->pos.z;
        pz->yaw = z->yaw;
        pz->hp = z->hp;
        pz->flags = (uint16_t)(clampi((int)(z->scale * 100.f), 0, 255) | (clampi((int)(z->limb_seed * 40.f), 0, 255) << 8));
    }
    s.zombie_count = (uint8_t)zi;
    int bi = 0;
    for (int i = 0; i < MAX_BARRICADES && bi < MAX_SNAP_B; i++) {
        Barricade *b = &w->barricades[i];
        if (!b->active) continue;
        PktBarricade *pb = &s.barricades[bi++];
        pb->slot = (uint8_t)b->slot;
        pb->level = (uint8_t)b->level;
        pb->active = 1;
        pb->hp = b->hp;
    }
    s.barricade_count = (uint8_t)bi;
    for (int i = 0; i < g_net.peer_count; i++) net_send_to(&s, sizeof s, &g_net.peers[i]);
}

static void host_apply_input(Game *g, int index, PktInput *in) {
    World *w = g->w;
    if (index <= 0 || index >= MAX_PLAYERS) return;
    Player *p = &w->players[index];
    if (!p->active) return;
    p->yaw = in->yaw;
    p->pitch = in->pitch;
    // movement itself is integrated every simulation frame in net_step_remote(),
    // so a 30 Hz input stream still produces a continuous 60 Hz walk
    if (in->buttons & 1) combat_trigger(g, p, 0);
    if (in->buttons & 2) { p->reloading = 1; p->reload_t = g_weapon_defs[p->weapon].reload_time; }
    if (in->weapon < WP_COUNT && p->owned[in->weapon]) p->weapon = in->weapon;
}

// Host-side integration of every remote player, once per simulation frame.
static void net_step_remote(Game *g, float dt) {
    World *w = g->w;
    for (int i = 1; i < MAX_PLAYERS; i++) {
        if (!g_net.have_input[i]) continue;
        Player *p = &w->players[i];
        if (!p->active || !p->alive) continue;
        Vec3 wish = g_net.in_move[i];
        if (vlen(wish) > 1.f) wish = vnorm(wish);
        float speed = 4.35f;
        Vec3 target = vscale(wish, speed);
        Vec3 diff = vsub(target, v3(p->vel.x, 0, p->vel.z));
        float dl = vlen(diff);
        if (dl > 0.0001f) {
            float st = minf(dl, 42.f * dt);
            p->vel = vadd(p->vel, vscale(vnorm(diff), st));
        }
        if (vlen(v3(p->vel.x, 0, p->vel.z)) < 0.01f) { p->vel.x = 0.f; p->vel.z = 0.f; continue; }
        Vec3 np = vmad(p->pos, p->vel, dt);
        if (!world_point_blocked(g, np, p->radius)) p->pos = np;
        else { p->vel.x = 0.f; p->vel.z = 0.f; }
        p->pos.y = 0.f;
    }
}

void net_update(Game *g, float dt) {
    World *w = g->w;
    if (!w->net.running || g_net.sock == SOCK_INVALID) return;
    g_net.last_recv += dt;
    // ---- receive
    for (int guard = 0; guard < 64; guard++) {
        char buf[8192];
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        int n = (int)recvfrom(g_net.sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) break;
        if (n < (int)sizeof(Hdr)) continue;
        Hdr *h = (Hdr *)buf;
        if (h->magic != NET_MAGIC) continue;
        g_net.last_recv = 0.f;
        w->net.peer_count = g_net.peer_count;
        switch (h->type) {
            case PKT_HELLO: {
                if (g_net.mode != 1) break;
                // find or add the peer
                int found = -1;
                for (int i = 0; i < g_net.peer_count; i++)
                    if (g_net.peers[i].sin_addr.s_addr == from.sin_addr.s_addr &&
                        g_net.peers[i].sin_port == from.sin_port) found = i;
                if (found < 0 && g_net.peer_count < MAX_CLIENTS) {
                    g_net.peers[g_net.peer_count++] = from;
                    found = g_net.peer_count - 1;
                    LOGI("net: peer %d joined", found);
                }
                if (found >= 0) {
                    int idx = found + 1;
                    Hdr wl = {NET_MAGIC, PKT_WELCOME, (uint8_t)idx, 0, 0};
                    net_send_to(&wl, sizeof wl, &from);
                    if (!w->players[idx].active) {
                        player_spawn(g, &w->players[idx], idx);
                        w->players[idx].pos = vadd(w->level.player_spawn, v3((float)idx * 1.2f, 0, 0));
                        w->player_count = maxi(w->player_count, idx + 1);
                        snprintf(w->subtitle, sizeof w->subtitle, "A survivor has joined the store.");
                        w->subtitle_t = 3.f;
                    }
                    snprintf(w->net.status, sizeof w->net.status, "HOSTING - %d CLIENT(S) CONNECTED", g_net.peer_count);
                }
                break;
            }
            case PKT_WELCOME:
                g_net.my_index = h->from;
                w->local_player = h->from;
                g_net.connected = 1;
                w->net.running = 1;
                w->net.mode = 2;
                snprintf(w->net.status, sizeof w->net.status, "CONNECTED AS PLAYER %d", h->from + 1);
                LOGI("net: welcomed as player %d", h->from);
                break;
            case PKT_INPUT: {
                if (g_net.mode != 1) break;
                PktInput *in = (PktInput *)buf;
                int idx = h->from;
                if (idx > 0 && idx < MAX_PLAYERS) {
                    g_net.in_yaw[idx] = in->yaw;
                    g_net.in_pitch[idx] = in->pitch;
                    g_net.in_move[idx] = v3(in->mx, 0, in->mz);
                    g_net.in_buttons[idx] = in->buttons;
                    g_net.have_input[idx] = 1;
                    host_apply_input(g, idx, in);
                }
                break;
            }
            case PKT_SNAPSHOT:
                if (g_net.mode != 2) break;
                apply_snapshot(g, (PktSnapshot *)buf);
                g_net.connected = 1;
                update_ping(g, (float)dt);
                break;
            case PKT_BYE:
                if (g_net.mode == 1) {
                    snprintf(w->net.status, sizeof w->net.status, "A PLAYER LEFT THE STORE");
                } else {
                    w->net.status[0] = 0;
                    w->net.running = 0;
                    w->state = GS_MAIN_MENU;
                    snprintf(w->subtitle, sizeof w->subtitle, "HOST DISCONNECTED");
                    w->subtitle_t = 4.f;
                }
                break;
            default: break;
        }
    }
    // ---- transmit
    if (g_net.mode == 1) {
        net_step_remote(g, dt);
        g_net.seq++;
        static float snap_t = 0.f;
        snap_t += dt;
        if (snap_t >= SNAP_RATE) {
            snap_t = 0.f;
            send_snapshot(g);
        }
        g_net.my_index = 0;
        w->local_player = 0;
    } else if (g_net.mode == 2) {
        static float in_t = 0.f;
        in_t += dt;
        if (in_t >= INPUT_RATE) {
            in_t = 0.f;
            PktInput pkt;
            memset(&pkt, 0, sizeof pkt);
            pkt.h.magic = NET_MAGIC;
            pkt.h.type = PKT_INPUT;
            pkt.h.from = (uint8_t)g_net.my_index;
            pkt.h.len = sizeof pkt;
            pkt.h.seq = ++g_net.seq;
            Player *p = &w->players[w->local_player];
            pkt.yaw = p->yaw;
            pkt.pitch = p->pitch;
            Input *in = &g->plat->in;
            float mz = 0.f, mx = 0.f;
            if (in->down['W']) mz -= 1.f;
            if (in->down['S']) mz += 1.f;
            if (in->down['A']) mx -= 1.f;
            if (in->down['D']) mx += 1.f;
            float cy = cosf(p->yaw), sy = sinf(p->yaw);
            pkt.mx = mx * cy + mz * sy;
            pkt.mz = -mx * sy + mz * cy;
            pkt.buttons = (uint16_t)((in->mouse_pressed[0] ? 1 : 0) | (in->pressed['R'] ? 2 : 0));
            pkt.weapon = (uint16_t)p->weapon;
            net_send_to(&pkt, sizeof pkt, &g_net.peers[0]);
        }
    }
    // keep knocking until the host answers; the first datagram may be dropped
    if (g_net.mode == 2 && !g_net.connected) {
        static float hello_t = 0.f;
        hello_t += dt;
        if (hello_t >= 0.5f) {
            hello_t = 0.f;
            Hdr h = {NET_MAGIC, PKT_HELLO, 0, 0, ++g_net.seq};
            net_send_to(&h, sizeof h, &g_net.peers[0]);
        }
    }
    if (g_net.mode == 2 && g_net.last_recv > 5.f && g_net.connected) {
        snprintf(w->net.status, sizeof w->net.status, "CONNECTION LOST");
        w->net.running = 0;
        g_net.connected = 0;
    }
}
