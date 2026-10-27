--!strict
-- A notice board, as a page (core.page_register): the example of a mod's own
-- page. Everybody reads it; a moderator posts a note from the field at the
-- bottom and takes one down with its button. Kept for every room, in the
-- shared store, a note a line.

core.page_register("board", "Notice board")

local function notes(): { string }
    local out = {}
    for line in string.gmatch(core.shared_storage_get("notes"), "[^\n]+") do
        table.insert(out, line)
    end
    return out
end

local function keep(list: { string })
    core.shared_storage_set("notes", table.concat(list, "\n"))
end

-- What a page shows as text must not read as its markup.
local function plain(s: string): string
    return (string.gsub(s, "[%[%]%(%)|]", "'"))
end

local function show(player: number)
    local staff = core.has_perm(player, "moderator") == 1
    local list  = notes()
    local page  = { "[All pages](page:pages)", "" }
    if #list == 0 then
        table.insert(page, "Nothing on the board.")
    end
    for i, note in list do
        table.insert(page, "## " .. plain(note))
        if staff then
            table.insert(page, `[Take down](act:del{i})`)
        end
    end
    if staff then
        table.insert(page, "")
        table.insert(page, "?[post a note](act:add)")
    end
    core.page_show(player, "board", table.concat(page, "\n"))
end

core.event.on("core:page_open", function(ev)
    show(ev.player)
end)

core.event.on("core:page_action", function(ev)
    if core.has_perm(ev.player, "moderator") ~= 1 then
        return
    end
    local list = notes()
    if ev.id == "add" and ev.text ~= "" then
        table.insert(list, 1, `{string.sub(ev.text, 1, 100)} -- {core.player_name(ev.player)}`)
        while #list > 20 do
            table.remove(list)
        end
    else
        local n = tonumber(string.match(ev.id, "^del(%d+)$"))
        if n and list[n] then
            table.remove(list, n)
        end
    end
    keep(list)
    show(ev.player)
end)
