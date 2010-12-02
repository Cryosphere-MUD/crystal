-- this needs to be $HOME/.crystal.lua
-- functions available at the moment are just
--   register_auto (string, int) 
--   register_trig (string)
--   register_prompt (string)
--   register_host (string)
--   
--   tomud (string)      -- sends a command to the mud without any echo
--   tomud_echo (string) -- sends a command to the mud, and echo it
--   info (string)       -- send an info message to the buffer

function auto()
  tomud("")
-- stops us from idling out
end

function trig(s)
  # in LUA 4 this is strfind, not string.find
  if (string.find(s, "^Muon the Barman gives a .* to you.$")) then
    tomud_echo("drink all");
  end
end

function prompt(s)
--  if (s == "By what name shall I call you? ") then
--    tomud_echo("")
--  end
--    an autologin trigger.  replace "" with username and password.
--  if (s == "Password:") then
--    tomud_echo("")
--  end
  if (s == "or press [Enter] to keep vis level (Current: 0): ") then
    tomud_echo("")
  end
end

function host(h)
  if h=="cs" then
    return "cryosphere.net:6666"
  end
  if h=="tf" then
    return "terrafirma.terra.mud.org:2222"
  end
 -- host dealiaser thing. the port is the default port to use instead of 'telnet'
end

register_auto("auto", 60*30)
register_trig("trig");
register_prompt("prompt");
register_host("host")
