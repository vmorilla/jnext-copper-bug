-- MAME autoboot script: snapshot every SHOT_INTERVAL emulated frames, up to
-- SHOT_LAST, then exit. Used to take the mame.png hardware reference in each
-- case directory without depending on when F12 was pressed.
--
--   mame -snapsize 1440x1080 -nounevenstretch -video bgfx \
--        -bgfx_screen_chains unfiltered -window -skip_gameinfo \
--        -snapshot_directory snap -snapname 'boot%i' \
--        -autoboot_script shots.lua \
--        tbblue -hard1 ~/bin/next-images/cspect-next-2gb.img
--
-- MAME has to boot NextZXOS and load through .nexload first, so take several
-- snapshots and keep the first one in which the program is on screen. Two
-- consecutive snapshots being byte-identical is the signal that it has settled.
local interval = tonumber(os.getenv("SHOT_INTERVAL") or "200")
local last     = tonumber(os.getenv("SHOT_LAST") or "3000")
local n = 0

emu.add_machine_frame_notifier(function()
    n = n + 1
    if n % interval == 0 then
        manager.machine.video:snapshot()
        print(string.format("[shots.lua] snapshot at frame %d", n))
        io.stdout:flush()
    end
    if n >= last then
        print("[shots.lua] done, exiting")
        manager.machine:exit()
    end
end)
