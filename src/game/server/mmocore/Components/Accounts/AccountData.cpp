/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountData.h"

#include "game/server/mmocore/Components/Houses/HouseData.h"

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountTempData > CAccountTempData::ms_aPlayerTempData;

CHouseData* CAccountData::GetHouse()
{
	if(m_pHouse->GetAccountID() == m_UserID)
		return m_pHouse;

	m_pHouse = nullptr;
	return m_pHouse;
}
