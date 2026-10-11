-- Gate helper: at $MS_CHECK_AT seconds, writes to $MS_CHECK_OUT whether the character RAM
-- holds the CP866 upper half (BIOS table at F000:B000) and saves a snapshot.
local at = tonumber(os.getenv('MS_CHECK_AT') or '30')
local out = os.getenv('MS_CHECK_OUT') or 'cp866.txt'
local done = false
SUB = emu.add_machine_frame_notifier(function()
    if done or manager.machine.time.seconds < at then return end
    done = true
    local font = manager.machine.memory.regions[':lcdc:lcdc:gfx1']
    local bios = manager.machine.memory.regions[':bios']
    local same = true
    for i = 0, 0x3ff do
        if font:read_u8(0x400 + i) ~= bios:read_u8(0xb000 + i) then same = false break end
    end
    local f = io.open(out, 'w'); f:write(same and 'CP866\n' or 'OTHER\n'); f:close()
    for _, s in pairs(manager.machine.screens) do s:snapshot('check.png') break end
end)
