/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountData.h"

#include "game/server/mmocore/Components/Houses/HouseData.h"

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountTempData > CAccountTempData::ms_aPlayerTempData;

CHouseData* CAccountData::GetHouse() const
{
	CHouseData* pHouse = std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [this](const CHouseData& p){ return p.GetAccountID() == m_UserID; });
	return pHouse;
}

bool CAccountData::HasHouse() const { return GetHouse() != nullptr; }
