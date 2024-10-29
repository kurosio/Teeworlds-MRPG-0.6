#include "wiki_manager.h"

#include <game/server/gamecontext.h>

void CWikiManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_wiki_content");
	while(pRes->next())
	{
		// initialize variables
		const auto ID = pRes->getInt("ID");
		const auto Title = pRes->getString("Title");
		const auto Content = pRes->getString("Content");

		// create new element
		CWikiData::CreateElement(ID)->Init(Title, Content);
	}
}

bool CWikiManager::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	const auto ClientID = pPlayer->GetCID();

	if(Menulist == MOTD_MENU_WIKI_INFO)
	{
		MotdMenu Menu(ClientID);
		for(const auto& pData : CWikiData::Data())
		{
			Menu.AddMenu(MOTD_MENU_WIKI_SELECT, pData->GetID(), pData->GetTitle());
		}
		Menu.Send(MOTD_MENU_WIKI_INFO);

		return true;
	}

	if(Menulist == MOTD_MENU_WIKI_SELECT)
	{
		pPlayer->m_pMotdMenu->SetLastMenulist(MOTD_MENU_WIKI_INFO);

		std::string Content = "Invalid wiki ID";
		const auto MotdSelect = pPlayer->m_pMotdMenu->GetMenuExtra();

		if(MotdSelect.has_value())
		{
			if(const auto pWikiData = GetWikiData(MotdSelect.value()))
			{
				Content = pWikiData->GetContent();
			}
		}

		MotdMenu Menu(ClientID, Content.c_str());
		Menu.AddBackpage();
		Menu.Send(MOTD_MENU_WIKI_SELECT);
		return true;
	}

	return false;
}

CWikiData* CWikiManager::GetWikiData(int ID) const
{
	const auto iter = std::ranges::find_if(CWikiData::Data(), [ID](const auto pData)
	{
		return pData->GetID() == ID;
	});
	return iter != CWikiData::Data().end() ? iter->get() : nullptr;
}
