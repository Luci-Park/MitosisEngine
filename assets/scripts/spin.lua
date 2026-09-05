local Spin = {}
local speed = 1.0

function Spin.OnUpdate(self, world, entity, dt)
    self.angle = (self.angle or 0.0) + speed * dt
    local half = self.angle * 0.5
    world:set(entity, "Transform", "rotation", { w = math.cos(half), x = 0.0, y = math.sin(half), z = 0.0 })
end

return Spin
