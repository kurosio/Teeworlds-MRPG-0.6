/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountMiningManager.h"

#include <engine/server.h>
#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

std::map < int , CAccountMiningManager::MiningPoint > CAccountMiningManager::ms_vmMiningPoints;

void CAccountMiningManager::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_positions_mining", pWhereLocalWorld);
	while (pRes->next())
	{
		// initialize variables
		MiningPoint Point;
		Point.m_ItemID = pRes->getInt("ItemID");
		Point.m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		Point.m_Distance = pRes->getInt("Distance");
		Point.m_WorldID = pRes->getInt("WorldID");

		// insert to game base
		const int ID = pRes->getInt("ID");
		ms_vmMiningPoints[ID] = std::move(Point);
	}
}

void CAccountMiningManager::OnPlayerLogin(CPlayer* pPlayer)
{
	// try load from database
	auto& refMiningDbField = pPlayer->Account()->m_MiningData;
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_mining", "WHERE UserID = '{}'", pPlayer->Account()->GetID());
	if (pRes->next())
	{
		refMiningDbField.initFields(&pRes);
		return;
	}

	// set default values
	refMiningDbField.getRef<int>(JOB_LEVEL) = 1;
	refMiningDbField.getRef<uint64_t>(JOB_EXPERIENCE) = 0;
	refMiningDbField.getRef<int>(JOB_UPGRADES) = 0;
	Database->Execute<DB::INSERT>("tw_accounts_mining", "(UserID) VALUES ('{}')", pPlayer->Account()->GetID());
}

CItemDescription* CAccountMiningManager::GetMiningItemInfoByPos(vec2 Pos) const
{
	// search ore by position
	auto Iter = std::find_if(ms_vmMiningPoints.begin(), ms_vmMiningPoints.end(), [Pos](auto& p)
	{
		return distance(p.second.m_Position, Pos) < p.second.m_Distance;
	});
	return (Iter != ms_vmMiningPoints.end()) ? GS()->GetItemInfo(Iter->second.m_ItemID) : nullptr;;
}

bool CAccountMiningManager::InsertItemsDetailVotes(CPlayer* pPlayer, int WorldID) const
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();

	for(const auto& [ID, Ore] : ms_vmMiningPoints)
	{
		if(WorldID != Ore.m_WorldID)
			continue;

		const vec2 Pos = Ore.m_Position / 32.0f;
		VoteWrapper VOres(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "Ore {}", GS()->GetItemInfo(Ore.m_ItemID)->GetName());
		VOres.MarkList().Add("Location:");
		{
			VOres.BeginDepth();
			VOres.Add(Instance::Localize(ClientID, Instance::Server()->GetWorldName(WorldID)));
			VOres.Add("x{} y{}", (int)Pos.x, (int)Pos.y);
			VOres.EndDepth();
		}
		VOres.AddLine();
		VOres.MarkList().Add("Description");
		{
			VOres.BeginDepth();
			//VOres.Add("Level: {}", Ore.Level);
			//VOres.Add("Health: {}P", Ore.m_StartHealth);
			VOres.Add("Distance of distribution: {}P", Ore.m_Distance);
			VOres.EndDepth();
		}
		VOres.AddLine();
		Found = true;
	}

	return Found;
}

void CAccountMiningManager::Process(CPlayer *pPlayer, int Level) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const int MultiplierExperience = maximum(1, (int)computeExperience(Level) / g_Config.m_SvMiningLevelIncrease);

	int& refLevel = pPlayer->Account()->m_MiningData.getRef<int>(JOB_LEVEL);
	auto& refExperience = pPlayer->Account()->m_MiningData.getRef<uint64_t>(JOB_EXPERIENCE);
	int& refUpgrade = pPlayer->Account()->m_MiningData.getRef<int>(JOB_UPGRADES);

	// append experience
	refExperience += MultiplierExperience;

	// check level up
	auto ExperienceNeed = computeExperience(refLevel);
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
			GS()->EntityManager()->Text(pPlayer->GetCharacter()->m_Core.m_Pos + vec2(0, -40), 40, "miner up");
		}
		GS()->Chat(ClientID, "Miner Level UP. Now Level {}!", refLevel);
	}

	// progress bar
	pPlayer->ProgressBar("Miner", refLevel, refExperience, ExperienceNeed, MultiplierExperience);
	Core()->SaveAccount(pPlayer, SAVE_MINING_DATA);
}


bool CAccountMiningManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	if (PPSTR(pCmd, "MINING_UPGRADE") == 0)
	{
		auto* pUpgrades = &pPlayer->Account()->m_MiningData.getRef<int>(JOB_UPGRADES);
		auto* pSelectedUpgr = &pPlayer->Account()->m_MiningData.getRef<int>(Extra1);

		if (pPlayer->Upgrade(ReasonNumber, pSelectedUpgr, pUpgrades, Extra2, 3))
		{
			GS()->Core()->SaveAccount(pPlayer, SAVE_MINING_DATA);
			pPlayer->m_VotesData.UpdateVotesIf(MENU_UPGRADES);
		}
		return true;
	}

	return false;
}