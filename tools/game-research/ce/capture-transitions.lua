-- Read-only, bounded sampler. Run only after attaching CE to the game.
local source = debug.getinfo(1, 'S').source
assert(source:sub(1, 1) == '@', 'Load this script with dofile')
local directory = source:sub(2):match('^(.*[/\\])') or './'
local json = dofile(directory .. 'json.lua')
local name = 'PirateHatGameResearchTransitions'
local previous = rawget(_G, name)
if previous ~= nil then
  assert(type(previous) == 'table' and previous.tool == 'capture-transitions',
         'Recorder namespace is already in use')
  previous.stop('script_reloaded')
end
local api = {tool = 'capture-transitions'}
local current

local function integer(value, minimum, maximum, label)
  assert(type(value) == 'number' and value == math.floor(value) and
         value >= minimum and value <= maximum, label .. ' is out of range')
  return value
end

local function dispose(session)
  local timer = session.timer
  session.timer = nil
  if timer then
    pcall(function() timer.Enabled = false end)
    pcall(function() timer.OnTimer = nil end)
    pcall(function() timer.destroy() end)
  end
end

local function finish(session, status, reason)
  if session.report.status ~= 'running' then return end
  dispose(session)
  session.report.status = status
  session.report.stop_reason = reason
  for index, field in ipairs(session.fields) do
    session.report.checks[index] = {
      id = field.label, status = field.failures == 0 and 'pass' or 'unknown',
      details = {
        message = field.failures == 0 and 'All sampled bytes were readable' or
                  'Unreadable samples do not establish an inactive state',
        unreadable_samples = field.failures
      }
    }
  end
end

local function elapsed(session)
  -- CE's millisecond tick counter can wrap; captures are limited to two minutes.
  return (getTickCount() - session.started) % 4294967296
end

local function sample(session)
  local milliseconds = elapsed(session)
  if milliseconds >= session.duration and #session.report.samples > 0 then
    finish(session, 'completed', 'duration_limit')
    return
  end
  local readings = json.array({})
  for _, field in ipairs(session.fields) do
    local ok, bytes = pcall(readBytes, field.address, field.size, true)
    local value
    if ok and type(bytes) == 'table' and #bytes == field.size then
      local formatted = {}
      for _, byte in ipairs(bytes) do
        if type(byte) ~= 'number' or byte < 0 or byte > 255 or
           byte ~= math.floor(byte) then
          formatted = nil
          break
        end
        formatted[#formatted + 1] = string.format('%02X', byte)
      end
      if formatted then value = table.concat(formatted, ' ') end
    end
    local state = value and 'readable' or 'unknown'
    if not value then field.failures = field.failures + 1 end
    readings[#readings + 1] = {label = field.label, status = state, bytes = value}
    local key = value or 'unknown'
    if key ~= field.last then
      session.report.events[#session.report.events + 1] = {
        elapsed_ms = milliseconds, label = field.label,
        kind = field.last == nil and 'initial' or 'change',
        previous = field.last, current = key
      }
      field.last = key
    end
  end
  session.report.samples[#session.report.samples + 1] = {
    elapsed_ms = milliseconds, readings = readings
  }
  session.report.elapsed_ms = milliseconds
  if #session.report.samples >= session.limit then
    finish(session, 'completed', 'sample_limit')
  end
end

function api.stop(reason)
  if current then finish(current, 'stopped', reason or 'requested') end
  return current and current.report.status or 'idle'
end

function api.status()
  return current and current.report.status or 'idle'
end

function api.result()
  assert(current, 'No capture has been started')
  return json.encode(current.report)
end

function api.start(options)
  api.stop('restarted')
  assert(type(options) == 'table', 'Expected capture options')
  local duration = integer(options.duration_ms or 15000, 1, 120000, 'duration_ms')
  local interval = integer(options.interval_ms or 100, 20, 5000, 'interval_ms')
  local limit = integer(options.max_samples or 1000, 1, 6000, 'max_samples')
  assert(type(options.fields) == 'table' and #options.fields > 0 and
         #options.fields <= 32, 'Supply 1 to 32 explicit labeled fields')
  assert(#options.fields * limit <= 20000, 'Capture exceeds 20000 field samples')
  local fields, definitions, labels = {}, json.array({}), {}
  for _, entry in ipairs(options.fields) do
    assert(type(entry) == 'table' and type(entry.label) == 'string' and
           #entry.label > 0 and #entry.label <= 128, 'Each field needs a label')
    assert(not labels[entry.label], 'Field labels must be unique')
    labels[entry.label] = true
    assert(type(entry.address) == 'number' or type(entry.address) == 'string',
           'Each field needs an explicit address or CE address expression')
    local address = entry.address
    if type(address) == 'string' then address = getAddressSafe(address) end
    integer(address, 1, 0x7FFFFFFFFFFF, 'address')
    local size = integer(entry.size or 1, 1, 8, 'size')
    fields[#fields + 1] = {
      label = entry.label, address = address, size = size, failures = 0
    }
    definitions[#definitions + 1] = {
      label = entry.label, address = string.format('0x%X', address), size = size
    }
  end
  local session = {
    fields = fields, duration = duration, limit = limit, started = getTickCount(),
    report = {
      schema_version = 1, tool = 'capture-transitions', status = 'running',
      game_version = options.game_version, scenario = options.scenario,
      duration_ms = duration, interval_ms = interval, max_samples = limit,
      fields = definitions, checks = json.array({}), samples = json.array({}),
      events = json.array({}), elapsed_ms = 0
    }
  }
  current = session
  local ok = pcall(function()
    sample(session)
    if session.report.status ~= 'running' then return end
    session.timer = createTimer(nil, false)
    session.timer.Interval = interval
    session.timer.OnTimer = function()
      local sampled = pcall(sample, session)
      if not sampled then
        session.report.error = 'Sampling failed; capture stopped'
        finish(session, 'error', 'sampling_error')
      end
    end
    session.timer.Enabled = true
  end)
  if not ok then
    session.report.error = 'Recorder startup failed; capture stopped'
    finish(session, 'error', 'startup_error')
  end
  -- A handle remains bound to this capture even if another capture starts.
  return {
    status = function() return session.report.status end,
    stop = function() finish(session, 'stopped', 'requested'); return session.report.status end,
    result = function() return json.encode(session.report) end
  }
end

rawset(_G, name, api)
return api
