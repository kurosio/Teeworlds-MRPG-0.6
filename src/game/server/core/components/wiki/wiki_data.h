#ifndef GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_DATA_H

class CWikiData : public MultiworldIdentifiableData<std::deque<std::shared_ptr<CWikiData>>>
{
	int m_ID {};
	std::string m_Title {};
	std::string m_Content {};

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

	int GetID() const noexcept
	{
		return m_ID;
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