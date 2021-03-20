skynetroot = "skynet/"
thread = 4
logger = "log"
logservice = "snlua"
logpath = "."
harbor = 0
start = "main"
bootstrap = "snlua bootstrap"	-- The service for bootstrap
luaservice = skynetroot .. "service/?.lua;" ..
			"./server/login/?.lua"

lualoader = skynetroot .. "lualib/loader.lua"

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