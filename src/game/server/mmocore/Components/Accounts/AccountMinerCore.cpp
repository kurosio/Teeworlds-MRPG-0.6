/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountMinerCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

std::map < int , CAccountMinerCore::StructOres > CAccountMinerCore::ms_aOre;

void CAccountMinerCore::ShowMenu(CPlayer *pPlayer) const
{
	const int ClientID = pPlayer->GetCID();
	const int JobLevel = pPlayer->Acc().m_aMining[JOB_LEVEL].m_Value;
	const int JobExperience = pPlayer->Acc().m_aMining[JOB_EXPERIENCE].m_Value;
	const int JobUpgrades = pPlayer->Acc().m_aMining[JOB_UPGRADES].m_Value;
	const int JobUpgrQuantity = pPlayer->Acc().m_aMining[JOB_UPGR_QUANTITY].m_Value;
	const int ExperienceNeed = computeExperience(JobLevel);

	GS()->AVM(ClientID, "null", NOPE, TAB_UPGR_JOB, "Miner Point: {INT} :: Level: {INT} Exp: {INT}/{INT}", JobUpgrades, JobLevel, JobExperience, ExperienceNeed);
	GS()->AVD(ClientID, "MINERUPGRADE", JOB_UPGR_QUANTITY, 20, TAB_UPGR_JOB, "Quantity +{INT} (Price 20P)", JobUpgrQuantity);
}

int CAccountMinerCore::GetOreLevel(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aOre.begin(), ms_aOre.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aOre.end() ? (*Iter).second.m_Level : -1;
}

int CAccountMinerCore::GetOreItemID(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aOre.begin(), ms_aOre.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aOre.end() ? (*Iter).second.m_ItemID : -1;
}

int CAccountMinerCore::GetOreHealth(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aOre.begin(), ms_aOre.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aOre.end() ? (*Iter).second.m_StartHealth : -1;
}

void CAccountMinerCore::Work(CPlayer *pPlayer, int Level)
{
	const int ClientID = pPlayer->GetCID();
	const int MultiplierExperience = computeExperience(Level) / g_Config.m_SvMiningIncreaseLevel;
	pPlayer->Acc().m_aMining[JOB_EXPERIENCE].m_Value += clamp(MultiplierExperience, 1, MultiplierExperience);

	int ExperienceNeed = computeExperience(pPlayer->Acc().m_aMining[JOB_LEVEL].m_Value);
	for( ; pPlayer->Acc().m_aMining[JOB_EXPERIENCE].m_Value >= ExperienceNeed; )
	{
		pPlayer->Acc().m_aMining[JOB_EXPERIENCE].m_Value -= ExperienceNeed;
		pPlayer->Acc().m_aMining[JOB_LEVEL].m_Value++;
		pPlayer->Acc().m_aMining[JOB_UPGR_QUANTITY].m_Value++;

		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
		{
			GS()->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, 4);
			GS()->CreateDeath(pPlayer->GetCharacter()->m_Core.m_Pos, ClientID);
			GS()->CreateText(pPlayer->GetCharacter(), false, vec2(0, -40), vec2(0, -1), 40, "miner up");
		}

		const int NewLevel = pPlayer->Acc().m_aMining[JOB_LEVEL].m_Value;
		ExperienceNeed = computeExperience(NewLevel);
		GS()->Chat(ClientID, "Miner Level UP. Now Level {INT}!", NewLevel);
	}

	pPlayer->ProgressBar("Miner", pPlayer->Acc().m_aMining[JOB_LEVEL].m_Value, pPlayer->Acc().m_aMining[JOB_EXPERIENCE].m_Value, ExperienceNeed, MultiplierExperience);
	Job()->SaveAccount(pPlayer, SAVE_MINER_DATA);
}

void CAccountMinerCore::OnInitAccount(CPlayer* pPlayer)
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_accounts_mining", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	if (pRes->next())
	{
		for(int i = 0; i < NUM_JOB_ACCOUNTS_STATS; i++)
		{
			const char* pFieldName = pPlayer->Acc().m_aMining[i].getFieldName();
			pPlayer->Acc().m_aMining[i].m_Value = pRes->getInt(pFieldName);
		}
		return;
	}
	pPlayer->Acc().m_aMining[JOB_LEVEL].m_Value = 1;
	pPlayer->Acc().m_aMining[JOB_UPGR_QUANTITY].m_Value = 1;
	Sqlpool.Execute<DB::INSERT>("tw_accounts_mining", "(UserID) VALUES ('%d')", pPlayer->Acc().m_UserID);
}

void CAccountMinerCore::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_positions_mining", pWhereLocalWorld);
	while (pRes->next())
	{
		const int ID = pRes->getInt("ID");
		ms_aOre[ID].m_ItemID = pRes->getInt("ItemID");
		ms_aOre[ID].m_Level = pRes->getInt("Level");
		ms_aOre[ID].m_StartHealth = pRes->getInt("Health");
		ms_aOre[ID].m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		ms_aOre[ID].m_Distance = pRes->getInt("Distance");
	}
}

bool CAccountMinerCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if (PPSTR(CMD, "MINERUPGRADE") == 0)
	{
		if (pPlayer->Upgrade(Get, &pPlayer->Acc().m_aMining[VoteID].m_Value, &pPlayer->Acc().m_aMining[JOB_UPGRADES].m_Value, VoteID2, 3))
		{
			GS()->Mmo()->SaveAccount(pPlayer, SAVE_MINER_DATA);
			GS()->StrongUpdateVotes(ClientID, MENU_UPGRADE);
		}
		return true;
	}

	return false;
}