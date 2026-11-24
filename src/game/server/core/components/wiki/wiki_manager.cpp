#include "wiki_manager.h"

#include <base/system.h>
#include <engine/server/async_file.h>
#include <engine/shared/linereader.h>
#include <game/server/gamecontext.h>

namespace
{
	bool IsBracketHeading(std::string_view Line, std::string_view& OutPath)
	{
		auto trimView = [](std::string_view text)
		{
			while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
			while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
			return text;
		};

		Line = trimView(Line);
		if(Line.size() >= 2 && Line.front() == '[' && Line.back() == ']')
		{
			OutPath = Line.substr(1, Line.size() - 2);
			return !trimView(OutPath).empty();
		}
		return false;
	}
}

void CWikiManager::OnPreInit()
{
	// startup: the content is part of the initialization and is read directly
	void* pData = nullptr;
	unsigned Size = 0;
	if(!GS()->Storage()->ReadFile("server_data/wiki_content.txt", IStorageEngine::TYPE_ABSOLUTE, &pData, &Size) || !pData)
		return;

	std::string Content(static_cast<const char*>(pData), Size);
	free(pData);
	ParseContent(Content);
}

void CWikiManager::ParseContent(const std::string& Content)
{
	// initialize variables
	std::string CurrentContent;
	int incrementWikiID = 1;
	std::unordered_map<std::string, int> PathToID;
	CWikiData* pCurrentNode = nullptr;

	// try create new node
	const auto GetOrCreateNode = [&](const std::vector<std::string>& Segments) -> CWikiData*
	{
		if(Segments.empty())
			return nullptr;

		int ParentID = NOPE;
		std::string Path;
		CWikiData* pNode = nullptr;

		for(const auto& Segment : Segments)
		{
			if(!Path.empty())
				Path.append("/");
			Path.append(Segment);

			auto it = PathToID.find(Path);
			if(it == PathToID.end())
			{
				auto pData = CWikiData::CreateElement(incrementWikiID++);
				pData->Init(Segment, "");
				pData->SetParentID(ParentID);
				pNode = pData.get();
				PathToID[Path] = pData->GetID();
				if(ParentID != NOPE)
				{
					if(auto pParent = GetWikiData(ParentID))
						pParent->AddChild(pData->GetID());
				}
			}
			else
				pNode = GetWikiData(it->second);

			if(pNode && ParentID != NOPE && pNode->GetParentID() == NOPE)
			{
				pNode->SetParentID(ParentID);
				if(auto pParent = GetWikiData(ParentID))
					pParent->AddChild(pNode->GetID());
			}

			ParentID = pNode ? pNode->GetID() : NOPE;
		}

		return pNode;
	};

	// lambda for install content to node
	const auto FinalizeNodeContent = [&]()
	{
		if(pCurrentNode)
			pCurrentNode->SetContent(CurrentContent);
		CurrentContent.clear();
	};

	// parse wiki information
	const char* pPosition = Content.c_str();
	while(pPosition && *pPosition)
	{
		const char* pLineEnd = str_find(pPosition, "\n");
		std::string Line = pLineEnd ? std::string(pPosition, pLineEnd - pPosition) : std::string(pPosition);
		pPosition = pLineEnd ? pLineEnd + 1 : nullptr;

		const auto TrimmedLine = mystd::string::trim(Line);
		std::string_view HeadingPath;

		if(IsBracketHeading(TrimmedLine, HeadingPath))
		{
			FinalizeNodeContent();
			pCurrentNode = GetOrCreateNode(mystd::string::split_array_by_delimiter(HeadingPath, '/'));
		}
		else if(!TrimmedLine.empty() && TrimmedLine.front() == '#')
		{
			FinalizeNodeContent();
			std::string_view TitleView = TrimmedLine;
			TitleView.remove_prefix(1);
			const auto TrimmedTitle = mystd::string::trim(TitleView);
			pCurrentNode = GetOrCreateNode({ std::string(TrimmedTitle) });
		}
		else
		{
			CurrentContent += Line;
			CurrentContent += "\n";
		}
	}

	// add last element
	FinalizeNodeContent();
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
		MotdMenu Menu(ClientID);
		for(const auto& pData : CWikiData::Data())
		{
			if(pData->GetParentID() == NOPE)
				Menu.AddMenu(MOTD_MENU_WIKI_SELECT, pData->GetID(), pData->GetTitle());
		}
		Menu.AddSeparateLine();
		Menu.AddBackpage();
		Menu.AddSeparateLine();
		Menu.Send(MOTD_MENU_WIKI_INFO);
		return true;
	}

	// motd wiki select
	if(Menulist == MOTD_MENU_WIKI_SELECT)
	{
		const auto MotdSelect = pPlayer->m_pMotdMenu->GetMenuExtra();
		const auto pWikiData = MotdSelect ? GetWikiData(MotdSelect.value()) : nullptr;

		MotdMenu Menu(ClientID, pWikiData ? pWikiData->GetContent().c_str() : "Invalid wiki ID");
		if(pWikiData)
		{
			for(const auto ChildID : pWikiData->GetChildren())
			{
				if(const auto pChild = GetWikiData(ChildID))
					Menu.AddMenu(MOTD_MENU_WIKI_SELECT, pChild->GetID(), pChild->GetTitle());
			}
		}
		Menu.AddSeparateLine();
		Menu.AddBackpage();
		Menu.AddSeparateLine();
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
	static_cast<CWikiManager*>(pUserData)->Reload();
}

void CWikiManager::Reload()
{
	// the file is read in the background, the tree is rebuilt on the game thread
	AsyncFile::Read(GS()->Storage(), "server_data/wiki_content.txt", IStorageEngine::TYPE_ABSOLUTE,
		[this](AsyncFile::SReadResult&& Read)
	{
		if(!Read.Ok())
		{
			dbg_msg("wiki", "Wiki information is not available: %s", Read.m_Error.c_str());
			return;
		}
		if(Read.m_Content.empty())
		{
			dbg_msg("wiki", "Wiki information is not available");
			return;
		}

		mystd::freeContainer(CWikiData::Data());
		ParseContent(Read.m_Content);
		dbg_msg("wiki", "Wiki information reloaded");
	});
}
