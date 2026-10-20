#pragma once

#include <networksystem/inetworkmessages.h>
#include "netmessages.pb.h"
#include "networkbasetypes.pb.h"

class CNETMsg_Tick_t : public CNetMessagePB<CNETMsg_Tick> {};
class CNETMsg_StringCmd_t : public CNetMessagePB<CNETMsg_StringCmd> {};
class CCLCMsg_Move_t : public CNetMessagePB<CCLCMsg_Move> {};
