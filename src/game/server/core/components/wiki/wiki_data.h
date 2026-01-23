#ifndef GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_DATA_H

class CWikiData : public MultiworldIdentifiableData<std::deque<std::shared_ptr<CWikiData>>>
{
	int m_ID {};
	int m_ParentID { NOPE };
	std::string m_Title {};
	std::string m_Content {};
	std::vector<int> m_Children {};

public:
	CWikiData() = default;

	static std::shared_ptr<CWikiData> CreateElement(int ID)
	{
		auto pData = std::make_shared<CWikiData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(pData);
	}

	void Init(const std::string& Title, const std::string& Content)
	{
		m_Title.assign(Title);
		m_Content.assign(Content);
	}

	void SetParentID(int ParentID) noexcept
	{
		m_ParentID = ParentID;
	}

	void SetContent(const std::string& Content)
	{
		m_Content.assign(Content);
	}

	void AddChild(int ChildID)
	{
		if(std::ranges::find(m_Children, ChildID) == m_Children.end())
			m_Children.push_back(ChildID);
	}

	int GetID() const noexcept
	{
		return m_ID;
	}

	int GetParentID() const noexcept
	{
		return m_ParentID;
	}

	const std::vector<int>& GetChildren() const noexcept
	{
		return m_Children;
	}

	const std::string& GetTitle() const noexcept
	{
		return m_Title;
	}

	const std::string& GetContent() const noexcept
	{
		return m_Content;
	}
};

#endif
