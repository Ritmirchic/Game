// Minimal Luau host used as an offline test harness for the game's Luau code.
// Modes:
//   luau_rt --check <file...>      parse-only (syntax validation)
//   luau_rt --run <file> [args...] compile + execute a file
#include "lua.h"
#include "lualib.h"
#include "luacode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" int luaopen_luau_test(lua_State* L);

static std::string read_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    std::string out;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    fclose(f);
    return out;
}

static int run_file(lua_State* L, const char* path, int nargs)
{
    std::string src = read_file(path);
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(src.data(), src.size(), nullptr, &bytecodeSize);
    if (!bytecode)
    {
        fprintf(stderr, "compile failed: %s\n", path);
        return 3;
    }
    std::string chunkname = std::string("@") + path;
    int status = luau_load(L, chunkname.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);
    if (status != 0)
    {
        const char* msg = lua_tostring(L, -1);
        fprintf(stderr, "load error: %s\n", msg ? msg : "?");
        return 3;
    }
    int base = lua_gettop(L) - nargs;
    if (lua_pcall(L, nargs, 0, 0) != 0)
    {
        const char* msg = lua_tostring(L, -1);
        fprintf(stderr, "runtime error: %s\n", msg ? msg : "?");
        return 4;
    }
    (void)base;
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s --check|--run file...\n", argv[0]);
        return 1;
    }
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaopen_luau_test(L);
    lua_setglobal(L, "TestHost");

    if (strcmp(argv[1], "--check") == 0)
    {
        int failures = 0;
        for (int i = 2; i < argc; i++)
        {
            std::string src = read_file(argv[i]);
            size_t bytecodeSize = 0;
            char* bytecode = luau_compile(src.data(), src.size(), nullptr, &bytecodeSize);
            if (!bytecode)
            {
                printf("SYNTAX %s\n", argv[i]);
                failures++;
            }
            else
            {
                free(bytecode);
            }
        }
        if (failures)
        {
            fprintf(stderr, "%d file(s) failed to parse\n", failures);
            return 5;
        }
        printf("parsed %d file(s) OK\n", argc - 2);
        return 0;
    }

    if (strcmp(argv[1], "--run") == 0)
    {
        int nargs = argc - 3;
        lua_newtable(L);
        for (int i = 0; i < nargs; i++)
        {
            lua_pushstring(L, argv[3 + i]);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setglobal(L, "args");
        return run_file(L, argv[2], 0);
    }
    fprintf(stderr, "unknown mode %s\n", argv[1]);
    return 1;
}
