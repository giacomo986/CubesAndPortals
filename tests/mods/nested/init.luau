--!strict
-- A handler may emit. The inner event has fewer fields than the outer one, and
-- the outer handler writes its string field AFTER the inner emit has run --
-- which is the order that once had the inner emit re-size the bus's one
-- string buffer under the outer, so the outer indexed past its end and handed
-- back a pointer into a string that was gone. script_isolation_test emits
-- nested:outer and reads `text` back.
core.event.define("nested:outer", { fields = { n = "number", text = "string" }, cancellable = false })
core.event.define("nested:inner", { fields = { n = "number" }, cancellable = false })

core.event.on("nested:inner", function(ev) end)
core.event.on("nested:outer", function(ev)
    core.event.emit("nested:inner", { n = ev.n })
    ev.text = "written after the inner emit"
end)

core.log("nested: loaded")
