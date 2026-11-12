/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "EidolonManager.h"

#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>
#include <game/server/core/components/accounts/profession.h>

namespace
{
	bool ReadProgress(const CPlayerItem* pItem, nlohmann::json& Data, int& Level, uint64_t& Experience, int& Points)
	{
		Data = pItem->GetStringData().empty() ? nlohmann::json::object() : nlohmann::json::parse(pItem->GetStringData(), nullptr, false);
		if(!Data.is_object())
			return false;
		try
		{
			Level = Data.value("level", 1);
			Experience = Data.value("exp", uint64_t(0));
			Points = Data.value("up", 0);
			return Level >= 1 && Points >= 0;
		}
		catch(const nlohmann::json::exception&)
		{
			return false;
		}
	}
}

void CEidolonManager::OnPreInit()
{
	m_Events.Init(this, IEventListener::PlayerProfessionExperience);
	m_Events.Register();
}

void CEidolonManager::OnPlayerProfessionExperience(CPlayer* pPlayer, CProfession* pProfession, uint64_t& Experience)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !pProfession || !pProfession->IsProfessionType(PROFESSION_TYPE_WAR) || !Experience)
		return;

	auto* pEidolon = pPlayer->GetEidolon();
	if(!pEidolon || !pEidolon->GetCharacter() || !pEidolon->GetCharacter()->IsAlive())
		return;

	auto* pItem = pPlayer->GetItem(pEidolon->m_EidolonItemID);
	if(!pItem->HasItem() || !pItem->IsEquipped())
		return;

	nlohmann::json Data;
	int Level, Points;
	uint64_t Current;
	if(!ReadProgress(pItem, Data, Level, Current, Points))
		return;

	uint64_t Remaining = Experience;
	const int OldLevel = Level;
	while(Remaining >= computeExperience(Level) - Current)
	{
		Remaining -= computeExperience(Level) - Current;
		Current = 0;
		++Level;
		if(Points < std::numeric_limits<int>::max())
			++Points;
	}

	const uint64_t Room = computeExperience(Level) - 1 - Current;
	Current += std::min(Remaining, Room);
	Data["level"] = Level;
	Data["exp"] = Current;
	Data["up"] = Points;

	if(pItem->SetStringData(Data.dump()) && Level > OldLevel)
		static_cast<CGS*>(Instance::GameServerPlayer(pPlayer->GetCID()))->Chat(pPlayer->GetCID(), "Your eidolon reached level {}!", Level);
}

bool CEidolonManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(pCmd, "EIDOLON_SELECT") == 0)
	{
        const int EidolonID = GetIfExists<int>(Extras, 0, NOPE);
		m_EidolonItemSelected[ClientID] = EidolonID;
		pPlayer->m_VotesData.UpdateVotes(MENU_EIDOLON_SELECT);
		return true;
	}

	return false;
}

bool CEidolonManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_EIDOLON)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		VoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "Eidolon Collection Information");
		VInfo.Add("Here you can see your collection of eidolons.");
		VInfo.AddLine();

		std::pair EidolonSize = GetEidolonsSize(ClientID);
		VoteWrapper VEidolon(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "\u2727 My eidolons (own {} out of {}).", EidolonSize.first, EidolonSize.second);

		for(auto& pEidolon : CEidolonInfoData::Data())
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pEidolon.GetItemID());
			const char* pCollectedInfo = (pPlayerItem->HasItem() ? "✔" : "\0");
			const char* pUsedAtMoment = pPlayerItem->IsEquipped() ? Instance::Localize(pPlayer->GetCID(), "[summoned by you]") : "\0";
			VEidolon.AddMenu(MENU_EIDOLON_SELECT, pEidolon.GetItemID(), "{} {} {}", pEidolon.GetDataBot()->m_aNameBot, pCollectedInfo, pUsedAtMoment);
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_EIDOLON_SELECT)
	{
		const auto EidolonID = pPlayer->m_VotesData.GetExtraID();
		pPlayer->m_VotesData.SetLastMenuID(MENU_EIDOLON);

		// check is selection valid
		if(!EidolonID.has_value())
		{
			VoteWrapper::AddBackpage(ClientID);
			return true;
		}

		// check valid eidolon info
		const auto* pEidolonInfo = GS()->GetEidolonByItemID(EidolonID.value());
		if(!pEidolonInfo)
		{
			VoteWrapper::AddBackpage(ClientID);
			return true;
		}

		// description about eidolon
		const auto* pPlayerItem = pPlayer->GetItem(pEidolonInfo->GetItemID());
		VoteWrapper VDesc(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Descriptions of eidolon ({})", pEidolonInfo->GetDataBot()->m_aNameBot);
		for(auto& Line : pEidolonInfo->GetLinesDescription())
		{
			VDesc.Add(Line.c_str());
		}

		VDesc.AddLine();

		if(pPlayerItem->Info()->HasAttributes())
		{
			VDesc.Add("{~}", pPlayerItem->GetStringAttributesInfo(pPlayer).c_str());
		}

		VDesc.AddLine();

		// enchancements
		VoteWrapper VEnchancement(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Unlocking Enhancements");
		VEnchancement.Add("Available soon.");
		VEnchancement.AddLine();

		// summoning
		if(pPlayerItem->HasItem())
		{
			const char* pStateSummon = Instance::Localize(pPlayer->GetCID(), pPlayerItem->IsEquipped() ? "Call off the summoned" : "Summon");
			VoteWrapper(ClientID).AddOption("TOGGLE_EQUIP", pEidolonInfo->GetItemID(), "{} {}", pStateSummon, pEidolonInfo->GetDataBot()->m_aNameBot);
		}
		else
		{
			VoteWrapper(ClientID).Add("To summon it, you must first get it");
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

std::pair<int, int> CEidolonManager::GetEidolonsSize(int ClientID) const
{
	int Collect = 0;
	int Max = static_cast<int>(CEidolonInfoData::Data().size());

	if(CPlayerItem::Data().find(ClientID) != CPlayerItem::Data().end())
	{
		for(auto& p : CPlayerItem::Data()[ClientID])
		{
			if(p.second.HasItem() && p.second.Info()->IsGroup(ItemGroup::Equipment) && p.second.Info()->IsType(ItemType::EquipEidolon))
				Collect++;
		}
	}

	return std::make_pair(Collect, Max);
}
