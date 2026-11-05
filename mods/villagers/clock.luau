--!strict
-- villagers: the clock: everybody's turn, every tick. Part of the mod (init.luau says the order).

local common = require("./common")
local age = common.age
local AID = common.AID
local bed_keepers = common.bed_keepers
local beds_looked = common.beds_looked
local come_out = common.come_out
local hunted_gone = common.hunted_gone
local id = common.id
local JOB = common.JOB
local look_for_homes = common.look_for_homes
local muster = common.muster
local near_player = common.near_player
local notice_theft = common.notice_theft
local order_post = common.order_post
local people = common.people
local save_homes = common.save_homes
local show_builder = common.show_builder
local show_mayor = common.show_mayor
local show_trade = common.show_trade
local show_curator = common.show_curator
local step = common.step
local survey = common.survey
local TRADES = common.TRADES
local trust_of = common.trust_of
local village_at = common.village_at
local watch = common.watch

-- ---------------------------------------------------------------- the clock

core.event.on("core:world_ready", function()
    common.age_at_start = tonumber(core.storage_get("age")) or 0
    survey()
end)

-- When the clock next ages the villages, looks round them and moves everybody
-- (one table and not three locals: a main chunk has 200).
local due = { looked = 0.0, step = 0.0, aged = 0.0 }
core.event.on("core:tick", function(ev)
    common.clock += ev.dt
    if common.clock >= due.aged then
        due.aged = common.clock + 30
        core.storage_set("age", tostring(math.floor(age())))
    end
    if common.clock >= due.looked then
        due.looked = common.clock + 4
        look_for_homes()
        local gone = false
        for key, v in people do
            -- A bed broken is a home gone, and its inhabitant with it.
            if core.loaded(v.hx, v.hy, v.hz) == 1 and core.get_block(v.hx, v.hy, v.hz) ~= id("bed:bed") then
                if v.id ~= 0 then
                    core.mob_remove(v.id)
                end
                people[key] = nil
                gone = true
            elseif v.id == 0 and common.clock >= v.back_at and core.loaded(v.hx, v.hy, v.hz) == 1 and near_player(v, 64) then
                come_out(v)
            end
        end
        if gone then
            save_homes()
        end
        survey()
        muster()
        for rk in common.villages do
            if common.clock >= (beds_looked[rk] or 0) then
                beds_looked[rk] = common.clock + 20
                for mark in bed_keepers(rk) do
                    order_post(rk, mark)
                end
            end
        end
        for key, v in watch do
            if common.villages[v.rk :: string] == nil then
                if v.id ~= 0 then
                    core.mob_remove(v.id)
                end
                watch[key] = nil
            elseif v.id == 0 and common.clock >= v.back_at and core.loaded(v.hx, v.hy, v.hz) == 1 and near_player(v, 64) then
                come_out(v)
            end
        end
    end
    if common.clock < due.step then
        return
    end
    due.step = common.clock + 0.2
    notice_theft()
    hunted_gone()
    for _, v in people do
        step(v)
    end
    for _, v in watch do
        step(v)
    end
end)

-- What one of them is up to, for whoever asks: right click.
core.event.on("core:used_mob", function(ev)
    for _, v in people do
        if v.id == ev.target then
            local things = {}
            for item, n in v.inv do
                table.insert(things, `{n} {(string.gsub(string.gsub(item, "^.*:", ""), "_", " "))}`)
            end
            table.sort(things)
            local what = { farmer = "a farmer", fisher = "a fisher", forager = "a forager",
                           woodcutter = "a woodcutter", miner = "a miner", digger = "a digger" }
            local x, _, z = core.player_pos(ev.player)
            local rk = if x ~= nil and z ~= nil then village_at(x, z) else nil
            if rk ~= nil and trust_of(rk, core.player_name(ev.player)).strikes > 0 then
                core.notice(ev.player, `The {v.kind} turns away from you: the village has not forgotten.`)
                return
            end
            core.notice(ev.player, `A {v.kind}, {what[JOB[v.kind]]}` ..
                (if #things > 0 then `, carrying {table.concat(things, ", ")}` else ""))
            if x ~= nil and z ~= nil and math.random() < 1 / 3 then
                local s = AID.pt.near(x, z, 600, ev.player)[1]
                if s then core.notice(ev.player, `"{AID.pt.rumour(s, x, z, "here")}"`) end
            end
            local home = village_at(v.hx, v.hz)
            if home ~= nil and TRADES[JOB[v.kind]] ~= nil then
                show_trade(ev.player, home, JOB[v.kind])
            end
            return
        end
    end
    for _, v in watch do
        if v.id == ev.target and (v.kind == "mayor" or v.kind == "builder" or v.kind == "merchant"
            or v.kind == "curator") then
            local x, _, z = core.player_pos(ev.player)
            local rk = if x ~= nil and z ~= nil then village_at(x, z) else nil
            if rk ~= nil and trust_of(rk, core.player_name(ev.player)).strikes > 0 then
                core.notice(ev.player, `The {v.kind} turns away from you: the village has not forgotten.`)
            elseif v.kind == "mayor" then
                show_mayor(ev.player, v.rk :: string)
            elseif v.kind == "merchant" then
                show_trade(ev.player, v.rk :: string, "merchant")
            elseif v.kind == "curator" then
                show_curator(ev.player, v.rk :: string)
            else
                show_builder(ev.player, v.rk :: string)
            end
            return
        end
        if v.id == ev.target then
            local what = { guard = "A guard of the village", archer = "An archer of the village",
                           inspector = "The village's inspector, who looks in the bags of passers-by",
                           crafter = "The village's crafter, who makes the tools and torches its people use" }
            core.notice(ev.player, what[v.kind])
            return
        end
    end
end)
