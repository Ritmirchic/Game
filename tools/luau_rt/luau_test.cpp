// Small helpers exposed to the Lua test harness: filesystem access + process exit.
#include "lua.h"
#include "lualib.h"
#include "luacode.h"

#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
#include <string>

static int l_readfile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        lua_pushnil(L);
        lua_pushfstring(L, "cannot open %s", path);
        return 2;
    }
    std::string out;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    fclose(f);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

static int l_writefile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    fwrite(data, 1, len, f);
    fclose(f);
    lua_pushboolean(L, 1);
    return 1;
}

static int l_listdir(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    DIR* d = opendir(path);
    if (!d)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    int i = 1;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;
        lua_pushstring(L, entry->d_name);
        lua_rawseti(L, -2, i++);
    }
    closedir(d);
    return 1;
}

static int l_isdir(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        lua_pushboolean(L, 1);
    else
        lua_pushboolean(L, 0);
    return 1;
}

// Compile a Luau source string into a callable function (used by the loader).
static int l_loadchunk(lua_State* L)
{
    size_t len = 0;
    const char* src = luaL_checklstring(L, 1, &len);
    const char* name = luaL_optstring(L, 2, "=chunk");
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(src, len, nullptr, &bytecodeSize);
    if (!bytecode)
    {
        lua_pushnil(L);
        lua_pushstring(L, "compile error");
        return 2;
    }
    std::string chunkname = std::string("@") + name;
    int status = luau_load(L, chunkname.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);
    if (status != 0)
    {
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }
    return 1;
}

static int l_exit(lua_State* L)
{
    int code = (int)luaL_optinteger(L, 1, 0);
    exit(code);
}

static const luaL_Reg testlib[] = {
    {"readFile", l_readfile},
    {"writeFile", l_writefile},
    {"listDir", l_listdir},
    {"isDir", l_isdir},
    {"exit", l_exit},
    {"loadChunk", l_loadchunk},
    {nullptr, nullptr},
};

extern "C" int luaopen_luau_test(lua_State* L)
{
    lua_newtable(L);
    for (const luaL_Reg* r = testlib; r->name; r++)
    {
        lua_pushcfunction(L, r->func, r->name);
        lua_setfield(L, -2, r->name);
    }
    return 1;
}
