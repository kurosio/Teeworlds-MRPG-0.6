#ifndef GAME_SERVER_CORE_TOOLS_ITEM_SOURCE_HELPER_H
#define GAME_SERVER_CORE_TOOLS_ITEM_SOURCE_HELPER_H

class CGS;

struct ItemHelper
{
	static std::string BuildSourceHint(CGS* pGS, int ItemID);
};

#endif