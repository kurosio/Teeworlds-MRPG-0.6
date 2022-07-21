/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountPlantCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>

std::map < int , CAccountPlantCore::StructPlants > CAccountPlantCore::ms_aPlants;

void CAccountPlantCore::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = SJK.SD("*", "tw_positions_farming", pWhereLocalWorld);
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		ms_aPlants[ID].m_ItemID = pRes->getInt("ItemID");
		ms_aPlants[ID].m_Level = pRes->getInt("Level");
		ms_aPlants[ID].m_PositionX = pRes->getInt("PositionX");
		ms_aPlants[ID].m_PositionY = pRes->getInt("PositionY");
		ms_aPlants[ID].m_Distance = pRes->getInt("Distance");
	}
}

void CAccountPlantCore::OnInitAccount(CPlayer *pPlayer)
{
	ResultPtr pRes = SJK.SD("*", "tw_accounts_farming", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	if(pRes->next())
	{
		for(int i = 0; i < NUM_JOB_ACCOUNTS_STATS; i++)
		{
			const char* pFieldName = pPlayer->Acc().m_aFarming[i].getFieldName();
			pPlayer->Acc().m_aFarming[i].m_Value = pRes->getInt(pFieldName);
		}
		return;
	}
	pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value = 1;
	pPlayer->Acc().m_aFarming[JOB_UPGR_QUANTITY].m_Value = 1;
	SJK.ID("tw_accounts_farming", "(UserID) VALUES ('%d')", pPlayer->Acc().m_UserID);
}

int CAccountPlantCore::GetPlantLevel(vec2 Pos) const
{
	for(const auto & Plant : ms_aPlants)
	{
		vec2 Position = vec2(Plant.second.m_PositionX, Plant.second.m_PositionY);
		if(distance(Position, Pos) < Plant.second.m_Distance)
			return Plant.second.m_Level;
	}
	return -1;
}

int CAccountPlantCore::GetPlantItemID(vec2 Pos) const
{
	for(const auto & Plant : ms_aPlants)
	{
		vec2 Position = vec2(Plant.second.m_PositionX, Plant.second.m_PositionY);
		if(distance(Position, Pos) < Plant.second.m_Distance)
			return Plant.second.m_ItemID;
	}
	return -1;
}

void CAccountPlantCore::ShowMenu(CPlayer* pPlayer) const
{
	const int ClientID = pPlayer->GetCID();
	const int JobLevel = pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value;
	const int JobExperience = pPlayer->Acc().m_aFarming[JOB_EXPERIENCE].m_Value;
	const int JobUpgrades = pPlayer->Acc().m_aFarming[JOB_UPGRADES].m_Value;
	const int JobUpgrQuantity = pPlayer->Acc().m_aFarming[JOB_UPGR_QUANTITY].m_Value;
	const int ExperienceNeed = computeExperience(JobLevel);

	GS()->AVM(ClientID, "null", NOPE, TAB_UPGR_JOB, "Plants Point: {INT} :: Level: {INT} Exp: {INT}/{INT}", JobUpgrades, JobLevel, JobExperience, ExperienceNeed);
	GS()->AVD(ClientID, "PLANTUPGRADE", JOB_UPGR_QUANTITY, 20, TAB_UPGR_JOB, "Quantity +{INT} (Price 20P)", JobUpgrQuantity);
}

void CAccountPlantCore::ShowPlantsItems(int ClientID) const
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	Job()->Item()->ListInventory(pPlayer, FUNCTION_PLANTS, true);
}

void CAccountPlantCore::Work(CPlayer* pPlayer, int Level)
{
	const int ClientID = pPlayer->GetCID();
	const int MultiplierExperience = computeExperience(Level) / g_Config.m_SvPlantingIncreaseLevel;
	pPlayer->Acc().m_aFarming[JOB_EXPERIENCE].m_Value += clamp(MultiplierExperience, 1, MultiplierExperience);

	int ExperienceNeed = computeExperience(pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value);
	for (; pPlayer->Acc().m_aFarming[JOB_EXPERIENCE].m_Value >= ExperienceNeed; )
	{
		pPlayer->Acc().m_aFarming[JOB_EXPERIENCE].m_Value -= ExperienceNeed;
		pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value++;
		pPlayer->Acc().m_aFarming[JOB_UPGRADES].m_Value++;

		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
		{
			GS()->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, 4);
			GS()->CreateDeath(pPlayer->GetCharacter()->m_Core.m_Pos, ClientID);
			GS()->CreateText(pPlayer->GetCharacter(), false, vec2(0, -40), vec2(0, -1), 40, "plants up");
		}

		ExperienceNeed = computeExperience(pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value);
		GS()->Chat(ClientID, "Plants Level UP. Now Level {INT}!", pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value);
	}

	pPlayer->ProgressBar("Plants", pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value, pPlayer->Acc().m_aFarming[JOB_EXPERIENCE].m_Value, ExperienceNeed, MultiplierExperience);
	Job()->SaveAccount(pPlayer, SAVE_PLANT_DATA);
}

bool CAccountPlantCore::OnHandleVoteCommands(CPlayer *pPlayer, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "PLANTUPGRADE") == 0)
	{
		if(pPlayer->Upgrade(Get, &pPlayer->Acc().m_aFarming[VoteID].m_Value, &pPlayer->Acc().m_aFarming[JOB_UPGRADES].m_Value, VoteID2, 3))
		{
			GS()->Mmo()->SaveAccount(pPlayer, SAVE_PLANT_DATA);
			GS()->StrongUpdateVotes(ClientID, MENU_UPGRADE);
		}
		return true;
	}

	return false;
}