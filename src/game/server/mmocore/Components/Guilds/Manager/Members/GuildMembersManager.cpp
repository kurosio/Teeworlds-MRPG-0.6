/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMembersManager.h"

#include <game/server/mmocore/Components/Guilds/GuildData.h>
#include <game/server/gamecontext.h>

CGS* CGuildMembersController::GS() const { return m_pGuild->GS(); }

CGuildMembersController::CGuildMembersController(CGuildData* pGuild, std::string&& MembersData) : m_pGuild(pGuild)
{
	CGuildMembersController::Init(std::move(MembersData));
}

CGuildMembersController::~CGuildMembersController()
{
	for(auto pIterMember : m_apMembers)
	{
		delete pIterMember.second;
		pIterMember.second = nullptr;
	}

	m_apMembers.clear();
}

GUILD_MEMBER_RESULT CGuildMembersController::Join(int AccountID)
{
	if(m_apMembers.find(AccountID) != m_apMembers.end())
	{
		return GUILD_MEMBER_RESULT::JOIN_ALREADY_IN_GUILD;
	}

	m_apMembers[AccountID] = new CGuildMemberData(m_pGuild, AccountID, m_pGuild->GetRanks()->GetDefaultRank());

	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
	{
		pPlayer->Account()->ReinitializeGuild();
	}

	Save();
	return GUILD_MEMBER_RESULT::SUCCESSFUL;
}

GUILD_MEMBER_RESULT CGuildMembersController::Kick(int AccountID)
{
	if(auto Iter = m_apMembers.find(AccountID); Iter != m_apMembers.end())
	{
		delete (*Iter).second;
		m_apMembers.erase(Iter);

		if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
		{
			pPlayer->Account()->ReinitializeGuild();
		}

		Save();
		return GUILD_MEMBER_RESULT::SUCCESSFUL;
	}

	return GUILD_MEMBER_RESULT::KICK_DOES_NOT_EXIST;
}

void CGuildMembersController::Init(std::string&& MembersData)
{
	dbg_assert(m_apMembers.empty(), "");

	Tools::Json::parseFromString(MembersData, [this](nlohmann::json& pJson)
	{
		for(auto& pMember : pJson["members"])
		{
			int UID = pMember.value("id", -1);
			if(UID > 0 && m_apMembers.find(UID) == m_apMembers.end())
			{
				int RID = pMember.value("rank_id", -1);
				int Deposit = pMember.value("deposit", 0);
				m_apMembers[UID] = (new CGuildMemberData(m_pGuild, UID, m_pGuild->GetRanks()->Get(RID), Deposit));
			}
		}
	});
}

void CGuildMembersController::Save() const
{
	nlohmann::json MembersData;
	for(auto& [UID, pMember] : m_apMembers)
	{
		nlohmann::json memberData;
		memberData["id"] = UID;
		memberData["rank_id"] = pMember->GetRank()->GetID();
		memberData["deposit"] = pMember->GetDeposit();
		MembersData["members"].push_back(memberData);
	}

	Database->Execute<DB::UPDATE, 300>(TW_GUILD_TABLE, "DefaultRankID = '%d', Members = '%s' WHERE ID = '%d'", 
		m_pGuild->GetRanks()->GetDefaultRank()->GetID(), MembersData.dump().c_str(), m_pGuild->GetID());
}

CGuildMemberData* CGuildMembersController::GetMember(int AccountID)
{
	return m_apMembers.find(AccountID) != m_apMembers.end() ? m_apMembers[AccountID] : nullptr;
}