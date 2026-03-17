/** @file lua_scardtool.cpp
 * @brief scardtool Lua 5.4 extension.
 *
 * Derleme: -DBUILD_LUA_EXTENSION=ON
 * Kullanım (Lua tarafında):
 * @code
 *   local sc = require("scardtool")
 *
 *   -- Basit APDU
 *   sc.set_reader(0)
 *   local uid = sc.exec("uid")
 *   print("UID:", uid)
 *
 *   -- Ham APDU
 *   local resp = sc.send_apdu("FF CA 00 00 00")
 *   print("Response:", resp)
 *
 *   -- Script çalıştır
 *   sc.run_file("provision.scard")
 *
 *   -- Reader listesi
 *   local readers = sc.list_readers()
 *   for i, r in ipairs(readers) do print(i, r) end
 *
 *   -- Byte manipülasyon (Lua 5.4 built-in)
 *   local data = string.pack(">I2", 0x1234)  -- big-endian uint16
 *   local apdu = string.format("00B0000%02X%s",
 *                               #data, data:gsub(".", function(c)
 *                                 return string.format("%02X", c:byte())
 *                               end))
 *   sc.send_apdu(apdu)
 * @endcode
 *
 * @par Context yönetimi
 * Her Lua coroutine veya thread için ayrı context kullanılabilir:
 * @code
 *   local sc = require("scardtool")
 *   local ctx = sc.new()          -- yeni context
 *   ctx:set_reader(0)
 *   local uid = ctx:exec("uid")
 *   ctx:close()
 * @endcode
 */
#ifdef SCARDTOOL_BUILD_LUA_EXT

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "ScardToolLib.h"
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Userdata: scardtool_ctx* Lua userdata olarak sarmalanır
// ─────────────────────────────────────────────────────────────────────────────

static const char* CTX_MT = "scardtool.ctx";

/** @brief Lua userdata'dan ctx çıkar. */
static scardtool_ctx* check_ctx(lua_State* L, int idx) {
    void* ud = luaL_checkudata(L, idx, CTX_MT);
    luaL_argcheck(L, ud != nullptr, idx, "scardtool context expected");
    scardtool_ctx** pp = static_cast<scardtool_ctx**>(ud);
    if (!*pp) luaL_error(L, "scardtool context is closed");
    return *pp;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lua context metotları
// ─────────────────────────────────────────────────────────────────────────────

static int l_ctx_set_reader(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    if (lua_isinteger(L, 2))
        scardtool_set_reader(ctx, (int)luaL_checkinteger(L, 2));
    else
        scardtool_set_reader_name(ctx, luaL_checkstring(L, 2));
    return 0;
}

static int l_ctx_set_json(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    scardtool_set_json(ctx, lua_toboolean(L, 2));
    return 0;
}

static int l_ctx_set_verbose(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    scardtool_set_verbose(ctx, lua_toboolean(L, 2));
    return 0;
}

static int l_ctx_set_dry_run(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    scardtool_set_dry_run(ctx, lua_toboolean(L, 2));
    return 0;
}

static int l_ctx_exec(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    const char* cmd = luaL_checkstring(L, 2);
    char buf[8192];
    int ec = scardtool_exec(ctx, cmd, buf, sizeof(buf));
    lua_pushstring(L, buf);
    lua_pushinteger(L, ec);
    return 2;  // output, exit_code
}

static int l_ctx_send_apdu(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    const char* apdu = luaL_checkstring(L, 2);
    char buf[4096];
    int ec = scardtool_send_apdu(ctx, apdu, buf, sizeof(buf));
    if (ec != 0) {
        lua_pushnil(L);
        lua_pushstring(L, scardtool_last_error(ctx));
        return 2;
    }
    lua_pushstring(L, buf);
    return 1;
}

static int l_ctx_run_file(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int ec = scardtool_run_file(ctx, path);
    lua_pushinteger(L, ec);
    return 1;
}

static int l_ctx_run_string(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    const char* script = luaL_checkstring(L, 2);
    int ec = scardtool_run_string(ctx, script);
    lua_pushinteger(L, ec);
    return 1;
}

static int l_ctx_list_readers(lua_State* L) {
    auto* ctx = check_ctx(L, 1);
    char names[16][256];
    int n = scardtool_list_readers(ctx, names, 16);
    if (n < 0) {
        lua_pushnil(L);
        lua_pushstring(L, scardtool_last_error(ctx));
        return 2;
    }
    lua_createtable(L, n, 0);
    for (int i = 0; i < n; ++i) {
        lua_pushstring(L, names[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int l_ctx_close(lua_State* L) {
    void* ud = luaL_checkudata(L, 1, CTX_MT);
    scardtool_ctx** pp = static_cast<scardtool_ctx**>(ud);
    if (*pp) {
        scardtool_destroy(*pp);
        *pp = nullptr;
    }
    return 0;
}

static int l_ctx_gc(lua_State* L) {
    return l_ctx_close(L);
}

static const luaL_Reg ctx_methods[] = {
    {"set_reader",  l_ctx_set_reader},
    {"set_json",    l_ctx_set_json},
    {"set_verbose", l_ctx_set_verbose},
    {"set_dry_run", l_ctx_set_dry_run},
    {"exec",        l_ctx_exec},
    {"send_apdu",   l_ctx_send_apdu},
    {"run_file",    l_ctx_run_file},
    {"run_string",  l_ctx_run_string},
    {"list_readers",l_ctx_list_readers},
    {"close",       l_ctx_close},
    {"__gc",        l_ctx_gc},
    {"__close",     l_ctx_close},  // Lua 5.4 to-be-closed
    {nullptr, nullptr}
};

// ─────────────────────────────────────────────────────────────────────────────
// Module-level fonksiyonlar (module-global default context)
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Yeni context oluştur (Lua tarafından yönetilen userdata). */
static int l_new_ctx(lua_State* L) {
    scardtool_ctx** pp = static_cast<scardtool_ctx**>(
        lua_newuserdatauv(L, sizeof(scardtool_ctx*), 0));
    *pp = scardtool_create();
    if (!*pp) {
        lua_pushnil(L);
        lua_pushstring(L, "scardtool_create failed");
        return 2;
    }
    luaL_setmetatable(L, CTX_MT);
    return 1;
}

static int l_version(lua_State* L) {
    lua_pushstring(L, scardtool_version());
    return 1;
}

/** @brief Module-level exec (otomatik default context). */
static scardtool_ctx* get_default_ctx(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "scardtool._default_ctx");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        // Default ctx yok, oluştur
        scardtool_ctx** pp = static_cast<scardtool_ctx**>(
            lua_newuserdatauv(L, sizeof(scardtool_ctx*), 0));
        *pp = scardtool_create();
        luaL_setmetatable(L, CTX_MT);
        lua_setfield(L, LUA_REGISTRYINDEX, "scardtool._default_ctx");
        return *pp;
    }
    scardtool_ctx** pp = static_cast<scardtool_ctx**>(
        luaL_checkudata(L, -1, CTX_MT));
    lua_pop(L, 1);
    return *pp;
}

static int l_set_reader(lua_State* L) {
    auto* ctx = get_default_ctx(L);
    if (lua_isinteger(L, 1))
        scardtool_set_reader(ctx, (int)luaL_checkinteger(L, 1));
    else
        scardtool_set_reader_name(ctx, luaL_checkstring(L, 1));
    return 0;
}

static int l_exec(lua_State* L) {
    auto* ctx = get_default_ctx(L);
    const char* cmd = luaL_checkstring(L, 1);
    char buf[8192];
    int ec = scardtool_exec(ctx, cmd, buf, sizeof(buf));
    lua_pushstring(L, buf);
    lua_pushinteger(L, ec);
    return 2;
}

static int l_send_apdu(lua_State* L) {
    auto* ctx = get_default_ctx(L);
    const char* apdu = luaL_checkstring(L, 1);
    char buf[4096];
    int ec = scardtool_send_apdu(ctx, apdu, buf, sizeof(buf));
    if (ec != 0) {
        lua_pushnil(L);
        lua_pushstring(L, scardtool_last_error(ctx));
        return 2;
    }
    lua_pushstring(L, buf);
    return 1;
}

static int l_list_readers(lua_State* L) {
    auto* ctx = get_default_ctx(L);
    char names[16][256];
    int n = scardtool_list_readers(ctx, names, 16);
    if (n < 0) {
        lua_pushnil(L);
        lua_pushstring(L, scardtool_last_error(ctx));
        return 2;
    }
    lua_createtable(L, n, 0);
    for (int i = 0; i < n; ++i) {
        lua_pushstring(L, names[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int l_run_file(lua_State* L) {
    auto* ctx = get_default_ctx(L);
    int ec = scardtool_run_file(ctx, luaL_checkstring(L, 1));
    lua_pushinteger(L, ec);
    return 1;
}

static const luaL_Reg module_funcs[] = {
    {"new",          l_new_ctx},
    {"version",      l_version},
    {"set_reader",   l_set_reader},
    {"exec",         l_exec},
    {"send_apdu",    l_send_apdu},
    {"list_readers", l_list_readers},
    {"run_file",     l_run_file},
    {nullptr, nullptr}
};

// ─────────────────────────────────────────────────────────────────────────────
// luaopen_scardtool — extension entry point
// ─────────────────────────────────────────────────────────────────────────────

extern "C" SCARDTOOL_API int luaopen_scardtool(lua_State* L) {
    // Context metatable oluştur
    luaL_newmetatable(L, CTX_MT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");  // mt.__index = mt
    luaL_setfuncs(L, ctx_methods, 0);
    lua_pop(L, 1);

    // Module tablosu oluştur
    luaL_newlib(L, module_funcs);

    // Versiyon alanı
    lua_pushstring(L, scardtool_version());
    lua_setfield(L, -2, "_VERSION");

    return 1;
}

#endif  // SCARDTOOL_BUILD_LUA_EXT
