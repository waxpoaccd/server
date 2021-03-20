skynetroot = "skynet/"
thread = 4
logger = "log"
logservice = "snlua"
logpath = "."
harbor = 0
start = "main"
bootstrap = "snlua bootstrap"	-- The service for bootstrap
luaservice = skynetroot .. "service/?.lua;" ..
			"./luaservice/?.lua;"..
			"./server/login/?.lua"

nodename = "$NODE_NAME"

lualoader = skynetroot .. "lualib/loader.lua"
preload = "./lualib/preload.lua"

cpath = skynetroot .. "cservice/?.so"

lua_path = skynetroot .. "lualib/?.lua;" ..
			"./server/login/?.lua;" ..
			"./server/game/logic/?.lua;" ..
			"./server/game/logic-util/?.lua;" ..
			"./server/game/room/?.lua;" ..
			"./server/game/table/?.lua;" ..
			"./lualib/?.lua;" ..
			"./protocol/?.lua;"..
			"./config/?.lua;"

lua_cpath = skynetroot .. "luaclib/?.so;" .. 
			"./luaclib/?.so"