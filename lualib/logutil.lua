--
-- Author: guobin
-- Date: 2017-12-16 17:51:48
--
local skynet = require "skynet"

if skynet.getenv("daemon") then
	CONSOLE_LOG = false
end

NODE_NAME = skynet.getenv("nodename")

local SERVICE_NAME = SERVICE_NAME

local function fmt_msg(fmt, ...)
	local msg = string.format(fmt, ...)
	local info = debug.getinfo(3) --NOTE: attention the level number
	if info then
		-- msg = string.format("[%s:%d] %s", info.short_src, info.currentline, msg)
		local filename = string.match(info.short_src, "[^/.]+.lua")
		msg = string.format("[%s:%s:%d] %s", filename, info.name, info.currentline, msg)
	end
	return msg
end

function LOG_DEBUG(fmt, ...)
	local msg = fmt_msg(fmt, ...)
	skynet.send(".logger", "lua", "debug", SERVICE_NAME, msg)
	return msg
end

function LOG_INFO(fmt, ...)
	local msg = fmt_msg(fmt, ...)
	skynet.send(".logger", "lua", "info", SERVICE_NAME, msg)
	return msg
end

function LOG_WARNING(fmt, ...)
	local msg = fmt_msg(fmt, ...)
	skynet.send(".logger", "lua", "warning", SERVICE_NAME, msg)
	return msg
end

function LOG_ERROR(fmt, ...)
	local msg = fmt_msg(fmt, ...)
	skynet.send(".logger", "lua", "error", SERVICE_NAME, msg)
	return msg
end

function LOG_FATAL(fmt, ...)
	local msg = fmt_msg(fmt, ...)
	skynet.send(".logger", "lua", "fatal", SERVICE_NAME, msg)
	return msg
end
