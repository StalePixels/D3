-- telnet-probe.lua: checks of .TELNET from inside MAME. run.sh gives it to
-- pinesy-harness in PINESY_HARNESS_PROBE for the telnet scenarios.
--
-- It watches the byte _tn_taken: .TELNET sets it to 1 with interrupts off
-- and nothing yet changed, and clears it when everything is back, just
-- before interrupts go back on. At each of the two moments the probe takes
-- the state of the machine: NextRegs, the tilemap clip window, the UART
-- ports, the ULA and tilemap palettes, all of bank 5, and I and IM. The
-- two must be the same.
--
-- While the session runs it checks the takeover: the registers, the
-- interrupt set-up, the font and the palette. It also logs every byte the
-- program writes to the UART and reads from it. Then, by
-- TELNET_PROBE_MODE:
--
--   echo     .TELNET to the harness echo server. When the link is open
--            (the ESP's '>'), types keys and waits for their echo on the
--            screen. Then it puts a block of data in the key queue with
--            the frame interrupt off; the block must come back whole, so
--            the Rx near full interrupt alone has to move it. Between
--            the two, EDIT opens the menu in the status bar and another
--            key closes it: the key is not sent, the terminal's cells do
--            not change, and keys typed after it go out. Then EDIT, Q.
--   banner   .TELNET to the harness banner server, which sends telnet
--            commands, text, and DECCKM on. The text must show, and the
--            commands must not. Then cursor up, and EDIT, Q.
--   badhost  .TELNET to a host that does not exist: the program must put
--            the machine back and give the error "DNS Fail".
--   pi       .TELNET -pi, with Layer 2 and sprites on before it starts.
--            Types keys, then EDIT, Q. The program must send exactly the
--            bytes of those keys.
--
-- In every mode that opens a session it checks the status bar on display
-- row 28: the connection and "EDIT menu".
--
-- In echo and banner modes it writes telnet-server.hex: the bytes the
-- server must have got, in hex. run.sh checks the server log against it.
--
-- TELNET_MAP is the map file of the build (DEV-D3/tmp/TELNET.map), for
-- the addresses. The results go to telnet-probe.txt in the run folder,
-- one line for each check with PASS or FAIL, and DONE at the end.

local pinesy = ...

local cpu = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local iosp = cpu.spaces["io"]
local b5 = manager.machine.memory.shares[":bram_bank5"]

local out = io.open("telnet-probe.txt", "w")
local function log(fmt, ...)
  out:write(string.format(fmt, ...), "\n")
  out:flush()
end
local function check(name, ok, detail)
  log("%s %s%s", ok and "PASS" or "FAIL", name, detail and ("  " .. detail) or "")
end

local mode = os.getenv("TELNET_PROBE_MODE") or "echo"

-- Addresses from the map file
local sym = {}
local map = os.getenv("TELNET_MAP")
local mf = map and io.open(map)
if not mf then
  log("FAIL no map file (TELNET_MAP=%s)", tostring(map))
  log("DONE")
  return
end
for line in mf:lines() do
  local name, addr = line:match("^(%S+)%s+= %$(%x+) ; addr")
  if name then sym[name] = tonumber(addr, 16) end
end
mf:close()
for _, s in ipairs({ "_tn_taken", "_tn_irq_save", "irq_frame", "irq_uart",
                     "keyq_buf", "keyq_head", "keyq_tail", "_tn_rx_overflow", "_tn_esp_error", "_tn_quit", "_tn_frames" }) do
  if not sym[s] then
    log("FAIL no symbol %s in %s", s, map)
    log("DONE")
    return
  end
end

-- NextRegs, keeping the select port as the program left it
local function nr(reg)
  local sel = iosp:read_u8(0x243b)
  iosp:write_u8(0x243b, reg)
  local v = iosp:read_u8(0x253b)
  iosp:write_u8(0x243b, sel)
  return v
end
local function nrw(reg, v)
  local sel = iosp:read_u8(0x243b)
  iosp:write_u8(0x243b, reg)
  iosp:write_u8(0x253b, v)
  iosp:write_u8(0x243b, sel)
end

local function cpureg(name)
  local s = cpu.state[name]
  return s and s.value
end

local REGS = { 0x07, 0x11, 0x14, 0x15, 0x2F, 0x30, 0x31, 0x40, 0x43, 0x4A, 0x4C, 0x56, 0x57,
               0x68, 0x69, 0x6B, 0x6C, 0x6E, 0x6F, 0xA0, 0xC0, 0xC4, 0xC5, 0xC6 }

-- 512 bytes of palette: the 8-bit value, then $44's bit 0, for each entry
local function palette(select)
  local p43, p40 = nr(0x43), nr(0x40)
  local t = {}
  nrw(0x43, (p43 & 0x0F) | select)
  for i = 0, 255 do
    nrw(0x40, i)
    t[#t + 1] = string.char(nr(0x41), nr(0x44))
  end
  nrw(0x43, p43)
  nrw(0x40, p40)
  return table.concat(t)
end

local function clip()
  local t = {}
  nrw(0x1C, 0x08)
  for i = 1, 4 do
    t[i] = nr(0x1B)
    nrw(0x1B, t[i])
  end
  return t
end

local function snapshot()
  local s = { regs = {}, clip = clip() }
  for _, r in ipairs(REGS) do s.regs[r] = nr(r) end
  s.regs[0x07] = s.regs[0x07] & 0x03          -- bits 5-4: the speed the CPU runs at now
  s.uart_select = iosp:read_u8(0x153b)
  s.uart_frame = iosp:read_u8(0x163b)
  s.ula_pal = palette(0x00)
  s.tm_pal = palette(0x30)
  local b = {}
  for i = 0, 0x3FFF do b[#b + 1] = string.char(b5:read_u8(i)) end
  s.bank5 = table.concat(b)
  -- The ESP error, while the command's memory is still paged in
  local t = {}
  for i = 0, 31 do
    local c = prog:read_u8(sym._tn_esp_error + i)
    t[#t + 1] = string.char(c & 0x7F)
    if c >= 0x80 or c == 0 then break end
  end
  s.esp_error = table.concat(t)
  s.i = cpureg("I")
  s.im = cpureg("IM")
  return s
end

local function first_diff(a, b)
  for i = 1, math.max(#a, #b) do
    if a:byte(i) ~= b:byte(i) then
      return string.format("offset %d: %s -> %s", i - 1,
        tostring(a:byte(i)), tostring(b:byte(i)))
    end
  end
end

local function compare(before, after)
  local bad = {}
  for _, r in ipairs(REGS) do
    if before.regs[r] ~= after.regs[r] then
      bad[#bad + 1] = string.format("$%02X %02X->%02X", r, before.regs[r], after.regs[r])
    end
  end
  check("NextRegs put back", #bad == 0, #bad > 0 and table.concat(bad, " ") or nil)
  local cb, ca = table.concat(before.clip, ","), table.concat(after.clip, ",")
  check("tilemap clip put back", cb == ca, cb .. " -> " .. ca)
  check("UART select put back", before.uart_select == after.uart_select,
    string.format("%02X -> %02X", before.uart_select, after.uart_select))
  check("UART frame put back", before.uart_frame == after.uart_frame,
    string.format("%02X -> %02X", before.uart_frame, after.uart_frame))
  check("ULA palette put back", before.ula_pal == after.ula_pal, first_diff(before.ula_pal, after.ula_pal))
  check("tilemap palette put back", before.tm_pal == after.tm_pal, first_diff(before.tm_pal, after.tm_pal))
  check("bank 5 put back", before.bank5 == after.bank5, first_diff(before.bank5, after.bank5))
  check("I put back", before.i == after.i, string.format("%s -> %s", tostring(before.i), tostring(after.i)))
  check("IM put back", before.im == after.im, string.format("%s -> %s", tostring(before.im), tostring(after.im)))
end

-- The session, while it runs

local function in_session()
  local function reg(r, want, mask, name)
    local v = nr(r) & (mask or 0xFF)
    check(string.format("%s ($%02X)", name, r), v == want, string.format("%02X, want %02X", v, want))
  end
  reg(0x07, 0x03, 0x03, "28 MHz")
  reg(0x68, 0x80, 0x90, "ULA off, extra keys in the matrix")
  reg(0x15, 0x00, 0x01, "sprites off")
  reg(0x69, 0x00, 0x80, "Layer 2 off")
  reg(0x6B, 0xCA, nil, "tilemap 80x32 text, 512 tiles")
  reg(0x6E, 0x10, nil, "map at $5000")
  reg(0x6F, 0x00, nil, "tiles at $4000")
  reg(0xC4, 0x01, nil, "frame interrupt only")
  reg(0xC5, 0x00, nil, "no CTC interrupts")
  reg(0xC6, mode == "pi" and 0x20 or 0x02, nil, "UART Rx near full interrupt")
  -- The table the hardware reads: I is the high byte of its address,
  -- $C0 bits 7-5 the top of the low byte. Entry 11 is the frame
  -- interrupt, entries 1 and 2 the UARTs' Rx.
  local c0 = nr(0xC0)
  local table = (cpureg("I") << 8) | (c0 & 0xE0)
  check("hardware IM2 mode", (c0 & 0x01) == 1, string.format("$C0=%02X", c0))
  check("vector table holds the frame and UART handlers",
    prog:read_u16(table + 22) == sym.irq_frame and prog:read_u16(table + 2) == sym.irq_uart
      and prog:read_u16(table + 4) == sym.irq_uart,
    string.format("table at %04X: frame %04X, UART %04X %04X", table,
      prog:read_u16(table + 22), prog:read_u16(table + 2), prog:read_u16(table + 4)))
  check("IM 2", cpureg("IM") == 2, tostring(cpureg("IM")))
  local sel = iosp:read_u8(0x153b)
  check("UART selected", (sel & 0x40) == (mode == "pi" and 0x40 or 0), string.format("$153B=%02X", sel))
  if mode == "pi" then reg(0xA0, 0x30, 0x30, "Pi UART on GPIO 14, 15") end

  -- tile 256 + n is tile n inverted, and the font has glyphs
  local inv_ok, glyph = true, 0
  for i = 0, 2047 do
    local t = b5:read_u8(i)
    if b5:read_u8(2048 + i) ~= (~t & 0xFF) then inv_ok = false end
    if i >= 65 * 8 and i < 66 * 8 then glyph = glyph | t end
  end
  check("tiles 256-511 are 0-255 inverted", inv_ok)
  check("font has a glyph for A", glyph ~= 0)

  -- attribute $0E: paper black (entry 14), ink white 5,5,5 (entry 15)
  local p = palette(0x30)
  local e14 = { p:byte(29, 30) }
  local e15 = { p:byte(31, 32) }
  check("palette: default paper black", e14[1] == 0 and (e14[2] & 1) == 0,
    string.format("%02X %02X", e14[1], e14[2]))
  check("palette: default ink white", e15[1] == 0xB6 and (e15[2] & 1) == 1,
    string.format("%02X %02X", e15[1], e15[2]))
end


-- The screen: rows 0-31 of the map, character bytes
local function screen_stream()
  local t = {}
  for cell = 0, 32 * 80 - 1 do t[#t + 1] = string.char(b5:read_u8(0x1000 + cell * 2)) end
  return table.concat(t)
end

local function printable(s)
  return (s:gsub("[^%g ]", "."):gsub("%s+$", ""))
end

local function hex(s)
  return (s:gsub(".", function(c) return string.format("%02x ", c:byte()) end):gsub(" $", ""))
end

-- Put bytes in the key queue, as far as there is room. Returns what is left.
local function keyq_push(s)
  local head = prog:read_u8(sym.keyq_head)
  local tail = prog:read_u8(sym.keyq_tail)
  local room = (tail - head - 1) & 0xFF
  local n = math.min(room, #s)
  for i = 1, n do
    prog:write_u8(sym.keyq_buf + ((head + i - 1) & 0xFF), s:byte(i))
  end
  prog:write_u8(sym.keyq_head, (head + n) & 0xFF)
  return s:sub(n + 1)
end

-- A block for the echo server: 1000 letters and digits
local block
do
  local chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
  local t = {}
  for i = 0, 999 do
    local k = (i * 7) % #chars + 1
    t[#t + 1] = chars:sub(k, k)
  end
  block = table.concat(t)
end

-- The keys the echo and pi modes type, and the bytes they must give.
-- Capital H (CAPS SHIFT), i, ! (SYMBOL SHIFT + 1), cursor up, BREAK (ESC),
-- EXTEND + C (3), EXTEND + B (2), a, b, DELETE, ENTER.
local ECHO_KEYS = "Hi!{UP}{BREAK}{EXTC}{EXTB}ab{DELETE}{ENTER}"
local ECHO_BYTES = "Hi!\27[A\27\3\2ab\8\r\n"
-- Typed after the menu has closed
local AFTER_MENU = "zz9"
local PI_KEYS = "a{EXTB}{BREAK}{ENTER}"
local PI_BYTES = "a\2\27\r"
local LEAVE = "{EDIT}q"

-- The status bar: display row 28, the map row NextReg $31 puts there
local STATUS_ROW = 28
local function status_map_row()
  return ((nr(0x31) >> 3) + STATUS_ROW) & 31
end
local function status_text()
  local row, t = status_map_row(), {}
  for c = 0, 79 do t[#t + 1] = string.char(b5:read_u8(0x1000 + (row * 80 + c) * 2)) end
  return table.concat(t)
end

-- The cells of every map row but the status bar's, without the cursor's
-- reverse bit
local function terminal_cells()
  local skip, t = status_map_row(), {}
  for row = 0, 31 do
    if row ~= skip then
      for c = 0, 79 do
        local off = 0x1000 + (row * 80 + c) * 2
        t[#t + 1] = string.char(b5:read_u8(off), b5:read_u8(off + 1) & 0xFE)
      end
    end
  end
  return table.concat(t)
end
local cells_before_menu

local BANNER_TEXT = "BANNER-START\255BANNER-END"
local BANNER_ANSWERS = "\255\252\24\255\253\1"    -- WONT TTYPE, DO ECHO (once)
local BANNER_UP = "\27OA"                         -- cursor up with DECCKM on

local function write_server_hex(s)
  local f = io.open("telnet-server.hex", "w")
  f:write(hex(s), "\n")
  f:close()
end

-- What the program wrote to the UART and read from it, while taken
local tx, rx = {}, {}
local function tx_text() return table.concat(tx) end
local function rx_text() return table.concat(rx) end

-- State

local before, after
local taken_at, after_at
local session_checked = false
local finished = false

-- The steps of a mode, one after the other. A step runs its "start" (which
-- can give bytes to put in the key queue as room comes), waits for its
-- "done" test, at most "wait" seconds, then runs its "finish".
local steps
local step, step_t = 0, 0

local function link_open()
  return rx_text():find("OK\r\n\r\n>", 1, true)
end

if mode == "echo" then
  write_server_hex(ECHO_BYTES .. AFTER_MENU .. block)
  steps = {
    { name = "link open (the ESP's '>')", wait = 10, done = link_open },
    { name = "typed keys echo on the screen", wait = 10,
      start = function() pinesy.type(ECHO_KEYS) end,
      done = function() return not pinesy.typing() and screen_stream():find("Hi!", 1, true) end },
    { name = "EDIT shows the menu in the status bar", wait = 3,
      start = function() cells_before_menu = terminal_cells(); pinesy.type("{EDIT}") end,
      done = function() return not pinesy.typing() and status_text():find("Q: leave", 1, true) end },
    { name = "another key closes the menu", wait = 3, start = function() pinesy.type("x") end,
      done = function() return not pinesy.typing() and status_text():find("EDIT menu", 1, true) end },
    { name = "the terminal's cells are as before the menu", wait = 1,
      done = function() return terminal_cells() == cells_before_menu end },
    { name = "keys typed after the menu go out", wait = 10,
      start = function() pinesy.type(AFTER_MENU) end,
      done = function() return not pinesy.typing() and screen_stream():find(AFTER_MENU, 1, true) end },
    -- With the frame interrupt off (NextReg $C4 = 0), only the Rx near
    -- full interrupt empties the UART, when its 512-byte buffer is 3/4
    -- full. 700 bytes need it twice. The rest of the block stays in the
    -- UART until the frame interrupt is back on.
    { name = "Rx near full interrupt alone moves the echo of a block", wait = 8,
      start = function() nrw(0xC4, 0x00); return block end,
      finish = function() nrw(0xC4, 0x01) end,
      done = function() return rx_text():find(block:sub(1, 700), 1, true) end },
    { name = "the whole block comes back", wait = 4,
      done = function() return rx_text():find(block, 1, true) end },
    { name = "EDIT, Q", wait = 1, start = function() pinesy.type(LEAVE) end,
      done = function() return true end },
  }
elseif mode == "banner" then
  write_server_hex(BANNER_ANSWERS .. BANNER_UP)
  steps = {
    { name = "banner text on the screen", wait = 10,
      done = function() return screen_stream():find(BANNER_TEXT, 1, true) end },
    { name = "no telnet commands on the screen", wait = 1,
      done = function() return not screen_stream():find("[\240\250\251\252\253\254]") end },
    { name = "cursor up, a second later", wait = 2,
      done = function(t) return t >= step_t + 1 end,
      finish = function() pinesy.type("{UP}") end },
    { name = "EDIT, Q", wait = 2, done = function() return not pinesy.typing() end,
      finish = function() pinesy.type(LEAVE) end },
  }
elseif mode == "pi" then
  steps = {
    { name = "keys typed", wait = 5, start = function() pinesy.type(PI_KEYS) end,
      done = function() return not pinesy.typing() end },
    { name = "EDIT, Q", wait = 1, start = function() pinesy.type(LEAVE) end,
      done = function() return true end },
  }
else
  steps = {}
end

probe_taps = {
  prog:install_write_tap(sym._tn_taken, sym._tn_taken, "telnet_taken",
    function(offset, value, mask)
      -- only the writes in irq.asm, not BASIC's or the loader's
      local pc = cpureg("PC")
      if not pc or pc < sym._tn_irq_save or pc >= sym.irq_frame then return end
      value = value & 0xFF
      local t = manager.machine.time:as_double()
      if value == 1 and not before then
        before = snapshot()
        taken_at = t
        log("taken at %.2f", t)
      elseif value == 0 and before and not after then
        after = snapshot()
        after_at = t
        log("put back at %.2f", t)
      end
    end),
  iosp:install_write_tap(0x133b, 0x133b, "telnet_tx",
    function(offset, data, mask)
      if before and not after then tx[#tx + 1] = string.char(data & 0xFF) end
    end),
  iosp:install_read_tap(0x143b, 0x143b, "telnet_rx",
    function(offset, data, mask)
      if before and not after then rx[#rx + 1] = string.char(data & 0xFF) end
    end),
}

local function at_end()
  compare(before, after)
  check("interrupts on after the exit", cpureg("IFF1") == 1, tostring(cpureg("IFF1")))
  check("every step ran", #steps == 0 or step > #steps, string.format("stopped at step %d", step))
  log("TX %s", printable(tx_text()))
  if mode == "echo" or mode == "banner" then
    local t = tx_text()
    local plus = t:find("+++", 1, true)
    check("leave: +++, AT+CIPMODE=0, AT+CIPCLOSE",
      plus and t:find("AT+CIPMODE=0\r\n", plus, true) and t:find("AT+CIPCLOSE\r\n", plus, true))
    check("no bytes lost from the ring", prog:read_u8(sym._tn_rx_overflow) == 0)
  elseif mode == "pi" then
    check("-pi sends the keys' bytes", tx_text() == PI_BYTES, hex(tx_text()))
    check("Layer 2 was on before", (before.regs[0x69] & 0x80) ~= 0, string.format("$69=%02X", before.regs[0x69]))
    check("sprites were on before", (before.regs[0x15] & 0x01) ~= 0, string.format("$15=%02X", before.regs[0x15]))
  elseif mode == "badhost" then
    check("error is the ESP's message", after.esp_error == "DNS Fail", printable(after.esp_error))
  end
  log("DONE")
  finished = true
end

probe_sub = emu.add_machine_frame_notifier(function()
  if finished then return end
  local t = manager.machine.time:as_double()

  if after then
    if t >= after_at + 1 then at_end() end
    return
  end

  if not taken_at then return end

  if not session_checked and t >= taken_at + 1.5 then
    session_checked = true
    in_session()
    if mode ~= "badhost" then
      local st = status_text()
      local want = mode == "pi" and " Pi UART" or " 127.0.0.1 port 7777"
      check("status bar on display row 28: the connection and EDIT menu",
        st:sub(1, #want) == want and st:sub(-10) == "EDIT menu ", printable(st))
    end
    step, step_t = 1, t
    return
  end

  local s = steps[step]
  if not s then return end
  if s.start and not s.started then
    s.started = true
    s.pending = s.start()
  end
  if s.pending then s.pending = keyq_push(s.pending); if #s.pending == 0 then s.pending = nil end end
  local ok = s.done(t)
  if ok or t > step_t + s.wait then
    if s.finish then s.finish() end
    check(s.name, ok and true or false, (not ok) and ("screen: " .. printable(screen_stream())) or nil)
    if ok then
      step = step + 1
      step_t = t
    else
      step = #steps + 1
      pinesy.type(LEAVE)
    end
  end
end)

-- A run that ends with the session still on
probe_stop = emu.add_machine_stop_notifier(function()
  if finished then return end
  check("the session ended", false,
    string.format("step %d, TX %s, RX %s, tn_quit %d, tn_frames %d, key queue head %d tail %d",
      step, printable(tx_text()), printable(rx_text()), prog:read_u8(sym._tn_quit),
      prog:read_u16(sym._tn_frames), prog:read_u8(sym.keyq_head), prog:read_u8(sym.keyq_tail)))
  log("PC %04X SP %04X IFF1 %s IM %s I %s $C0 %02X $C4 %02X $C8 %02X MMU4-7 %02X %02X %02X %02X",
    cpureg("PC"), cpureg("SP"), tostring(cpureg("IFF1")), tostring(cpureg("IM")), tostring(cpureg("I")),
    nr(0xC0), nr(0xC4), nr(0xC8), nr(0x54), nr(0x55), nr(0x56), nr(0x57))
  log("DONE")
end)
