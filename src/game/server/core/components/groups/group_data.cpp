/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "group_data.h"

#include "game/server/gamecontext.h"

bool GroupData::Add(int AccountID)
{
	// initialize variables
	const auto pGS = (CGS*)Instance::Server()->GameServer();
	const CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);

	// check is player already in group
	if(pPlayer && pPlayer->Account()->GetGroup())
	{
		pGS->ChatAccount(AccountID, "You're already in a group!");
		return false;
	}

	// check is group full
	if(IsFull())
	{
		pGS->ChatAccount(AccountID, "The group is full!");
		return false;
	}

	// try insert account id
	if(m_vAccountIds.insert(AccountID).second)
	{
		// set player group
		if(pPlayer)
			pPlayer->Account()->SetGroup(Data()[m_ID]);

		// send message and save
		Save();
		pGS->ChatAccount(AccountID, "You have become a member of the group!");
		return true;
	}

	return false;
}

bool GroupData::Remove(int AccountID)
{
	// initialize variables
	const auto pGS = (CGS*)Instance::Server()->GameServer();
	const CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);

	// check if player is in group
	if(pPlayer && (!pPlayer->Account()->GetGroup() || pPlayer->Account()->GetGroup()->GetID() != m_ID))
	{
		pGS->ChatAccount(AccountID, "You're not in a group!");
		return false;
	}

	// try erase account id
	if(m_vAccountIds.erase(AccountID) > 0)
	{
		// set player group
		if(pPlayer)
			pPlayer->Account()->SetGroup(nullptr);

		// if is empty disband group
		if(m_vAccountIds.empty())
		{
			pGS->ChatAccount(AccountID, "The group was disbanded.");
			Disband();
			return true;
		}

		// set new begin leader
		if(AccountID == m_LeaderUID)
		{
			ChangeOwner(*m_vAccountIds.begin());
		}

		// send message and save
		Save();
		pGS->ChatAccount(AccountID, "You left the group.");
		return true;
	}

	return false;
}

void GroupData::Disband() const
{
	m_pData.erase(m_ID);
	Database->Execute<DB::REMOVE>(TW_GROUPS_TABLE, "WHERE ID = '{}'", m_ID);
}

void GroupData::ChangeOwner(int AccountID)
{
	// assert if the account ID is not included in the account IDs
	dbg_assert(m_vAccountIds.find(AccountID) != m_vAccountIds.end(), "[Group system] account not included inside accountids");

	// update leader UID
	m_LeaderUID = AccountID;
	Save();

	// send messages
	CGS* pGS = (CGS*)Instance::Server()->GameServer();
	pGS->ChatAccount(m_LeaderUID, "You've transferred ownership of the group!");
	pGS->ChatAccount(AccountID, "You are now the new owner of the group!");
}

void GroupData::UpdateRandomColor()
{
	// start random color
	while(true)
	{
		// initialize variables
		const int Color = rand() % 63;
		bool IsFree = true;

		// check is color free
		for(const auto& [ID, pGroup] : m_pData)
		{
			if(pGroup->GetTeamColor() == Color)
			{
				IsFree = false;
				break;
			}
		}

		// set random color if is free
		if(IsFree)
		{
			m_TeamColor = Color;
			break;
		}
	}
}

int GroupData::GetOnlineCount() const
{
	// initialize variables
	int Online = 0;
	const auto pGS = (CGS*)Instance::Server()->GameServer();

	// count online players
	for(const auto& UID : m_vAccountIds)
	{
		if(pGS->GetPlayerByUserID(UID))
			Online++;
	}

	return Online;
}

void GroupData::Save() const
{
	// initialize variables
	std::string StrAccountIds;
	StrAccountIds.reserve(m_vAccountIds.size() * 8);

	// collect account ids
	for(const auto& UID : m_vAccountIds)
		StrAccountIds += std::to_string(UID) + ",";

	// remove last comma
	if(!StrAccountIds.empty())
		StrAccountIds.pop_back();

	// update group data
	Database->Execute<DB::UPDATE>(TW_GROUPS_TABLE, "AccountIDs = '{}', OwnerUID = '{}' WHERE ID = '{}'",
		StrAccountIds.c_str(), m_LeaderUID, m_ID);
}
