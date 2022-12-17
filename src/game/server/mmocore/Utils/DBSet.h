#ifndef GAME_SERVER_MMO_UTILS_DBSET_H
#define GAME_SERVER_MMO_UTILS_DBSET_H

#include <string>
#include <list>

class DBSet
{
	std::string m_Data{};
	std::list < std::string > m_DataItems{};

public:
	DBSet() = default;
	explicit DBSet(const std::string& pData) { Init(pData); }

	void Init(const std::string& pData)
	{
		m_Data = pData;
		if(!pData.empty())
		{
			size_t start;
			size_t end = 0;
			std::string delim = ",";

			while((start = pData.find_first_not_of(delim, end)) != std::string::npos)
			{
				end = pData.find(delim, start);
				m_DataItems.push_back(pData.substr(start, end - start));
			}
		}
	}

	bool hasSet(const char* pSet) const
	{
		auto p = std::find_if(m_DataItems.begin(), m_DataItems.end(), [pSet](const std::string& item) { return item == pSet; });
		return p != m_DataItems.end();
	}
	std::list<std::string>& GetDataItems() { return m_DataItems; }
};

#endif //GAME_SERVER_MMO_UTILS_DBSET_H
