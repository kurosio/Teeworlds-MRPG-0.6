#ifndef GAME_SERVER_MMOCORE_UTILS_DBSET_H
#define GAME_SERVER_MMOCORE_UTILS_DBSET_H

#include <string>

class DBSet
{
	std::string m_Data {};
	ska::flat_hash_map<std::string, size_t> m_DataItems {};

public:
	DBSet() = default;

	DBSet(std::string_view Data) : m_Data(Data)
	{
		Init();
	}

	bool operator&(const size_t& flag) const
	{
		return std::any_of(m_DataItems.begin(), m_DataItems.end(), [flag](const auto& item)
		{
			const auto& [key, value] = item;
			return flag & value;
		});
	}

	DBSet& operator=(std::string_view set)
	{
		DBSet tmp(set);
		*this = std::move(tmp);
		return *this;
	}

	std::string Dump() const { return m_Data; }
	bool hasSet(const std::string& pSet) const { return m_DataItems.find(pSet) != m_DataItems.end(); }
	const ska::flat_hash_map<std::string, size_t>& GetDataItems() const { return m_DataItems; }

private:
	void Init()
	{
		if(m_Data.empty()) return;

		const std::string_view delimiter = ",";
		size_t iteration = 0;

		m_DataItems.reserve(m_Data.length() / delimiter.length() + 1);

		auto split_view = m_Data | std::views::split(delimiter);
		for(auto token_range : split_view)
		{
			std::string token(token_range.begin(), token_range.end());

			token.erase(token.find_last_not_of(' ') + 1);
			token.erase(0, token.find_first_not_of(' '));

			if(!token.empty())
			{
				m_DataItems[token] = (size_t)1 << iteration++;
			}
		}
	}
};

#endif //GAME_SERVER_MMO_UTILS_DBSET_H
