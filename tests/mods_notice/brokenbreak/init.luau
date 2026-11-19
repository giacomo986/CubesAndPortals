--!strict
-- Switched off on purpose: tests/mod_notice.sh checks who is told.
core.event.on("core:broken", function(_ev)
    error("broken on purpose")
end)
