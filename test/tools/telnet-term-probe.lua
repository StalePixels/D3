-- telnet-term-probe.lua: checks of the .TELNET terminal from inside MAME.
-- run.sh gives it to pinesy-harness in PINESY_HARNESS_PROBE for the
-- telnet-term scenarios.
--
-- .TELNET runs with -pi, where nothing arrives on the UART. The probe
-- writes byte streams straight into the UART receive ring, as the
-- interrupts would, and waits for the main loop to pass them to the
-- terminal. Then it reads the tilemap and the terminal's variables. Each
-- test starts with ESC c, which resets the terminal.
--
-- With TELNET_TERM_MODE=speed it measures how fast the terminal takes
-- bytes instead, and feeds a long stream at the rate of 115,200 baud.
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

local mode = os.getenv("TELNET_TERM_MODE") or "functions"

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
for _, s in ipairs({ "_tn_taken", "rx_head", "rx_tail", "_tn_rx_overflow",
                     "keyq_buf", "keyq_head", "term_state", "st_ground", "term_base",
                     "cur_row", "cur_col", "cur_attr", "wrap_pending", "cursor_shown",
                     "cursor_cell", "margin_top", "margin_bot", "_tn_term_ckm",
                     "_tn_term_lnm", "mode_tcem" }) do
  if not sym[s] then
    log("FAIL no symbol %s in %s", s, map)
    log("DONE")
    return
  end
end

local RING, RING_END = 0xE000, 0x10000
local MAP = 0x1000                      -- the tilemap, as an offset in bank 5
local ESC = "\27"
local CSI = ESC .. "["

local function rd(a) return prog:read_u8(a) end
local function rdw(a) return prog:read_u8(a) | (prog:read_u8(a + 1) << 8) end
local function cpureg(name)
  local s = cpu.state[name]
  return s and s.value
end

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

-- Tilemap palette entry i: the 8-bit value
local function palette_entry(i)
  local p43, p40 = nr(0x43), nr(0x40)
  nrw(0x43, (p43 & 0x0F) | 0x30)
  nrw(0x40, i)
  local v = nr(0x41)
  nrw(0x43, p43)
  nrw(0x40, p40)
  return v
end

-- ---------------------------------------------------------------- ring

local function ring_used()
  return (rdw(sym.rx_head) - rdw(sym.rx_tail)) & 0x1FFF
end

-- Write as much of s into the ring as fits. Only between instructions of
-- the main program (interrupts on), never inside the interrupt, which
-- writes rx_head too. Returns what is left of s.
local function ring_write(s)
  if cpureg("IFF1") ~= 1 then return s end
  local room = 0x1FFF - ring_used()
  local n = math.min(room, #s)
  local head = rdw(sym.rx_head)
  for i = 1, n do
    prog:write_u8(head, s:byte(i))
    head = head + 1
    if head == RING_END then head = RING end
  end
  prog:write_u8(sym.rx_head, head & 0xFF)
  prog:write_u8(sym.rx_head + 1, head >> 8)
  return s:sub(n + 1)
end

-- ---------------------------------------------------------------- screen

-- The terminal: 24 rows, on display rows 4 to 27, and the status bar on
-- display row 28 (term.asm)
local ROWS, TOP = 24, 4
local LAST = ROWS - 1
local STATUS_ROW = TOP + ROWS

-- The text on display row 28: the map row NextReg $31 puts there
local function status_text()
  local row, t = ((nr(0x31) >> 3) + STATUS_ROW) & 31, {}
  for c = 0, 79 do t[#t + 1] = string.char(b5:read_u8(MAP + (row * 80 + c) * 2)) end
  return table.concat(t)
end

local function expect_status(name)
  local st = status_text()
  check(name, st:sub(1, 8) == " Pi UART" and st:sub(-10) == "EDIT menu ", string.format("%q", st))
end

-- Terminal row r (0 to LAST), column c (0-79): the character and the
-- attribute, without the cursor's reverse bit
local function cell(r, c)
  local row = (rd(sym.term_base) + r) & 31
  local off = MAP + (row * 80 + c) * 2
  local ch, attr = b5:read_u8(off), b5:read_u8(off + 1)
  if rd(sym.cursor_shown) ~= 0 and rdw(sym.cursor_cell) == 0x4000 + off then
    attr = attr ~ 1
  end
  return ch, attr
end

local function row_text(r, from, to)
  local t = {}
  for c = from or 0, to or 79 do t[#t + 1] = string.char((cell(r, c))) end
  return table.concat(t)
end

local function show(s)
  return (s:gsub("[^%g ]", function(ch) return string.format("<%02X>", ch:byte()) end))
end

local function expect_text(name, r, c, want)
  local got = row_text(r, c, c + #want - 1)
  check(name, got == want, string.format("row %d col %d: %q, want %q", r, c, show(got), show(want)))
end

local function expect_cell(name, r, c, ch, attr)
  local gch, gattr = cell(r, c)
  local ok = (ch == nil or gch == ch) and (attr == nil or gattr == attr)
  check(name, ok, string.format("row %d col %d: %02X/%02X, want %s/%s", r, c, gch, gattr,
    ch and string.format("%02X", ch) or "--", attr and string.format("%02X", attr) or "--"))
end

local function expect_cursor(name, r, c)
  local gr, gc = rd(sym.cur_row), rd(sym.cur_col)
  check(name, gr == r and gc == c, string.format("at %d,%d, want %d,%d", gr, gc, r, c))
end

-- Every cell of rows from-to is a space with attribute attr
local function expect_blank(name, from, to, attr)
  for r = from, to do
    for c = 0, 79 do
      local ch, a = cell(r, c)
      if ch ~= 32 or a ~= attr then
        check(name, false, string.format("row %d col %d: %02X/%02X, want 20/%02X", r, c, ch, a, attr))
        return
      end
    end
  end
  check(name, true)
end

-- ---------------------------------------------------------------- tests

local co                                -- the test coroutine
local function frame() coroutine.yield() end

-- Send s to the terminal and wait until it has taken all of it, with a
-- frame more for the last byte. Returns the replies it put in the key
-- queue.
local function send(s)
  local kq = rd(sym.keyq_head)
  local deadline = manager.machine.time:as_double() + 10
  while #s > 0 do
    s = ring_write(s)
    frame()
  end
  while ring_used() > 0 or rdw(sym.term_state) ~= sym.st_ground do
    if manager.machine.time:as_double() > deadline then break end
    frame()
  end
  frame()
  frame()
  local t, kh = {}, rd(sym.keyq_head)
  while kq ~= kh do
    t[#t + 1] = string.char(rd(sym.keyq_buf + kq))
    kq = (kq + 1) & 0xFF
  end
  return table.concat(t)
end

local function reset(s) return send(ESC .. "c" .. (s or "")) end

local function tests()
  -- Text, and the cursor after it
  reset("Hello")
  expect_text("text", 0, 0, "Hello")
  expect_cell("default attribute", 0, 0, nil, 0x0E)
  expect_cursor("cursor after text", 0, 5)

  -- SGR. Attribute: bit 7 bold, 6-4 paper, 3-1 ink, 0 reverse.
  reset(CSI .. "1;31;44mR" .. CSI .. "0mN" .. CSI .. "7mV" .. CSI .. "27;32mG"
        .. CSI .. "39;49mD" .. CSI .. "1;33m" .. CSI .. "22mU"
        .. CSI .. "0;38;5;31mX" .. CSI .. "48;2;1;2;3;35mY" .. CSI .. "mZ")
  expect_cell("SGR 1;31;44 bold red on blue", 0, 0, 82, 0xC2)
  expect_cell("SGR 0 reset", 0, 1, 78, 0x0E)
  expect_cell("SGR 7 reverse", 0, 2, 86, 0x0F)
  expect_cell("SGR 27;32 not reverse, green", 0, 3, 71, 0x04)
  expect_cell("SGR 39;49 default colours", 0, 4, 68, 0x0E)
  expect_cell("SGR 22 not bold", 0, 5, 85, 0x06)
  expect_cell("SGR 38;5;n skipped", 0, 6, 88, 0x0E)
  expect_cell("SGR 48;2;r;g;b skipped, then 35", 0, 7, 89, 0x0A)
  expect_cell("SGR with no parameter", 0, 8, 90, 0x0E)

  -- Cursor moves
  reset(CSI .. "10;20HA" .. CSI .. "2AB" .. CSI .. "3CC" .. CSI .. "6DD" .. CSI .. "BE")
  expect_cell("CUP 10;20", 9, 19, 65)
  expect_cell("CUU 2", 7, 20, 66)
  expect_cell("CUF 3", 7, 24, 67)
  expect_cell("CUB 6", 7, 19, 68)
  expect_cell("CUD", 8, 20, 69)
  send(CSI .. "99A" .. CSI .. "99DF" .. CSI .. "99B" .. CSI .. "99CG" .. CSI .. "5;7fH")
  expect_cell("CUU and CUB stop at the edge", 0, 0, 70)
  expect_cell("CUD and CUF stop at the edge", LAST, 79, 71)
  expect_cell("HVP 5;7", 4, 6, 72)
  send(CSI .. "HI")
  expect_cell("CUP with no parameters", 0, 0, 73)

  -- Controls
  reset("ABC\8\8X\rY\tT\nL\11V\12F")
  expect_text("BS and CR", 0, 0, "YXC")
  expect_cell("HT to column 8", 0, 8, 84)
  expect_cell("LF keeps the column", 1, 9, 76)
  expect_cell("VT as LF", 2, 10, 86)
  expect_cell("FF as LF", 3, 11, 70)
  reset("A\7\0\5B" .. ESC .. "[3\24C" .. ESC .. "[1\26D")
  expect_text("BEL and others ignored, CAN and SUB end a sequence", 0, 0, "ABCD")

  -- Tab stops
  reset(CSI .. "1;5H" .. ESC .. "H\r\tA" .. CSI .. "3g" .. "\r\tB")
  expect_cell("HTS", 0, 4, 65)
  expect_cell("TBC 3: HT goes to the last column", 0, 79, 66)
  reset("\t" .. CSI .. "g\r\t\tC")
  expect_cell("TBC 0 clears one stop", 0, 24, 67)

  -- Erase
  local xs = string.rep("X", 10)
  local fill = CSI .. "1;1H" .. xs .. CSI .. "2;1H" .. xs .. CSI .. "3;1H" .. xs
  reset(fill .. CSI .. "2;5H" .. CSI .. "42m" .. CSI .. "K")
  expect_text("EL 0 keeps the left part", 1, 0, "XXXX")
  expect_cell("EL 0 erases with the paper", 1, 4, 32, 0x2E)
  expect_cell("EL 0 erases to the end", 1, 79, 32, 0x2E)
  send(CSI .. "7m" .. CSI .. "1;5H" .. CSI .. "1K")
  expect_cell("EL 1 erases to the cursor, reverse off", 0, 4, 32, 0x2E)
  expect_text("EL 1 keeps the right part", 0, 5, "XXXXX")
  send(CSI .. "3;3H" .. CSI .. "2K")
  expect_blank("EL 2", 2, 2, 0x2E)
  reset(fill .. CSI .. "2;3H" .. CSI .. "J")
  expect_text("ED 0 keeps what is before", 1, 0, "XX")
  expect_cell("ED 0 erases the rest of the line", 1, 2, 32)
  expect_blank("ED 0 erases the rows below", 2, LAST, 0x0E)
  expect_text("ED 0 keeps the rows above", 0, 0, xs)
  reset(fill .. CSI .. "2;3H" .. CSI .. "1J")
  expect_blank("ED 1 erases the rows above", 0, 0, 0x0E)
  expect_text("ED 1 erases to the cursor", 1, 0, "   XXXXXXX")
  expect_text("ED 1 keeps the rows below", 2, 0, xs)
  send(CSI .. "44m" .. CSI .. "2J")
  expect_blank("ED 2 erases all, with the paper", 0, LAST, 0x4E)
  expect_cursor("ED 2 does not move the cursor", 1, 2)

  -- Deferred wrap
  reset(CSI .. "1;80HAB")
  expect_cell("last column", 0, 79, 65)
  expect_cell("the next character wraps", 1, 0, 66)
  send(CSI .. "5;80HC")
  expect_cursor("wrap pending: the cursor stays", 4, 79)
  check("wrap pending set", rd(sym.wrap_pending) == 1)
  send(CSI .. "CD")
  expect_cell("a cursor move clears it", 4, 79, 68)
  expect_cell("no wrap after the move", 5, 0, 32)
  send(CSI .. "7;80HE\rF")
  expect_cell("CR clears it", 6, 0, 70)
  send(CSI .. "?7l" .. CSI .. "9;79HGHI")
  expect_text("DECAWM off: no wrap", 8, 78, "GI")
  expect_cell("DECAWM off: next line untouched", 9, 0, 32)
  reset(CSI .. ROWS .. ";80HJK")
  expect_cell("wrap at the last row scrolls", LAST - 1, 79, 74)
  expect_cell("and goes on in the new row", LAST, 0, 75)

  -- Full screen scroll: the ring and NextReg $31
  local lines = {}
  for i = 0, 34 do lines[#lines + 1] = string.format("L%02d\r\n", i) end
  reset(table.concat(lines))
  local scrolls = #lines - LAST
  local ok, bad = true, nil
  for r = 0, LAST - 1 do
    local want = string.format("L%02d", r + scrolls)
    if row_text(r, 0, 2) ~= want then ok, bad = false, string.format("row %d: %q", r, row_text(r, 0, 2)) end
  end
  check("full scroll: rows moved up", ok, bad)
  expect_blank("full scroll: new row blank", LAST, LAST, 0x0E)
  check("full scroll: ring moved", rd(sym.term_base) == TOP + scrolls, tostring(rd(sym.term_base)))
  check("full scroll: NextReg $31", nr(0x31) == ((rd(sym.term_base) - TOP) & 31) * 8,
    string.format("%02X", nr(0x31)))
  expect_status("full scroll: status bar on display row 28")
  send(CSI .. "46m\n")
  expect_blank("full scroll: new row with the paper", LAST, LAST, 0x6E)
  send(CSI .. "0m" .. CSI .. "H" .. ESC .. "M")
  expect_blank("RI at the top scrolls down", 0, 0, 0x0E)
  expect_text("RI: rows moved down", 1, 0, string.format("L%02d", scrolls + 1))
  expect_cursor("RI: cursor stays at the top", 0, 0)
  expect_status("RI scroll: status bar on display row 28")
  local clip = {}
  nrw(0x1C, 0x08)
  for i = 1, 4 do clip[i] = nr(0x1B); nrw(0x1B, clip[i]) end
  check("clip window shows the terminal rows and the status bar only",
    clip[3] == TOP * 8 and clip[4] == (STATUS_ROW + 1) * 8 - 1,
    table.concat(clip, ","))

  -- Scroll region
  local rows = {}
  for i = 0, 9 do rows[#rows + 1] = string.format("%s%d;1HR%d", CSI, i + 1, i) end
  reset(table.concat(rows) .. CSI .. "3;6r")
  expect_cursor("DECSTBM homes the cursor", 0, 0)
  send(CSI .. "6;1H\n")
  expect_text("region scroll: row above kept", 1, 0, "R1")
  expect_text("region scroll: rows moved up", 2, 0, "R3")
  expect_text("region scroll: rows moved up", 4, 0, "R5")
  expect_blank("region scroll: bottom row blank", 5, 5, 0x0E)
  expect_text("region scroll: row below kept", 6, 0, "R6")
  check("region scroll: ring not moved", rd(sym.term_base) == TOP, tostring(rd(sym.term_base)))
  send(CSI .. "3;1H" .. ESC .. "M")
  expect_blank("region RI: top row blank", 2, 2, 0x0E)
  expect_text("region RI: rows moved down", 3, 0, "R3")
  expect_text("region RI: rows moved down", 5, 0, "R5")
  expect_text("region RI: row below kept", 6, 0, "R6")
  send(CSI .. "5;1H" .. CSI .. "9A")
  expect_cursor("CUU stops at the top margin", 2, 0)
  send(CSI .. "9B")
  expect_cursor("CUD stops at the bottom margin", 5, 0)
  send(CSI .. "8;1H" .. CSI .. "9A")
  expect_cursor("CUU from below the region stops at the top margin", 2, 0)
  send(CSI .. "2;1H" .. CSI .. "9A")
  expect_cursor("CUU above the region goes to row 0", 0, 0)
  send(CSI .. ROWS .. ";1H\n")
  expect_text("LF at the last row, below the region: no scroll", 6, 0, "R6")
  local r = send(CSI .. "?6h" .. CSI .. "1;1HO" .. CSI .. "6n")
  expect_cell("DECOM: row 1 is the top margin", 2, 0, 79)
  check("DSR 6 in origin mode", r == CSI .. "1;2R", show(r))
  send(CSI .. "99;1HP")
  expect_cell("DECOM: CUP stops at the bottom margin", 5, 0, 80)
  send(CSI .. "?6l" .. CSI .. "r")
  check("DECSTBM with no parameters: whole screen",
    rd(sym.margin_top) == 0 and rd(sym.margin_bot) == LAST)

  -- Replies
  r = reset(CSI .. "5n")
  check("DSR 5", r == CSI .. "0n", show(r))
  r = send(CSI .. "10;20H" .. CSI .. "6n")
  check("DSR 6", r == CSI .. "10;20R", show(r))
  r = send(CSI .. "c")
  check("DA", r == CSI .. "?1;0c", show(r))
  r = send(CSI .. "0c")
  check("DA 0", r == CSI .. "?1;0c", show(r))

  -- Character sets
  reset(ESC .. "(0lqkx~" .. ESC .. "(B" .. "q" .. ESC .. ")0\14j\15j" .. ESC .. "(A#" .. ESC .. "(B#")
  expect_text("line drawing in G0", 0, 0, "\xDA\xC4\xBF\xB3\xFA")
  expect_cell("ESC ( B: ASCII", 0, 5, 113)
  expect_cell("SO: G1 line drawing", 0, 6, 0xD9)
  expect_cell("SI: back to G0", 0, 7, 106)
  expect_cell("UK: pound", 0, 8, 0x9C)
  expect_cell("ASCII #", 0, 9, 35)

  -- DECSC and DECRC
  reset(CSI .. "5;5H" .. CSI .. "31m" .. ESC .. "(0" .. ESC .. "7"
        .. CSI .. "1;1H" .. CSI .. "0m" .. ESC .. "(B" .. ESC .. "8q")
  expect_cell("DECRC: cursor, rendition and charset", 4, 4, 0xC4, 0x02)

  -- IND, NEL
  reset(CSI .. "3;4H" .. ESC .. "DA" .. ESC .. "EB")
  expect_cell("IND", 3, 3, 65)
  expect_cell("NEL", 4, 0, 66)

  -- Modes
  send(CSI .. "?1h")
  check("DECCKM set", rd(sym._tn_term_ckm) == 1)
  send(CSI .. "?1l")
  check("DECCKM reset", rd(sym._tn_term_ckm) == 0)
  reset(CSI .. "20hAB\nC")
  check("LNM set", rd(sym._tn_term_lnm) == 1)
  expect_cell("LNM: LF also returns", 1, 0, 67)
  send(CSI .. "20l")
  check("LNM reset", rd(sym._tn_term_lnm) == 0)

  -- DECSCNM: palette entries 14 (paper of $0E) and 15 (its ink) swap
  local p14, p15 = palette_entry(14), palette_entry(15)
  send(CSI .. "?5h")
  check("DECSCNM swaps ink and paper", palette_entry(14) == p15 and palette_entry(15) == p14,
    string.format("%02X %02X", palette_entry(14), palette_entry(15)))
  send(CSI .. "?5l")
  check("DECSCNM off puts them back", palette_entry(14) == p14 and palette_entry(15) == p15)

  -- Unknown sequences show nothing
  reset(CSI .. ">0cA" .. CSI .. "?99hB" .. ESC .. "#3C" .. ESC .. "(ZD" .. CSI .. "1:2mE"
        .. CSI .. "1 qF" .. ESC .. "ZG" .. CSI .. "99zH")
  expect_text("unknown sequences consumed", 0, 0, "ABCDEFGH ")

  -- DECALN
  reset(CSI .. "3;5r" .. ESC .. "#8")
  local all_e = true
  for rr = 0, LAST do if row_text(rr) ~= string.rep("E", 80) then all_e = false end end
  check("DECALN fills the screen with E", all_e)
  check("DECALN resets the margins", rd(sym.margin_top) == 0 and rd(sym.margin_bot) == LAST)

  -- The cursor: on after a quiet frame, blinking, on the cursor cell
  reset(CSI .. "12;34H")
  local seen_on, seen_off, right_cell = false, false, true
  local want = 0x4000 + MAP + (((rd(sym.term_base) + 11) & 31) * 80 + 33) * 2
  for _ = 1, 100 do
    frame()
    if rd(sym.cursor_shown) ~= 0 then
      seen_on = true
      if rdw(sym.cursor_cell) ~= want then right_cell = false end
      if (prog:read_u8(want + 1) & 1) == 0 then right_cell = false end
    else
      seen_off = true
    end
  end
  check("cursor blinks", seen_on and seen_off)
  check("cursor is the reverse bit of its cell", right_cell)
  send(CSI .. "?25l")
  local never = true
  for _ = 1, 100 do
    frame()
    if rd(sym.cursor_shown) ~= 0 then never = false end
  end
  check("DECTCEM off: no cursor", never)
  send(CSI .. "?25h")

  -- While text comes in the cursor does not show
  local shown = false
  local busy = string.rep("x", 40) .. "\r"
  for _ = 1, 50 do
    ring_write(busy)
    frame()
    if rd(sym.cursor_shown) ~= 0 then shown = true end
  end
  check("no cursor while text comes in", not shown)

  check("no bytes lost from the ring", rd(sym._tn_rx_overflow) == 0)
end

-- ---------------------------------------------------------------- speed

-- Frames the terminal takes to read the stream s, fed all at once
local function time_stream(s)
  send(ESC .. "c")
  local start = manager.machine.time:as_double()
  local frames = 0
  local left = s
  while #left > 0 or ring_used() > 0 do
    if #left > 0 then left = ring_write(left) end
    frame()
    frames = frames + 1
    if frames > 3000 then break end
  end
  return frames, manager.machine.time:as_double() - start
end

local function speed()
  -- A frame rate from the machine
  local t0 = manager.machine.time:as_double()
  for _ = 1, 50 do frame() end
  local fps = 50 / (manager.machine.time:as_double() - t0)
  local per_frame = math.floor(11520 / fps)  -- 115,200 baud, 10 bits a byte
  log("frame rate %.2f Hz: 115,200 baud is %d bytes a frame", fps, per_frame)

  -- Printable text that fills the screen, no scrolling
  local page = CSI .. "H" .. string.rep("The quick brown fox jumps over the lazy dog. ", 42)
  local text = string.rep(page, 3)
  local f, secs = time_stream(text)
  log("plain text: %d bytes in %d frames, %.0f bytes a frame", #text, f, #text / f)
  check("plain text faster than the line", #text / f > per_frame)

  -- Lines that scroll the whole screen, with CR LF
  local t = {}
  for i = 1, 200 do t[#t + 1] = string.format("line %03d %s\r\n", i, string.rep("-", 60)) end
  local scroll = table.concat(t)
  f = time_stream(scroll)
  log("scrolling lines: %d bytes in %d frames, %.0f bytes a frame", #scroll, f, #scroll / f)
  check("scrolling lines faster than the line", #scroll / f > per_frame)

  -- A colour every character, and a scroll region
  t = { CSI .. "2;" .. LAST .. "r" }
  for i = 1, 3000 do t[#t + 1] = string.format("%s3%dm%s", CSI, i % 8, string.char(65 + i % 26)) end
  local colour = table.concat(t)
  f = time_stream(colour)
  log("SGR every character, region scroll: %d bytes in %d frames, %.0f bytes a frame",
    #colour, f, #colour / f)

  -- The scrolling lines fed at the line's rate for 10 seconds: the ring
  -- must stay near empty
  send(ESC .. "c")
  local stream = string.rep(scroll, 6)
  local pos, most, frames = 1, 0, 0
  while pos <= #stream do
    local chunk = stream:sub(pos, pos + per_frame - 1)
    local left = ring_write(chunk)
    pos = pos + #chunk - #left
    frame()
    frames = frames + 1
    most = math.max(most, ring_used())
  end
  log("fed %d bytes at %d a frame over %d frames: most in the ring %d", #stream, per_frame, frames, most)
  check("terminal keeps up at 115,200 baud", most < 2 * per_frame, tostring(most))
  check("no bytes lost from the ring", rd(sym._tn_rx_overflow) == 0)
  expect_status("status bar on display row 28 after the stream")
end

-- ---------------------------------------------------------------- run

local taken_at
local finished = false

probe_taps = {
  prog:install_write_tap(sym._tn_taken, sym._tn_taken, "telnet_term_taken",
    function(offset, value, mask)
      if (value & 0xFF) == 1 and not taken_at then
        taken_at = manager.machine.time:as_double()
      end
    end),
}

probe_sub = emu.add_machine_frame_notifier(function()
  if finished or not taken_at then return end
  if manager.machine.time:as_double() < taken_at + 1.5 then return end
  if not co then
    co = coroutine.create(function()
      if mode == "speed" then speed() else tests() end
    end)
  end
  local ok, err = coroutine.resume(co)
  if not ok then
    log("FAIL probe error: %s", tostring(err))
  end
  if coroutine.status(co) == "dead" then
    log("DONE")
    finished = true
    pinesy.type("{EDIT}q")             -- EDIT, then Q ends the session
  end
end)
