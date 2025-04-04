#include "wiki_manager.h"

#include <engine/shared/linereader.h>
#include <game/server/gamecontext.h>

void CWikiManager::OnPreInit()
{
	CLineReader Reader;
	if(!Reader.OpenFile(GS()->Storage()->OpenFile("server_data/wiki_content.txt", IOFLAG_READ, IStorageEngine::TYPE_ABSOLUTE)))
		return;

	// initialize variables
	std::string CurrentTitle;
	std::string CurrentContent;
	int incrementWikiID = 1;

	// parse wiki information
	for(const char* pLine = Reader.Get(); pLine; pLine = Reader.Get())
	{
		if(str_startswith(pLine, "#"))
		{
			if(!CurrentTitle.empty() && !CurrentContent.empty())
			{
				CWikiData::CreateElement(incrementWikiID++)->Init(CurrentTitle, CurrentContent);
				CurrentContent.clear();
			}

			CurrentTitle = pLine + 1;
		}
		else
		{
			CurrentContent += pLine;
			CurrentContent += "\n";
		}
	}

	// add last element
	if(!CurrentTitle.empty() && !CurrentContent.empty())
	{
		CWikiData::CreateElement(incrementWikiID++)->Init(CurrentTitle, CurrentContent);
	}
}

void CWikiManager::OnConsoleInit()
{
	Console()->Register("reload_wiki", "", CFGFLAG_SERVER, ConReloadWiki, this, "Reload wiki information");
}

bool CWikiManager::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	const auto ClientID = pPlayer->GetCID();

	// motd wiki information
	if(Menulist == MOTD_MENU_WIKI_INFO)
	{
		MotdMenu Menu(ClientID, MTFLAG_CLOSE_BUTTON);
		for(const auto& pData : CWikiData::Data())
		{
			Menu.AddMenu(MOTD_MENU_WIKI_SELECT, pData->GetID(), pData->GetTitle());
		}
		Menu.Send(MOTD_MENU_WIKI_INFO);

		return true;
	}

	// motd wiki select
	if(Menulist == MOTD_MENU_WIKI_SELECT)
	{
		pPlayer->m_pMotdMenu->SetLastMenulist(MOTD_MENU_WIKI_INFO);

		std::string Content = "Invalid wiki ID";
		const auto MotdSelect = pPlayer->m_pMotdMenu->GetMenuExtra();

		if(const auto pWikiData = GetWikiData(MotdSelect.value_or(NOPE)))
		{
			Content = pWikiData->GetContent();
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

void CWikiManager::ConReloadWiki(IConsole::IResult*, void* pUserData)
{
	const auto pSelf = static_cast<CWikiManager*>(pUserData);

	mystd::freeContainer(CWikiData::Data());
	pSelf->OnPreInit();
	dbg_msg("wiki", "Wiki information reloaded");
}
