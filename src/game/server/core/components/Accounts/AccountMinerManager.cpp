/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountMinerManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

std::map < int , CAccountMinerManager::StructOres > CAccountMinerManager::ms_aOre;

void CAccountMinerManager::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_positions_mining", pWhereLocalWorld);
	while (pRes->next())
	{
		const int ID = pRes->getInt("ID");
		ms_aOre[ID].m_ItemID = pRes->getInt("ItemID");
		ms_aOre[ID].m_Level = pRes->getInt("Level");
		ms_aOre[ID].m_StartHealth = pRes->getInt("Health");
		ms_aOre[ID].m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		ms_aOre[ID].m_Distance = pRes->getInt("Distance");
		ms_aOre[ID].m_WorldID = pRes->getInt("WorldID");
	}
}

void CAccountMinerManager::OnInitAccount(CPlayer* pPlayer)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_mining", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	if (pRes->next())
	{
		pPlayer->Account()->m_MiningData.initFields(&pRes);
		dbg_msg("test", "%s", pPlayer->Account()->m_MiningData.getUpdateField().c_str());
		return;
	}

	pPlayer->Account()->m_MiningData(JOB_LEVEL, 1).m_Value = 1;
	pPlayer->Account()->m_MiningData(JOB_EXPERIENCE, 0).m_Value = 0;
	pPlayer->Account()->m_MiningData(JOB_UPGRADES, 0).m_Value = 0;
	Database->Execute<DB::INSERT>("tw_accounts_mining", "(UserID) VALUES ('%d')", pPlayer->Account()->GetID());
}

int CAccountMinerManager::GetOreLevel(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aOre.begin(), ms_aOre.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aOre.end() ? (*Iter).second.m_Level : -1;
}

int CAccountMinerManager::GetOreItemID(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aOre.begin(), ms_aOre.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aOre.end() ? (*Iter).second.m_ItemID : -1;
}

int CAccountMinerManager::GetOreHealth(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aOre.begin(), ms_aOre.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aOre.end() ? (*Iter).second.m_StartHealth : -1;
}

//void CAccountMinerManager::ShowMenu(CPlayer *pPlayer) const
//{
//	const int ClientID = pPlayer->GetCID();
//	const int JobLevel = pPlayer->AccountManager()->m_MiningData(JOB_LEVEL, 0).m_Value;
//	const int JobExperience = pPlayer->AccountManager()->m_MiningData(JOB_EXPERIENCE, 0).m_Value;
//	const int JobUpgrades = pPlayer->AccountManager()->m_MiningData(JOB_UPGRADES, 0).m_Value;
//	const int ExperienceNeed = computeExperience(JobLevel);
//
//	GS()->AVM(ClientID, "null", NOPE, TAB_UPGR_JOB, "Miner Point: {INT} :: Level: {INT} Exp: {INT}/{INT}", JobUpgrades, JobLevel, JobExperience, ExperienceNeed);
//}

bool CAccountMinerManager::ShowGuideDropByWorld(int WorldID, CPlayer* pPlayer)
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();
	
	for(const auto& [ID, Ore] : ms_aOre)
	{
		if (WorldID == Ore.m_WorldID)
		{
			const int HideID = (NUM_TAB_MENU + ID) << 0x10;
			const vec2 Pos = Ore.m_Position / 32.0f;
			CItemDescription* pItemInfo = GS()->GetItemInfo(Ore.m_ItemID);
			GS()->AVH(ClientID, HideID, "Ore {STR} [x{INT} y{INT}]", pItemInfo->GetName(), Ore.m_StartHealth, (int)Pos.x, (int)Pos.y);
			GS()->AVM(ClientID, "null", NOPE, HideID, "Level: {INT} | Health: {INT}P", Ore.m_Level, Ore.m_StartHealth);
			GS()->AVM(ClientID, "null", NOPE, HideID, "Distance of distribution: {INT}P", Ore.m_Distance);
			Found = true;
		}
	}
	return Found;
}

void CAccountMinerManager::Work(CPlayer *pPlayer, int Level)
{
	const int ClientID = pPlayer->GetCID();
	const int MultiplierExperience = maximum(1, (int)computeExperience(Level) / g_Config.m_SvMiningIncreaseLevel);
	pPlayer->Account()->m_MiningData(JOB_EXPERIENCE, 0).m_Value += MultiplierExperience;

	int ExperienceNeed = computeExperience(pPlayer->Account()->m_MiningData(JOB_LEVEL, 0).m_Value);
	for( ; pPlayer->Account()->m_MiningData(JOB_EXPERIENCE, 0).m_Value >= ExperienceNeed; )
	{
		pPlayer->Account()->m_MiningData(JOB_EXPERIENCE, 0).m_Value -= ExperienceNeed;
		pPlayer->Account()->m_MiningData(JOB_LEVEL, 0).m_Value++;
		pPlayer->Account()->m_MiningData(JOB_UPGRADES, 0).m_Value++;

		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
		{
			GS()->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, 4);
			GS()->CreateDeath(pPlayer->GetCharacter()->m_Core.m_Pos, ClientID);
			GS()->CreateText(pPlayer->GetCharacter(), false, vec2(0, -40), vec2(0, -1), 40, "miner up");
		}

		const int NewLevel = pPlayer->Account()->m_MiningData(JOB_LEVEL, 0).m_Value;
		ExperienceNeed = computeExperience(NewLevel);
		GS()->Chat(ClientID, "Miner Level UP. Now Level {INT}!", NewLevel);
	}

	pPlayer->ProgressBar("Miner", pPlayer->Account()->m_MiningData(JOB_LEVEL, 0).m_Value, pPlayer->Account()->m_MiningData(JOB_EXPERIENCE, 0).m_Value, ExperienceNeed, MultiplierExperience);
	Core()->SaveAccount(pPlayer, SAVE_MINER_DATA);
}


bool CAccountMinerManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if (PPSTR(CMD, "MINERUPGRADE") == 0)
	{
		if (pPlayer->Upgrade(Get, &pPlayer->Account()->m_MiningData(VoteID, 0).m_Value, &pPlayer->Account()->m_MiningData(JOB_UPGRADES, 0).m_Value, VoteID2, 3))
		{
			GS()->Core()->SaveAccount(pPlayer, SAVE_MINER_DATA);
			GS()->StrongUpdateVotes(ClientID, MENU_UPGRADES);
		}
		return true;
	}

	return false;
}