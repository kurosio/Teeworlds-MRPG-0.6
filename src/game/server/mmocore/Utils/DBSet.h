#ifndef GAME_SERVER_MMO_UTILS_DBSET_H
#define GAME_SERVER_MMO_UTILS_DBSET_H

#include <string>
#include <list>

class DBSet
{
	std::string m_Data {};
	std::list < std::string > m_DataItems {};

public:
	DBSet() = default;
	explicit DBSet(const std::string& pData) { Init(pData); }

	void Init(const std::string& pData)
	{
		m_Data = pData;
		if(!pData.empty())
		{
			size_t start = 0;
			size_t end;
			std::string delim = ",";

			while((end = pData.find(delim, start)) != std::string::npos)
			{
				m_DataItems.push_back(pData.substr(start, end - start));
				start = end + 1;
			}
			m_DataItems.push_back(pData.substr(start));
		}
	}

	bool hasSet(const char* pSet) const
	{
		return std::find(m_DataItems.begin(), m_DataItems.end(), pSet) != m_DataItems.end();
	}
	std::list<std::string>& GetDataItems() { return m_DataItems; }
};

#endif //GAME_SERVER_MMO_UTILS_DBSET_H
