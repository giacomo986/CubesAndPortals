--!strict
-- api = 2: a call that answers yes or no gives a boolean (tests/mods/parts,
-- on api 1, checks it still gets a number). Loading is passing.
local answer: any = core.node_solid(1)
assert(type(answer) == "boolean", `api 2: node_solid gave a {type(answer)}`)
core.log("yesno: loaded")
