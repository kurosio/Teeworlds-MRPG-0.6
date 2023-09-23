#ifndef GAME_SERVER_MMO_UTILS_DBSET_H
#define GAME_SERVER_MMO_UTILS_DBSET_H

#include <string>
#include <list>

class DBSet
{
	std::string m_Data;
	std::vector<std::string> m_DataItems;

public:
	// Default constructor
	DBSet() = default;

	// Parameterized constructor
	explicit DBSet(const std::string& pData) { Init(pData); }

	// Initialize the DBSet with data
	void Init(std::string pData)
	{
		m_Data = std::move(pData);
		if(!m_Data.empty())
		{
			size_t start = 0;
			size_t end;
			std::string delim = ",";

			// Split m_Data into m_DataItems using delimiter ','
			while((end = m_Data.find(delim, start)) != std::string::npos)
			{
				m_DataItems.push_back(m_Data.substr(start, end - start));
				start = end + 1;
			}
			m_DataItems.push_back(m_Data.substr(start));
		}
	}

	// Check if a set exists in the DBSet
	bool hasSet(const char* pSet) const
	{
		return std::find(m_DataItems.begin(), m_DataItems.end(), pSet) != m_DataItems.end();
	}

	// Get the data items in the DBSet
	const std::vector<std::string>& GetDataItems() const { return m_DataItems; }
};

#endif //GAME_SERVER_MMO_UTILS_DBSET_H
