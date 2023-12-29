/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMembersManager.h"

#include <game/server/mmocore/Components/Guilds/GuildData.h>
#include <game/server/gamecontext.h>

CGS* CGuildMembersController::GS() const
{
	return m_pGuild->GS();
}

CGuildMembersController::CGuildMembersController(CGuildData* pGuild) : m_pGuild(pGuild)
{
	m_apMembers.reserve(MAX_GUILD_PLAYERS);

	CGuildMembersController::InitMembers();
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

	CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID);
	if(pPlayer)
	{
		pPlayer->Account()->ReinitializeGuild();
	}

	Database->Execute<DB::UPDATE, 1000>("tw_accounts_data", "GuildID = 'NULL', GuildDeposit = '0', GuildRank = 'NULL' WHERE ID = '%d'", AccountID);
	return true;
}

bool CGuildMembersController::Join(int AccountID)
{
	if(GetMember(AccountID) != nullptr)
		return false;

	m_apMembers.push_back(new CGuildMemberData(m_pGuild, AccountID));

	CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID);
	if(pPlayer)
	{
		pPlayer->Account()->ReinitializeGuild();
	}

	Database->Execute<DB::UPDATE, 1000>("tw_accounts_data", "GuildID = '%d', GuildDeposit = '0', GuildRank = 'NULL' WHERE ID = '%d'", m_pGuild->GetID(), AccountID);
	return true;
}

void CGuildMembersController::InitMembers()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, GuildRank, GuildDeposit", "tw_accounts_data", "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		int UID = pRes->getInt("ID");
		GuildRankIdentifier Rank = pRes->getInt("GuildRank");
		int Deposit = pRes->getInt("GuildDeposit");

		m_apMembers.push_back(new CGuildMemberData(m_pGuild, UID, Rank, Deposit));
	}
}

CGuildMemberData* CGuildMembersController::GetMember(int AccountID)
{
	auto IterMember = std::find_if(m_apMembers.begin(), m_apMembers.end(), 
		[&AccountID](const CGuildMemberData* pMember){return pMember->GetAccountID() == AccountID; });
	return IterMember != m_apMembers.end() ? *IterMember : nullptr;
}