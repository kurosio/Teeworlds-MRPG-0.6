/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountFarmingManager.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

std::map < int , CAccountFarmingManager::FarmingPoint > CAccountFarmingManager::ms_vmFarmingPoints;

void CAccountFarmingManager::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_positions_farming", pWhereLocalWorld);
	while(pRes->next())
	{
		// initialize variables
		FarmingPoint Point;
		Point.m_ItemID = pRes->getInt("ItemID");
		Point.m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		Point.m_Distance = pRes->getInt("Distance");
		Point.m_WorldID = pRes->getInt("WorldID");

		// insert to game base
		const int ID = pRes->getInt("ID");
		ms_vmFarmingPoints[ID] = std::move(Point);
	}
}

void CAccountFarmingManager::OnPlayerLogin(CPlayer *pPlayer)
{
	// try load from database
	auto& refFarmingDbField = pPlayer->Account()->m_FarmingData;
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_farming", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	if(pRes->next())
	{
		refFarmingDbField.initFields(&pRes);
		return;
	}

	// set default values
	refFarmingDbField(JOB_LEVEL, 1).m_Value = 1;
	refFarmingDbField(JOB_EXPERIENCE, 0).m_Value = 0;
	refFarmingDbField(JOB_UPGRADES, 0).m_Value = 0;
	Database->Execute<DB::INSERT>("tw_accounts_farming", "(UserID) VALUES ('%d')", pPlayer->Account()->GetID());
}

CItemDescription* CAccountFarmingManager::GetFarmingItemInfoByPos(vec2 Pos) const
{
	// search farming item by position
	auto Iter = std::find_if(ms_vmFarmingPoints.begin(), ms_vmFarmingPoints.end(), [Pos](auto& p)
	{
		return distance(p.second.m_Position, Pos) < p.second.m_Distance;
	});
	return (Iter != ms_vmFarmingPoints.end()) ? GS()->GetItemInfo(Iter->second.m_ItemID) : nullptr;;
}

bool CAccountFarmingManager::InsertItemsDetailVotes(CPlayer* pPlayer, int WorldID)
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();

	for(const auto& [ID, Point] : ms_vmFarmingPoints)
	{
		if(WorldID != Point.m_WorldID)
			continue;

		const vec2 Pos = Point.m_Position / 32.0f;
		VoteWrapper VInfo(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "Farm point {}", GS()->GetItemInfo(Point.m_ItemID)->GetName());
		VInfo.MarkList().Add("Location:");
		{
			VInfo.BeginDepth();
			VInfo.Add(Instance::Localize(ClientID, Instance::Server()->GetWorldName(WorldID)));
			VInfo.Add("x{} y{}", (int)Pos.x, (int)Pos.y);
			VInfo.EndDepth();
		}
		VInfo.AddLine();
		VInfo.MarkList().Add("Description");
		{
			VInfo.BeginDepth();
			VInfo.Add("Distance of distribution: {}P", Point.m_Distance);
			VInfo.EndDepth();
		}
		VInfo.AddLine();
		Found = true;
	}

	return Found;
}

void CAccountFarmingManager::Procces(CPlayer* pPlayer, int Level) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const int MultiplierExperience = maximum(1, (int)computeExperience(Level) / g_Config.m_SvFarmingIncreaseLevel);
	int& refLevel = pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value;
	int& refExperience = pPlayer->Account()->m_FarmingData(JOB_EXPERIENCE, 0).m_Value;
	int& refUpgrade = pPlayer->Account()->m_FarmingData(JOB_UPGRADES, 0).m_Value;

	// append experience
	refExperience += MultiplierExperience;

	// check level up
	int ExperienceNeed = computeExperience(refLevel);
	while(refExperience >= ExperienceNeed)
	{
		// implement level up
		refExperience -= ExperienceNeed;
		refLevel++;
		refUpgrade++;
		ExperienceNeed = computeExperience(refLevel);

		// visual effects and messages
		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
		{
			GS()->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, 4);
			GS()->CreateDeath(pPlayer->GetCharacter()->m_Core.m_Pos, ClientID);
			GS()->EntityManager()->Text(pPlayer->GetCharacter()->m_Core.m_Pos + vec2(0, -40), 40, "farming up");
		}
		GS()->Chat(ClientID, "Farming Level UP. Now Level {}!", refLevel);
	}

	// progress bar
	pPlayer->ProgressBar("Farming", refLevel, refExperience, ExperienceNeed, MultiplierExperience);
	Core()->SaveAccount(pPlayer, SAVE_FARMING_DATA);
}

bool CAccountFarmingManager::OnPlayerVoteCommand(CPlayer *pPlayer, const char *pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char *pReason)
{
	if(PPSTR(pCmd, "FARMING_UPGRADE") == 0)
	{
		if(pPlayer->Upgrade(ReasonNumber, &pPlayer->Account()->m_FarmingData(Extra1, 0).m_Value, &pPlayer->Account()->m_FarmingData(JOB_UPGRADES, 0).m_Value, Extra2, 3))
		{
			GS()->Core()->SaveAccount(pPlayer, SAVE_FARMING_DATA);
			pPlayer->m_VotesData.UpdateVotesIf(MENU_UPGRADES);
		}
		return true;
	}

	return false;
}