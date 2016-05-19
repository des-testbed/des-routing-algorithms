-- ----------------------------------------------------------------------------
-- Copyright 2009, David Gutzmann, Freie Universitaet Berlin (FUB).
-- Copyright 2010, Bastian Blywis, Freie Universitaet Berlin (FUB).
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
-- params: Wireshark filter name, Proto name as shown in Paket Details
GOSSIP = Proto("des_gossip_adv_ext", "DES_GOSSIP_ADV_EXT")

-- Create the protocol fields
local f = GOSSIP.fields

-- please keep the following format:
-- dessert.ext.PROTOCOL.EXTENSION-NAME.FIELD
-- Examples:
-- dessert.ext.aodv.rreq.xyz
-- dessert.ext.dsr.rrep.abc
f.value1 = ProtoField.uint8("dessert.ext.gossip.gossip-adv.value1", "value1")
f.value2 = ProtoField.uint16("dessert.ext.gossip.gossip-adv.value2", "value2")
f.char1 = ProtoField.string("dessert.ext.gossip.gossip-adv.char1", "char1")

-- The dissector function
function GOSSIP.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = "DES_GOSSIP_ADV_EXT"
    
    local subtree = tree:add(GOSSIP, buffer,"Extension Data")
	local offset = 0
	local value1 = buffer(offset, 1)
	offset = offset + 1
    local value2 = buffer(offset, 2)
	offset = offset + 2
	local char1 = buffer(offset, 1)
	offset = offset + 1
    subtree:add(f.value1, value1)
    subtree:add(f.value2, value2)
	subtree:add(f.char1, char1)
	-- always return the offset!
    return offset
end

-- params: Type, Identified, Pointer to Proto
_G.dessert_register_ext_dissector(0x40 ,"DES_GOSSIP_ADV_EXT", GOSSIP)
