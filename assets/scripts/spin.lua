local Spin = {}
local angle = 0.0
local speed = 1.0

function Spin.OnUpdate(world, entity, dt)
    angle = angle + speed * dt
    local half = angle * 0.5
    world:set(entity, "Transform", "rotation", { w = math.cos(half), x = 0.0, y = math.sin(half), z = 0.0 })
end

return Spin
