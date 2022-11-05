/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountPlantCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>

std::map < int , CAccountPlantCore::StructPlants > CAccountPlantCore::ms_aPlants;

void CAccountPlantCore::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_positions_plant", pWhereLocalWorld);
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		ms_aPlants[ID].m_ItemID = pRes->getInt("ItemID");
		ms_aPlants[ID].m_Level = pRes->getInt("Level");
		ms_aPlants[ID].m_StartHealth = pRes->getInt("Health");
		ms_aPlants[ID].m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		ms_aPlants[ID].m_Distance = pRes->getInt("Distance");
		ms_aPlants[ID].m_WorldID = pRes->getInt("WorldID");
	}
}

void CAccountPlantCore::OnInitAccount(CPlayer *pPlayer)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_farming", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	if(pRes->next())
	{
		pPlayer->Acc().m_FarmingData.initFields(&pRes);
		return;
	}

	pPlayer->Acc().m_FarmingData(JOB_LEVEL, 1).m_Value = 1;
	pPlayer->Acc().m_FarmingData(JOB_EXPERIENCE, 0).m_Value = 0;
	pPlayer->Acc().m_FarmingData(JOB_UPGRADES, 0).m_Value = 0;
	Database->Execute<DB::INSERT>("tw_accounts_farming", "(UserID) VALUES ('%d')", pPlayer->Acc().m_UserID);
}

int CAccountPlantCore::GetPlantLevel(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aPlants.begin(), ms_aPlants.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aPlants.end() ? (*Iter).second.m_Level : -1;
}

int CAccountPlantCore::GetPlantItemID(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aPlants.begin(), ms_aPlants.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aPlants.end() ? (*Iter).second.m_ItemID : -1;
}

int CAccountPlantCore::GetPlantHealth(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aPlants.begin(), ms_aPlants.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aPlants.end() ? (*Iter).second.m_StartHealth : -1;
}

//void CAccountPlantCore::ShowMenu(CPlayer* pPlayer) const
//{
//	const int ClientID = pPlayer->GetCID();
//	const int JobLevel = pPlayer->Acc().m_FarmingData(JOB_LEVEL, 0).m_Value;
//	const int JobExperience = pPlayer->Acc().m_FarmingData(JOB_EXPERIENCE, 0).m_Value;
//	const int JobUpgrades = pPlayer->Acc().m_FarmingData(JOB_UPGRADES, 0).m_Value;
//	const int JobUpgrQuantity = pPlayer->Acc().m_FarmingData(JOB_UPGR_QUANTITY, 0).m_Value;
//	const int ExperienceNeed = computeExperience(JobLevel);
//
//	GS()->AVM(ClientID, "null", NOPE, TAB_UPGR_JOB, "Plants Point: {INT} :: Level: {INT} Exp: {INT}/{INT}", JobUpgrades, JobLevel, JobExperience, ExperienceNeed);
//}

bool CAccountPlantCore::ShowGuideDropByWorld(int WorldID, CPlayer* pPlayer)
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();
	
	for(const auto& [ID, Plant] : ms_aPlants)
	{
		if (WorldID == Plant.m_WorldID)
		{
			const int HideID = (NUM_TAB_MENU + ID) << 0x10;
			const vec2 Pos = Plant.m_Position / 32.0f;
			CItemDescription* pItemInfo = GS()->GetItemInfo(Plant.m_ItemID);
			GS()->AVH(ClientID, HideID, "Plant {STR} [x{INT} y{INT}]", pItemInfo->GetName(), Plant.m_StartHealth, (int)Pos.x, (int)Pos.y);
			GS()->AVM(ClientID, "null", NOPE, HideID, "Level: {INT} | Health: {INT}P", Plant.m_Level, Plant.m_StartHealth);
			GS()->AVM(ClientID, "null", NOPE, HideID, "Distance of distribution: {INT}P", Plant.m_Distance);
			Found = true;
		}
	}
	return Found;
}

void CAccountPlantCore::Work(CPlayer* pPlayer, int Level)
{
	const int ClientID = pPlayer->GetCID();
	const int MultiplierExperience = computeExperience(Level) / g_Config.m_SvPlantingIncreaseLevel;
	pPlayer->Acc().m_FarmingData(JOB_EXPERIENCE, 0).m_Value += clamp(MultiplierExperience, 1, MultiplierExperience);

	int ExperienceNeed = computeExperience(pPlayer->Acc().m_FarmingData(JOB_LEVEL, 0).m_Value);
	for (; pPlayer->Acc().m_FarmingData(JOB_EXPERIENCE, 0).m_Value >= ExperienceNeed; )
	{
		pPlayer->Acc().m_FarmingData(JOB_EXPERIENCE, 0).m_Value -= ExperienceNeed;
		pPlayer->Acc().m_FarmingData(JOB_LEVEL, 0).m_Value++;
		pPlayer->Acc().m_FarmingData(JOB_UPGRADES, 0).m_Value++;

		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
		{
			GS()->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, 4);
			GS()->CreateDeath(pPlayer->GetCharacter()->m_Core.m_Pos, ClientID);
			GS()->CreateText(pPlayer->GetCharacter(), false, vec2(0, -40), vec2(0, -1), 40, "plants up");
		}

		ExperienceNeed = computeExperience(pPlayer->Acc().m_FarmingData(JOB_LEVEL, 0).m_Value);
		GS()->Chat(ClientID, "Plants Level UP. Now Level {INT}!", pPlayer->Acc().m_FarmingData(JOB_LEVEL, 0).m_Value);
	}

	pPlayer->ProgressBar("Plants", pPlayer->Acc().m_FarmingData(JOB_LEVEL, 0).m_Value, pPlayer->Acc().m_FarmingData(JOB_EXPERIENCE, 0).m_Value, ExperienceNeed, MultiplierExperience);
	Job()->SaveAccount(pPlayer, SAVE_PLANT_DATA);
}

bool CAccountPlantCore::OnHandleVoteCommands(CPlayer *pPlayer, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "PLANTUPGRADE") == 0)
	{
		if(pPlayer->Upgrade(Get, &pPlayer->Acc().m_FarmingData(VoteID, 0).m_Value, &pPlayer->Acc().m_FarmingData(JOB_UPGRADES, 0).m_Value, VoteID2, 3))
		{
			GS()->Mmo()->SaveAccount(pPlayer, SAVE_PLANT_DATA);
			GS()->StrongUpdateVotes(ClientID, MENU_UPGRADES);
		}
		return true;
	}

	return false;
}