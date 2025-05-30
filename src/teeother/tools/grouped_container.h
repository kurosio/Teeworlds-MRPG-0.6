#ifndef GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H
#define GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H

template <typename T>
class grouped_container
{
public:
	using ItemList = std::vector<T*>;
	using SubgroupMap = std::map<std::string, ItemList, std::less<>>;
	using GroupMap = std::map<std::string, SubgroupMap, std::less<>>;

private:
	GroupMap m_Data;
	std::string m_DefaultSubgroupKey;

public:
	explicit grouped_container(std::string defaultSubgroupKeyVal = "Uncategorized")
		: m_DefaultSubgroupKey(std::move(defaultSubgroupKeyVal))
	{
		if(m_DefaultSubgroupKey.empty())
		{
			m_DefaultSubgroupKey = "Uncategorized";
		}
	}

	void set_default_subgroup_key(std::string_view key)
	{
		m_DefaultSubgroupKey = key.empty() ? "Uncategorized" : std::string(key);
	}

	[[nodiscard]] const std::string& get_default_subgroup_key() const noexcept
	{
		return m_DefaultSubgroupKey;
	}

	void add_item(std::string_view group, std::string_view subgroup, T* item)
	{
		if(item == nullptr)
		{
			return;
		}

		std::string actual_subgroup = subgroup.empty() ? m_DefaultSubgroupKey : std::string(subgroup);
		m_Data[std::string(group)][actual_subgroup].push_back(item);
	}

	void add_item(std::string_view group, T* item)
	{
		add_item(group, m_DefaultSubgroupKey, item);
	}

	[[nodiscard]] ItemList* get_items(std::string_view group, std::string_view subgroup)
	{
		auto groupIt = m_Data.find(std::string(group));
		if(groupIt != m_Data.end())
		{
			auto subgroupIt = groupIt->second.find(std::string(subgroup));
			if(subgroupIt != groupIt->second.end())
			{
				return &subgroupIt->second;
			}
		}
		return nullptr;
	}

	[[nodiscard]] const ItemList* get_items(std::string_view group, std::string_view subgroup) const
	{
		auto groupIt = m_Data.find(std::string(group));
		if(groupIt != m_Data.end())
		{
			auto subgroupIt = groupIt->second.find(std::string(subgroup));
			if(subgroupIt != groupIt->second.end())
			{
				return &subgroupIt->second;
			}
		}
		return nullptr;
	}

	[[nodiscard]] SubgroupMap* get_subgroups(std::string_view group)
	{
		auto groupIt = m_Data.find(std::string(group));
		if(groupIt != m_Data.end())
		{
			return &groupIt->second;
		}
		return nullptr;
	}

	[[nodiscard]] const SubgroupMap* get_subgroups(std::string_view group) const
	{
		auto groupIt = m_Data.find(std::string(group));
		if(groupIt != m_Data.end())
		{
			return &groupIt->second;
		}
		return nullptr;
	}

	[[nodiscard]] GroupMap& get_all_data() noexcept
	{
		return m_Data;
	}

	[[nodiscard]] const GroupMap& get_all_data() const noexcept
	{
		return m_Data;
	}

	[[nodiscard]] std::vector<std::string> get_group_keys() const
	{
		std::vector<std::string> keys;
		keys.reserve(m_Data.size());
		for(const auto& pair : m_Data)
		{
			keys.push_back(pair.first);
		}
		return keys;
	}

	[[nodiscard]] std::vector<std::string> get_subgroup_keys(std::string_view group) const
	{
		std::vector<std::string> keys;
		const SubgroupMap* pSubGroupMap = get_subgroups(group);
		if(pSubGroupMap)
		{
			keys.reserve(pSubGroupMap->size());
			for(const auto& pair : *pSubGroupMap)
			{
				keys.push_back(pair.first);
			}
		}
		return keys;
	}

	[[nodiscard]] bool has_group(std::string_view group) const
	{
		return m_Data.contains(std::string(group));
	}

	[[nodiscard]] bool has_subgroup(std::string_view group, std::string_view subgroup) const
	{
		auto groupIt = m_Data.find(std::string(group));
		if(groupIt != m_Data.end())
		{
			return groupIt->second.count(std::string(subgroup)) > 0;
		}
		return false;
	}

	[[nodiscard]] bool group_has_only_default_subgroup(std::string_view groupName) const
	{
		const auto* pSubGroupMap = get_subgroups(groupName);
		if(!pSubGroupMap || pSubGroupMap->empty())
		{
			return true;
		}
		return (pSubGroupMap->size() == 1 && pSubGroupMap->begin()->first == m_DefaultSubgroupKey);
	}

	[[nodiscard]] bool is_empty() const noexcept
	{
		return m_Data.empty();
	}

	[[nodiscard]] size_t group_count() const noexcept
	{
		return m_Data.size();
	}

	[[nodiscard]] size_t subgroup_count(std::string_view groupName) const
	{
		const auto* pSubGroupMap = get_subgroups(groupName);
		return pSubGroupMap ? pSubGroupMap->size() : 0;
	}

	[[nodiscard]] size_t item_count(std::string_view groupName, std::string_view subgroupName) const
	{
		const auto* pItems = get_items(groupName, subgroupName);
		return pItems ? pItems->size() : 0;
	}

	void clear() noexcept
	{
		m_Data.clear();
	}

	void clear_group(std::string_view group)
	{
		m_Data.erase(std::string(group));
	}

	void clear_subgroup(std::string_view group, std::string_view subgroup)
	{
		auto groupIt = m_Data.find(std::string(group));
		if(groupIt != m_Data.end())
		{
			groupIt->second.erase(std::string(subgroup));
			if(groupIt->second.empty())
			{
				m_Data.erase(groupIt);
			}
		}
	}

	void sort_subgroup_items(std::string_view group, std::string_view subgroup, const std::function<bool(const T* a, const T* b)>& comparator)
	{
		ItemList* pItems = get_items(group, subgroup);
		if(pItems)
		{
			std::ranges::sort(*pItems, comparator);
		}
	}

	void sort_group_items(std::string_view group, const std::function<bool(const T* a, const T* b)>& comparator)
	{
		SubgroupMap* pSubgroups = get_subgroups(group);
		if(pSubgroups)
		{
			for(auto& pair : *pSubgroups)
			{
				std::ranges::sort(pair.second, comparator);
			}
		}
	}

	void sort_all_items(const std::function<bool(const T* a, const T* b)>& comparator)
	{
		for(auto& groupPair : m_Data)
		{
			for(auto& subgroupPair : groupPair.second)
			{
				std::ranges::sort(subgroupPair.second, comparator);
			}
		}
	}
};

#endif // GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H