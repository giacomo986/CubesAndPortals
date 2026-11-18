--!strict
-- A mod split over files with require. Each check asserts; loading is passing.

local a = require("./lib/a")           -- lib/a.luau, which requires ./b beside it
local lib = require("./lib")           -- a directory is its init.luau
assert(a.two == 2, "a file's own require is from its own directory")
assert(lib.name == "lib", "a directory's init.luau")
assert(require("./lib/a") == a, "a file runs once and gives the same thing again")
assert(shared_by_lib == 5, "files share the mod's globals")
-- @self/ is Luau's "inside me": from an init.luau the same as ./, from a.luau the folder a/.
assert(require("@self/lib/a") == a, "@self from an init.luau is beside it")
assert(require("./self") == "inner", "@self from self.luau is in self/")

local function refused(path: string, why: string)
    local ok, err = pcall(function() return require(path) end)
    assert(not ok and string.find(tostring(err), why, 1, true), `{path}: {err}`)
end
refused("../good/init", "outside the mod's directory")
refused("./lib/../../good/init", "outside the mod's directory")
refused("lib/a", "starts with ./, ../ or @self/")
refused("./missing", "no missing.luau")
refused("./loop1", "a cycle")

-- A file with more locals than one chunk may hold, split in two: each half
-- is its own chunk.
assert(require("./many") == 150)

core.log("parts: loaded")
