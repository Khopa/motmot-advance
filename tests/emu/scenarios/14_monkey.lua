-- @fresh
-- @timeout 300
-- Random button mashing for 30000 frames: the game must never freeze (its
-- frame counter keeps moving) nor run code outside ROM / IWRAM / BIOS.
T.run(function()
  T.wait(20)
  local keys = { T.K.A, T.K.A, T.K.A, T.K.B, T.K.START, T.K.START, T.K.SELECT,
                 T.K.LEFT, T.K.RIGHT, T.K.UP, T.K.DOWN, T.K.A, T.K.START }
  math.randomseed(20260912)
  local held, holdleft = nil, 0
  local last, stall, worst_stall, bad_pc = T.frames(), 0, 0, nil
  local screens = {}
  for n = 1, 30000 do
    if holdleft == 0 then
      if held then emu:clearKey(held); held = nil end
      if math.random() < 0.6 then
        held = keys[math.random(#keys)]; emu:addKey(held); holdleft = math.random(1, 6)
      else
        holdleft = math.random(1, 10)
      end
    end
    holdleft = holdleft - 1
    coroutine.yield()
    local f = T.frames()
    if f == last then stall = stall + 1 else stall = 0 end
    if stall > worst_stall then worst_stall = stall end
    last = f
    local pc = emu:readRegister("pc")
    local ok = (pc >= 0x08000000 and pc < 0x0A000000) or (pc >= 0x03000000 and pc < 0x03008000) or pc < 0x4000
    if not ok and not bad_pc then bad_pc = pc end
    screens[T.screen()] = true
    if n % 10000 == 0 then T.log(string.format("frame %d: game frames %d, screen %d", n, f, T.screen())) end
  end
  if held then emu:clearKey(held) end
  T.check(worst_stall < 200, "frame counter never stalled (worst stall " .. worst_stall .. " frames)")
  T.check(bad_pc == nil, "PC always in ROM/IWRAM/BIOS" .. (bad_pc and string.format(" (saw %08x)", bad_pc) or ""))
  local count = 0
  for _ in pairs(screens) do count = count + 1 end
  T.check(count >= 4, "random input reached several screens (" .. count .. ")")
  T.shot("end")
end)
