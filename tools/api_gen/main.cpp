// Runs the binding generator.
//
// DESIGN.md 5.4 asks for the API to be declared once, in a table, with a build
// time script generating the C++ glue, the .d.luau type definitions and the
// documentation -- and for that script to be in Lua, so no new toolchain enters
// the project (constraint 4).
//
// This is the thirty lines that make that possible: the Luau CLI cannot write
// files, so the generator gets exactly one host function that can. Everything
// else about the generation -- the description, the code, the formatting -- is
// in the .luau files next to this one, which is the whole point.

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {

int l_write_file(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    size_t      len  = 0;
    const char *text = luaL_checklstring(L, 2, &len);

    // Only write when the content actually changed. A generator that rewrites
    // its outputs on every build makes every build rebuild everything that
    // includes them, and then people start turning the generator off.
    {
        std::ifstream in(path, std::ios::binary);
        if (in) {
            std::stringstream buf;
            buf << in.rdbuf();
            if (buf.str() == std::string(text, len)) {
                std::printf("  unchanged  %s\n", path);
                lua_pushboolean(L, 0);
                return 1;
            }
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) luaL_error(L, "cannot write %s", path);
    out.write(text, (std::streamsize)len);

    std::printf("  wrote      %s\n", path);
    lua_pushboolean(L, 1);
    return 1;
}

// The other direction, for the check that the hand-written calls are documented.
int l_read_file(lua_State *L)
{
    const char   *path = luaL_checkstring(L, 1);
    std::ifstream in(path, std::ios::binary);
    if (!in) luaL_error(L, "cannot read %s", path);
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string text = buf.str();
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}

bool run_file(lua_State *L, const char *path, int results)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "api_gen: cannot open %s\n", path);
        return false;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string source = buf.str();

    size_t             bytecode_size = 0;
    lua_CompileOptions opts{};
    opts.optimizationLevel = 1;
    opts.debugLevel        = 1;

    char *bytecode = luau_compile(source.c_str(), source.size(), &opts, &bytecode_size);
    if (!bytecode) {
        std::fprintf(stderr, "api_gen: cannot compile %s\n", path);
        return false;
    }

    const int rc = luau_load(L, path, bytecode, bytecode_size, 0);
    std::free(bytecode);

    if (rc != 0 || lua_pcall(L, 0, results, 0) != 0) {
        std::fprintf(stderr, "api_gen: %s: %s\n", path, lua_tostring(L, -1));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: cnp_api_gen <api.luau> <gen.luau> <source root>\n");
        return 2;
    }

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, l_write_file, "write_file");
    lua_setglobal(L, "write_file");
    lua_pushcfunction(L, l_read_file, "read_file");
    lua_setglobal(L, "read_file");

    lua_pushstring(L, argv[3]);
    lua_setglobal(L, "ROOT");

    // The description first: it returns the table the generator reads.
    if (!run_file(L, argv[1], 1)) {
        lua_close(L);
        return 1;
    }
    lua_setglobal(L, "API");

    const bool ok = run_file(L, argv[2], 0);
    lua_close(L);
    return ok ? 0 : 1;
}
