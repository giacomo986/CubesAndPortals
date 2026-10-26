--!strict
-- The order assertion lives in the test: it checks that "timekeeper: loaded"
-- was logged before this line, which alphabetical order would have reversed.
core.log("dependent: loaded")

function on_step(dt: number)
end
