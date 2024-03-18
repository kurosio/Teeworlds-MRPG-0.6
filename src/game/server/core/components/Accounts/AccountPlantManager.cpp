/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountPlantManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

std::map < int , CAccountPlantManager::StructPlants > CAccountPlantManager::ms_aPlants;

void CAccountPlantManager::OnInitWorld(const char* pWhereLocalWorld)
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

void CAccountPlantManager::OnInitAccount(CPlayer *pPlayer)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_farming", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	if(pRes->next())
	{
		pPlayer->Account()->m_FarmingData.initFields(&pRes);
		return;
	}

	pPlayer->Account()->m_FarmingData(JOB_LEVEL, 1).m_Value = 1;
	pPlayer->Account()->m_FarmingData(JOB_EXPERIENCE, 0).m_Value = 0;
	pPlayer->Account()->m_FarmingData(JOB_UPGRADES, 0).m_Value = 0;
	Database->Execute<DB::INSERT>("tw_accounts_farming", "(UserID) VALUES ('%d')", pPlayer->Account()->GetID());
}

int CAccountPlantManager::GetPlantLevel(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aPlants.begin(), ms_aPlants.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aPlants.end() ? (*Iter).second.m_Level : -1;
}

int CAccountPlantManager::GetPlantItemID(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aPlants.begin(), ms_aPlants.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aPlants.end() ? (*Iter).second.m_ItemID : -1;
}

int CAccountPlantManager::GetPlantHealth(vec2 Pos) const
{
	auto Iter = std::find_if(ms_aPlants.begin(), ms_aPlants.end(), [Pos](auto& p)
	{	return distance(p.second.m_Position, Pos) < p.second.m_Distance;	});
	return Iter != ms_aPlants.end() ? (*Iter).second.m_StartHealth : -1;
}

//void CAccountPlantManager::ShowMenu(CPlayer* pPlayer) const
//{
//	const int ClientID = pPlayer->GetCID();
//	const int JobLevel = pPlayer->AccountManager()->m_FarmingData(JOB_LEVEL, 0).m_Value;
//	const int JobExperience = pPlayer->AccountManager()->m_FarmingData(JOB_EXPERIENCE, 0).m_Value;
//	const int JobUpgrades = pPlayer->AccountManager()->m_FarmingData(JOB_UPGRADES, 0).m_Value;
//	const int JobUpgrQuantity = pPlayer->AccountManager()->m_FarmingData(JOB_UPGR_QUANTITY, 0).m_Value;
//	const int ExperienceNeed = computeExperience(JobLevel);
//
//	GS()->AVM(ClientID, "null", NOPE, TAB_UPGR_JOB, "Plants Point: {INT} :: Level: {INT} Exp: {INT}/{INT}", JobUpgrades, JobLevel, JobExperience, ExperienceNeed);
//}

bool CAccountPlantManager::InsertItemsDetailVotes(CPlayer* pPlayer, int WorldID)
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();

	for(const auto& [ID, Plant] : ms_aPlants)
	{
		if(WorldID != Plant.m_WorldID)
			continue;

		const vec2 Pos = Plant.m_Position / 32.0f;
		CVoteWrapper VPlant(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "Plant {STR}", GS()->GetItemInfo(Plant.m_ItemID)->GetName());
		VPlant.MarkList().Add("Location:");
		{
			VPlant.BeginDepth();
			VPlant.Add(Instance::Localize(ClientID, Instance::Server()->GetWorldName(WorldID)));
			VPlant.Add("x{INT} y{INT}", (int)Pos.x, (int)Pos.y);
			VPlant.EndDepth();
		}
		VPlant.AddLine();
		VPlant.MarkList().Add("Description");
		{
			VPlant.BeginDepth();
			VPlant.Add("Level: {INT}", Plant.m_Level);
			VPlant.Add("Health: {INT}P", Plant.m_StartHealth);
			VPlant.Add("Distance of distribution: {INT}P", Plant.m_Distance);
			VPlant.EndDepth();
		}
		VPlant.AddLine();
		Found = true;
	}

	return Found;
}

void CAccountPlantManager::Work(CPlayer* pPlayer, int Level)
{
	const int ClientID = pPlayer->GetCID();
	const int MultiplierExperience = maximum(1, (int)computeExperience(Level) / g_Config.m_SvPlantingIncreaseLevel);
	pPlayer->Account()->m_FarmingData(JOB_EXPERIENCE, 0).m_Value += MultiplierExperience;

	int ExperienceNeed = computeExperience(pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value);
	for (; pPlayer->Account()->m_FarmingData(JOB_EXPERIENCE, 0).m_Value >= ExperienceNeed; )
	{
		pPlayer->Account()->m_FarmingData(JOB_EXPERIENCE, 0).m_Value -= ExperienceNeed;
		pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value++;
		pPlayer->Account()->m_FarmingData(JOB_UPGRADES, 0).m_Value++;

		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
		{
			GS()->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, 4);
			GS()->CreateDeath(pPlayer->GetCharacter()->m_Core.m_Pos, ClientID);
			GS()->CreateText(pPlayer->GetCharacter(), false, vec2(0, -40), vec2(0, -1), 40, "plants up");
		}

		ExperienceNeed = computeExperience(pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value);
		GS()->Chat(ClientID, "Plants Level UP. Now Level {INT}!", pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value);
	}

	pPlayer->ProgressBar("Plants", pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value, pPlayer->Account()->m_FarmingData(JOB_EXPERIENCE, 0).m_Value, ExperienceNeed, MultiplierExperience);
	Core()->SaveAccount(pPlayer, SAVE_PLANT_DATA);
}

bool CAccountPlantManager::OnHandleVoteCommands(CPlayer *pPlayer, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "PLANTUPGRADE") == 0)
	{
		if(pPlayer->Upgrade(Get, &pPlayer->Account()->m_FarmingData(VoteID, 0).m_Value, &pPlayer->Account()->m_FarmingData(JOB_UPGRADES, 0).m_Value, VoteID2, 3))
		{
			GS()->Core()->SaveAccount(pPlayer, SAVE_PLANT_DATA);
			pPlayer->m_VotesData.UpdateVotesIf(MENU_UPGRADES);
		}
		return true;
	}

	return false;
}