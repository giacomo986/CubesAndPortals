--!nonstrict
-- Generates the C++ glue, the .d.luau type definitions and the reference
-- documentation from src/script/api.luau (DESIGN.md 5.4).
--
-- The generator is in Lua on purpose: the constraint is that no new toolchain
-- enters the project, and this runs on the Luau that is already a submodule,
-- through a host that gives it exactly one function -- write_file.
--
-- What is NOT generated: core.event.*, core.ecs.*, core.net.* and
-- core.render.*. They take tables or FUNCTIONS rather than a fixed list of
-- scalars -- and core.ecs's entity ids are doubles, because an id carries a
-- generation above its index and luaL_checkinteger would truncate it. A
-- generator that handled all that would be bigger than the calls it replaced.
-- They stay hand-written in script_luau.cpp, and this file says so rather than
-- pretending otherwise.
--
-- Three kinds of return, and the last two arrived with M8:
--
--   fn.ret  = "int" | "float" | "string" | "bool"   one value
--   fn.ret  = "flag"                                a yes or no: true/false from
--                                                   api 2 on, 1/0 under api 1
--   fn.ret  = "int[]"                               a list, as a Lua array
--   fn.rets = { "float", "float", "float" }         several values
--
-- The list and the several came in together because the alternative was
-- spelling them as scalars -- player_count() plus player_at(i) plus
-- player_x/y/z(id), five calls where two do -- and every future call that
-- answers with more than one number would have paid the same price. See
-- docs/archive/M8_FINDINGS.md.
--
-- Both are out-parameters on the C side, because a C function pointer cannot
-- return a vector: a list fills a caller's buffer and returns how many it
-- wrote, and several values are written through pointers with a 1 or 0 saying
-- whether there was an answer at all.

local api = API :: any
local root = ROOT :: string

-- The calls written by hand (OPEN-41): every `lua_pushcfunction(L, fn, "core.x")`
-- in script_luau.cpp names one, and each must appear in MODDING.md, or the
-- build stops here -- a hand-written call with no page is how the reference drifts.
local hand_names = {}
do
    local src = read_file(`{root}/src/script/script_luau.cpp`)
    local modding = read_file(`{root}/docs/MODDING.md`)
    local missing = {}
    for name in string.gmatch(src, 'lua_pushcfunction%\(L, [%w_]+, "(core%.[%w_%.]+)"%\)') do
        table.insert(hand_names, `\`{name}\``)
        if not string.find(modding, name, 1, true) then table.insert(missing, name) end
    end
    if #missing > 0 then
        error(`not in docs/MODDING.md: {table.concat(missing, ", ")}`)
    end
end

-- ------------------------------------------------------------------ types

local lua_type_of = {
    int = "number",
    float = "number",
    string = "string",
    bool = "boolean",
    -- A return only: a yes or no. The host answers 1 or 0 as ever; a mod
    -- written for api 2 gets true or false, one for api 1 the number it always
    -- had -- 0 is true in Luau, which is what made a 1/0 answer a trap.
    flag = "boolean",
}

local check_of = {
    int = "luaL_checkinteger",
    float = "luaL_checknumber",
    string = "luaL_checkstring",
}

local opt_of = {
    int = "luaL_optinteger",
    float = "luaL_optnumber",
    string = "luaL_optstring",
}

local c_type_of = {
    int = "int",
    float = "float",
    string = "const char *",
    bool = "int",
    flag = "int",
}

local function cpp_default(arg): string
    if arg.type == "bool" then
        return if arg.default then "1" else "0"
    elseif arg.type == "string" then
        return `"{arg.default}"`
    end
    return tostring(arg.default)
end

-- The element type of a list return, or nil when this is not one.
local function list_element(fn): string?
    if type(fn.ret) ~= "string" then return nil end
    local base = string.match(fn.ret, "^(%a+)%[%]$")
    return base
end

local function return_type(fn): string
    local element = list_element(fn)
    if element then
        return `\{ {lua_type_of[element]} \}`
    end
    if fn.rets then
        local parts = {}
        -- Optional, every one of them, and that is not hedging. A call that
        -- answers with several values answers with NONE when there is nothing
        -- to answer about -- a player who has left has no position, and 0 0 0
        -- is a real place in the world. `if not x then` is the check, and the
        -- type says so: the first value is optional and the rest are not,
        -- because a type checker refines the value it was asked about and
        -- never its siblings -- all optional and `if not x` would leave y and
        -- z still maybe-nil.
        for i, r in fn.rets do
            table.insert(parts, `{lua_type_of[r]}{if i == 1 then "?" else ""}`)
        end
        return `({table.concat(parts, ", ")})`
    end
    return if fn.ret then lua_type_of[fn.ret] else "()"
end

local function signature(fn): string
    local parts = {}
    for _, arg in fn.args do
        local t = lua_type_of[arg.type]
        table.insert(parts, `{arg.name}: {t}{if arg.default ~= nil then "?" else ""}`)
    end
    return `({table.concat(parts, ", ")}) -> {return_type(fn)}`
end

-- --------------------------------------------------------------- C++ glue

local function gen_cpp(): string
    local out = {}
    local function w(line: string)
        table.insert(out, line)
    end

    w("// GENERATED by tools/api_gen from src/script/api.luau -- do not edit.")
    w("//")
    w("// The point of generating it (DESIGN.md 5.4) is that the C++ argument")
    w("// checking, the .d.luau types and the documentation cannot drift apart:")
    w("// they all come from one table. Edit src/script/api.luau and rebuild.")
    w("")

    for _, fn in api do
        w(`// core.{fn.name}{signature(fn)}`)
        w(`int l_{fn.name}(lua_State *L)`)
        w("{")
        w("    VmState *vm = host_of(L);")

        for i, arg in fn.args do
            local ct = c_type_of[arg.type]

            -- `const char *` is already spelled with its own const: prefixing
            -- another one produces `const const char *`, which is what the
            -- first generated file said before anybody read it.
            local decl = if arg.type == "string" then `const char *{arg.name}`
                         else `const {ct} {arg.name}`

            if arg.type == "bool" then
                local fallback = if arg.default ~= nil then cpp_default(arg) else "0"
                w(`    {decl} = lua_isnoneornil(L, {i}) ? {fallback} : (lua_toboolean(L, {i}) ? 1 : 0);`)
            elseif arg.default ~= nil then
                w(`    {decl} = ({ct}){opt_of[arg.type]}(L, {i}, {cpp_default(arg)});`)
            else
                w(`    {decl} = ({ct}){check_of[arg.type]}(L, {i});`)
            end
        end

        local call_args = { "vm->host.ud" }
        for _, arg in fn.args do
            table.insert(call_args, arg.name)
        end
        local call = `vm->host.{fn.host}({table.concat(call_args, ", ")})`

        local element = list_element(fn)

        if element then
            -- A list. The host fills a buffer the size of which it is told, and
            -- returns how many it wrote; the count is clamped here rather than
            -- trusted, because a host that returns more than the buffer holds
            -- would be read past the end of it.
            local ct = c_type_of[element]
            local max = fn.ret_max or 32
            local list_call = `vm->host.{fn.host}({table.concat(call_args, ", ")}, buffer, {max})`
            w(`    {ct} buffer[{max}] = \{0\};`)
            w(`    const int written = (vm && vm->host.{fn.host}) ? {list_call} : 0;`)
            w(`    const int count = written < 0 ? 0 : (written > {max} ? {max} : written);`)
            w("    lua_createtable(L, count, 0);")
            w("    for (int i = 0; i < count; ++i) {")
            if element == "float" then
                w("        lua_pushnumber(L, buffer[i]);")
            else
                w("        lua_pushinteger(L, buffer[i]);")
            end
            w("        lua_rawseti(L, -2, i + 1);")
            w("    }")
            w("    return 1;")
        elseif fn.rets then
            -- Several values, through out-parameters, with a 1 or 0 saying
            -- whether there was an answer. NOTHING is pushed when there was
            -- not: a mod that reads a position for a player who has left gets
            -- nil rather than the origin, which is a real place.
            local outs = {}
            for i, r in fn.rets do
                w(`    {c_type_of[r]} out{i} = ({c_type_of[r]})0;`)
                table.insert(outs, `&out{i}`)
            end
            local multi_call =
                `vm->host.{fn.host}({table.concat(call_args, ", ")}, {table.concat(outs, ", ")})`
            w(`    const int answered = (vm && vm->host.{fn.host}) ? {multi_call} : 0;`)
            w("    if (!answered) return 0;")
            for i, r in fn.rets do
                if r == "int" then
                    w(`    lua_pushinteger(L, out{i});`)
                elseif r == "bool" then
                    w(`    lua_pushboolean(L, out{i});`)
                else
                    w(`    lua_pushnumber(L, out{i});`)
                end
            end
            w(`    return {#fn.rets};`)
        elseif fn.ret then
            local ct = c_type_of[fn.ret]
            local decl = if fn.ret == "string" then "const char *result" else `const {ct} result`
            local zero = if fn.ret == "string" then "nullptr" else `({ct})0`
            w(`    {decl} = (vm && vm->host.{fn.host}) ? {call} : {zero};`)
            if fn.ret == "int" then
                w("    lua_pushinteger(L, result);")
            elseif fn.ret == "float" then
                w("    lua_pushnumber(L, result);")
            elseif fn.ret == "string" then
                w("    lua_pushstring(L, result ? result : \"\");")
            elseif fn.ret == "bool" then
                w("    lua_pushboolean(L, result);")
            elseif fn.ret == "flag" then
                w("    if (vm && vm->api >= 2) lua_pushboolean(L, result != 0);")
                w("    else lua_pushinteger(L, result);")
            end
            w("    return 1;")
        else
            w(`    if (vm && vm->host.{fn.host}) {call};`)
            w("    return 0;")
        end

        w("}")
        w("")
    end

    w("// Puts the generated functions on the table at the top of the stack.")
    w("void register_generated_api(lua_State *L)")
    w("{")
    for _, fn in api do
        w(`    lua_pushcfunction(L, l_{fn.name}, "core.{fn.name}");`)
        w(`    lua_setfield(L, -2, "{fn.name}");`)
    end
    w("}")
    w("")

    return table.concat(out, "\n")
end

-- ------------------------------------------------------------- .d.luau

local function gen_dluau(): string
    local out = {}
    local function w(line: string)
        table.insert(out, line)
    end

    w("--!strict")
    w("-- GENERATED by tools/api_gen from src/script/api.luau -- do not edit.")
    w("--")
    w("-- Point an editor's Luau language server at this file and `core` becomes")
    w("-- typed: wrong argument counts and wrong types are errors before the game")
    w("-- has been started once.")
    w("")
    w("-- What a mapgen stage is handed. Read-only: a stage writes with")
    w("-- core.set_block and reads the ground with core.terrain_height.")
    w("-- min/max are the column of cells this call generates.")
    w("export type MapgenStage = {")
    w("    seed: number,")
    w("    height: number,")
    w("    depth: number,")
    w("    min_x: number,")
    w("    min_z: number,")
    w("    max_x: number,")
    w("    max_z: number,")
    w("}")
    w("")
    w("-- A payload's fields are whatever its event's schema declared, so a")
    w("-- handler reads them as the types it knows they are.")
    w("export type EventPayload = { [string]: any }")
    w("")
    w("-- The cell an ABM or LBM fires on.")
    w("export type ModifierCell = { x: number, y: number, z: number, id: number }")
    w("")
    w("-- What a core.ecs.system is handed: n entities, and a column (indexed")
    w("-- from 1) per field -- x, y, z and the component's own. Writes are read back.")
    w("export type EcsBatch = { n: number, dt: number, tick: number, id: {number}, [string]: any }")
    w("")
    w("export type CraftPose = {")
    w("    x: number, y: number, z: number,")
    w("    yaw: number, pitch: number, roll: number,")
    w("    vx: number, vy: number, vz: number,")
    w("    speed: number, grounded: boolean, impact: number, throttle: number,")
    w("}")
    w("export type EventSchema = {")
    w("    fields: { [string]: string },")
    w("    cancellable: boolean?,")
    w("}")
    w("")
    w("declare core: {")

    for _, fn in api do
        -- A doc may run to paragraphs: every line of it must stay a comment.
        for _, line in string.split(fn.doc, "\n") do
            w(if line == "" then "    --" else `    -- {line}`)
        end
        w(`    {fn.name}: {signature(fn)},`)
    end

    w("")
    w("    -- Hand-written, not generated: these take tables or functions rather")
    w("    -- than scalars.")
    w("    register_mapgen: (name: string, stage: (MapgenStage) -> ()) -> (),")
    w("    event: {")
    w("        define: (name: string, schema: EventSchema) -> (),")
    w("        on: (name: string, priorityOrHandler: number | (EventPayload) -> (),")
    w("             handler: ((EventPayload) -> ())?) -> number,")
    w("        emit: (name: string, payload: EventPayload?) -> boolean,")
    w("    },")
    w("    register_abm: (name: string, nodes: string | {string}, interval: number, chance: number,")
    w("                   action: (ModifierCell) -> ()) -> (),")
    w("    register_lbm: (name: string, nodes: string | {string}, action: (ModifierCell) -> ()) -> (),")
    w("    ecs: {")
    w("        component: (name: string, fields: {string}) -> (),")
    w("        phase: (name: string, order: number) -> (),")
    w("        system: (name: string, phase: string, component: string, fn: (EcsBatch) -> ()) -> (),")
    w("        spawn: (component: string, x: number?, y: number?, z: number?) -> number?,")
    w("        set: (id: number, field: string, value: number) -> boolean,")
    w("        get: (id: number, field: string) -> number?,")
    w("        kill: (id: number) -> boolean,")
    w("        visible: (id: number, on: boolean?) -> boolean,")
    w("        model: (id: number, model: number) -> boolean,")
    w("        health: (id: number, hp: number?) -> number?,")
    w("        clip: (id: number, name: string?) -> boolean,")
    w("        box: (id: number, half: number, height: number) -> boolean,")
    w("        count: (component: string) -> number,")
    w("    },")
    w("    craft: {")
    w("        make: (id: number, kind: string, mass: number, half_w: number, half_h: number,")
    w("               half_l: number, accel: number, top: number, turn: number) -> boolean,")
    w("        keys: (id: number, forward: number?, right: number?, up: number?, yaw: number?) -> boolean,")
    w("        pose: (id: number) -> CraftPose?,")
    w("        push: (id: number, vx: number, vy: number, vz: number) -> boolean,")
    w("        mount: (id: number, on: number, x: number?, y: number?, z: number?) -> boolean,")
    w("        aim: (id: number, yaw: number, pitch: number?) -> boolean,")
    w("        seat: (player: number, id: number, x: number?, y: number?, z: number?) -> boolean,")
    w("    },")
    w("    net: {")
    w("        open: () -> number?,")
    w("        send: (player: number, text: string) -> boolean,")
    w("    },")
    w("    render: {")
    w("        pass: (name: string, order: number, fn: () -> ()) -> (),")
    w("        box: (x: number, y: number, z: number, w: number?, h: number?, d: number?,")
    w("              r: number?, g: number?, b: number?, a: number?) -> boolean,")
    w("        model: (model: number, x: number, y: number, z: number, yaw: number?, scale: number?,")
    w("                pitch: number?, roll: number?, gone: number?, alpha: number?, tint: number?) -> boolean,")
    w("        fx: (kind: string, x: number, y: number, z: number, size: number?, r: number?, g: number?,")
    w("             b: number?, a: number?, t: number?, x2: number?, y2: number?, z2: number?) -> boolean,")
    w("        camera: (id: number, x: number, y: number, z: number, yaw: number?, pitch: number?,")
    w("                 fov: number?, still: boolean?) -> boolean,")
    w("        camera_drop: (id: number) -> boolean,")
    w("        screen: (id: number, x: number, y: number, z: number, yaw: number?, w: number?,")
    w("                 h: number?, plain: boolean?) -> boolean,")
    w("        text: (text: string, x: number, y: number, z: number, yaw: number?, size: number?,")
    w("               r: number?, g: number?, b: number?, a: number?, max_w: number?, max_h: number?) -> boolean,")
    w("        camera_read: (id: number) -> string,")
    w("        photo: (id: number, data: string) -> boolean,")
    w("    },")
    w("    hud: {")
    w("        pass: (name: string, order: number, fn: () -> ()) -> (),")
    w("        text: (x: number, y: number, text: string, size: number?, align: number?,")
    w("               r: number?, g: number?, b: number?, a: number?) -> boolean,")
    w("        rect: (x: number, y: number, w: number, h: number,")
    w("               r: number?, g: number?, b: number?, a: number?) -> boolean,")
    w("        aspect: () -> number,")
    w("        image: (name: string, x: number, y: number, w: number, h: number, a: number?) -> boolean,")
    w("        item: (name: string, x: number, y: number, side: number) -> boolean,")
    w("        mouse: () -> (number?, number?),")
    w("    },")
    w("}")
    w("")

    return table.concat(out, "\n")
end

-- ------------------------------------------------------------------ docs

local function gen_docs(): string
    local out = {}
    local function w(line: string)
        table.insert(out, line)
    end

    w("# Scripting API")
    w("")
    w("<!-- GENERATED by tools/api_gen from src/script/api.luau: do not edit by hand. -->")
    w("")
    w("Everything is under `core`. The types are the ones the type checker sees in")
    w("[`api.d.luau`](api.d.luau); an argument with a default is optional. This file")
    w("is generated and in English only; the guide to writing a mod is in three")
    w("languages: [MODDING.md](MODDING.md), [MODDING.it.md](MODDING.it.md),")
    w("[MODDING.pt.md](MODDING.pt.md).")
    w("")
    w("**What is not here.** The calls that take tables or functions are written")
    w("by hand in `src/script/script_luau.cpp` and documented in")
    w("[MODDING.md](MODDING.md); the generator refuses to run if one is missing there.")
    w(`They are: {table.concat(hand_names, ", ")}.`)
    w("")

    for _, fn in api do
        w(`## \`core.{fn.name}\``)
        w("")
        w("```luau")
        w(`core.{fn.name}{signature(fn)}`)
        w("```")
        w("")
        w(fn.doc)
        w("")
        if fn.ret == "flag" then
            w("Answers `true` or `false`; a mod with `api = 1` gets `1` or `0`, as before.")
            w("")
        end

        if #fn.args > 0 then
            w("| argument | type | default | notes |")
            w("|---|---|---|---|")
            for _, arg in fn.args do
                local default = if arg.default ~= nil then `\`{tostring(arg.default)}\`` else "—"
                w(`| \`{arg.name}\` | {lua_type_of[arg.type]} | {default} | {arg.doc or ""} |`)
            end
            w("")
        end
    end

    w("## `core.event`")
    w("")
    w("Written by hand, not generated: they take tables instead of scalars, and a")
    w("generator that covered them would be bigger than the three functions it")
    w("would replace. Documented in [MODDING.md](MODDING.md#typed-event-bus).")
    w("")

    return table.concat(out, "\n")
end

-- ------------------------------------------------------------------ write

print(`api_gen: {#api} functions`)
write_file(`{root}/src/script/api_generated.inc`, gen_cpp())
write_file(`{root}/docs/api.d.luau`, gen_dluau())
write_file(`{root}/docs/API.md`, gen_docs())
