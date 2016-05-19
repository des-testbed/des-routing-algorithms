-- ----------------------------------------------------------------------------
-- Copyright 2009, David Gutzmann, Freie Universitaet Berlin (FUB).
-- Copyright 2010, Bastian Blywis, Freie Universitaet Berlin (FUB).
--                 Alexander Ende, Freie Universitaet Berlin (FUB).
-- All rights reserved.

--These sources were originally developed by David Gutzmann,
--rewritten and extended by Bastian Blywis
--at Freie Universitaet Berlin (http://www.fu-berlin.de/),
--Computer Systems and Telematics / Distributed, embedded Systems (DES) group 
--(http://cst.mi.fu-berlin.de, http://www.des-testbed.net)
-- ----------------------------------------------------------------------------
--This program is free software: you can redistribute it and/or modify it under
--the terms of the GNU General Public License as published by the Free Software
--Foundation, either version 3 of the License, or (at your option) any later
--version.

--This program is distributed in the hope that it will be useful, but WITHOUT
--ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
--FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

--You should have received a copy of the GNU General Public License along with
--this program. If not, see http://www.gnu.org/licenses/ .
-- ----------------------------------------------------------------------------
--For further information and questions please use the web site
--       http://www.des-testbed.net
-- ----------------------------------------------------------------------------

-- Create a new dissector
DESSERT_EXT_ARA_FANT = Proto("dessert_ext_ara_fant", "DESSERT_EXT_ARA_FANT")

-- Create the protocol fields
local f = DESSERT_EXT_ARA_FANT.fields

f.source = ProtoField.ether("dessert.ext.ara.fant.source", "Source")
f.tag = ProtoField.string("dessert.ext.ara.fant.tag", "Type")

-- The dissector function
function DESSERT_EXT_ARA_FANT.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = "DESSERT_EXT_ARA_FANT"
    
    local subtree = tree:add(DESSERT_EXT_ARA_FANT, buffer,"Extension Data")
    local offset = 0
    local source = buffer(offset, 6)
    offset = offset + 6
    local tag = buffer(offset, 4)
    offset = offset + 4
    subtree:add(f.source, source)
    subtree:add(f.tag, tag)
    _G.g_offset = offset
    return offset
end


-- Create BANT dissector
DESSERT_EXT_ARA_BANT = Proto("dessert_ext_ara_bant", "DESSERT_EXT_ARA_BANT")

local r = DESSERT_EXT_ARA_BANT.fields

r.source = ProtoField.ether("dessert.ext.ara.bant.source", "Source")
r.tag = ProtoField.string("dessert.ext.ara.bant.tag", "Type")

function DESSERT_EXT_ARA_BANT.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = "DESSERT_EXT_ARA_BANT"
    local subtree = tree:add(DESSERT_EXT_ARA_BANT, buffer,"Extension Data")
    local offset = 0
    local source = buffer(offset, 6)
    offset = offset + 6
    local tag = buffer(offset, 4)
    offset = offset + 4
    subtree:add(r.source, source)
    subtree:add(r.tag, tag)
    _G.g_offset = offset
    return offset
end

-- register both dissectors
_G.dessert_register_ext_dissector(0x40, "DESSERT_EXT_ARA_FANT", DESSERT_EXT_ARA_FANT)
_G.dessert_register_ext_dissector(0x41, "DESSERT_EXT_ARA_BANT", DESSERT_EXT_ARA_BANT)
