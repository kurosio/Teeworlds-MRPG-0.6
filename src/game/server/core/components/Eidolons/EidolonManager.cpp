/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "EidolonManager.h"

#include <game/server/gamecontext.h>

using namespace sqlstr;

#define TW_TABLE_EIDOLON_ENHANCEMENTS "tw_account_eidolon_enhancements"

bool CEidolonManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(pCmd, "EIDOLON_SELECT") == 0)
	{
		m_EidolonItemSelected[ClientID] = Extra1;

		//pPlayer->m_TempMenuValue = MENU_EIDOLON_SELECT;
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
			VDesc.Add(pPlayerItem->GetStringAttributesInfo(pPlayer).c_str());
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
			VoteWrapper(ClientID).AddOption("TOGGLE_EQUIP", pEidolonInfo->GetItemID(), NOPE, "{} {}", pStateSummon, pEidolonInfo->GetDataBot()->m_aNameBot);
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
