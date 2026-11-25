/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_BANS_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_BANS_H

#include <base/timeperiod.h>

#include <functional>
#include <string>
#include <vector>

class CPlayer;

namespace AccountBans
{
	struct SBan
	{
		int id;
		std::string until;
		std::string nickname;
		std::string reason;
	};

	void Ban(CPlayer* pPlayer, CTimePeriod Time, const std::string& Reason);
	void Unban(int BanId);
	void List(std::function<void(std::vector<SBan>&&)> OnLoaded);
}

#endif
