/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountData.h"

#include "game/server/mmocore/Components/Houses/HouseData.h"
#include <game/server/mmocore/Components/Groups/GroupData.h>

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountTempData > CAccountTempData::ms_aPlayerTempData;

CHouseData* CAccountData::GetHouse() const
{
	auto it = std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [this](const auto& p) { return p->GetAccountID() == m_ID; });
	return it != CHouseData::Data().end() ? it->get() : nullptr;
}

void CAccountData::InitGroup()
{
	for(auto& it : GroupData::Data())
	{
		auto& Accounts = it.second.GetAccounts();
		auto AccountIter = Accounts.find(m_ID);
		if(AccountIter != Accounts.end())
		{
			m_GroupID = it.first;
			return;
		}
	}

	m_GroupID = -1;
}

GroupData* CAccountData::GetGroup() const
{
	if(GroupData::Data().find(m_GroupID) != GroupData::Data().end())
		return &GroupData::Data()[m_GroupID];
	return nullptr;
}

bool CAccountData::HasHouse() const { return GetHouse() != nullptr; }
