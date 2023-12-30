/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMembersManager.h"

#include <game/server/mmocore/Components/Guilds/GuildData.h>
#include <game/server/gamecontext.h>

CGS* CGuildMembersController::GS() const { return m_pGuild->GS(); }

CGuildMembersController::CGuildMembersController(CGuildData* pGuild, std::string&& MembersData) : m_pGuild(pGuild)
{
	m_apMembers.reserve(MAX_GUILD_PLAYERS);

	CGuildMembersController::Init(std::move(MembersData));
}

CGuildMembersController::~CGuildMembersController()
{
	for(auto pMember : m_apMembers)
		delete pMember;

	m_apMembers.clear();
	m_apMembers.shrink_to_fit();
}

bool CGuildMembersController::Kick(int AccountID)
{
	CGuildMemberData* pMember = GetMember(AccountID);
	if(pMember == nullptr)
		return false;

	m_apMembers.erase(std::remove_if(m_apMembers.begin(), m_apMembers.end(), [&pMember](CGuildMemberData* p){return p == pMember; }), m_apMembers.end());
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
		pPlayer->Account()->ReinitializeGuild();

	Save();
	return true;
}

bool CGuildMembersController::Join(int AccountID)
{
	if(GetMember(AccountID) != nullptr)
		return false;

	m_apMembers.push_back(new CGuildMemberData(m_pGuild, AccountID));
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
		pPlayer->Account()->ReinitializeGuild();

	Save();
	return true;
}

void CGuildMembersController::Init(std::string&& MembersData)
{
	dbg_assert(m_apMembers.empty(), "");

	Tools::Json::parseFromString(std::move(MembersData), [this](nlohmann::json& pJson)
	{
		for(auto& pMember : pJson["members"])
		{
			int UID = pMember.value("id", -1);
			int RID = pMember.value("rank_id", -1);
			int Deposit = pMember.value("deposit", 0);

			m_apMembers.push_back(new CGuildMemberData(m_pGuild, UID, RID, Deposit));
		}
	});
}

void CGuildMembersController::Save() const
{
	nlohmann::json MembersData;
	for(auto& pMember : m_apMembers)
	{
		nlohmann::json memberData;
		memberData["id"] = pMember->GetAccountID();
		memberData["rank_id"] = pMember->GetRankID();
		memberData["deposit"] = pMember->GetDeposit();
		MembersData["members"].push_back(memberData);
	}

	Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "MembersData = '%s' WHERE ID = '%d'", MembersData.dump().c_str(), m_pGuild->GetID());
}

CGuildMemberData* CGuildMembersController::GetMember(int AccountID)
{
	auto IterMember = std::find_if(m_apMembers.begin(), m_apMembers.end(), 
		[&AccountID](const CGuildMemberData* pMember){return pMember->GetAccountID() == AccountID; });
	return IterMember != m_apMembers.end() ? *IterMember : nullptr;
}