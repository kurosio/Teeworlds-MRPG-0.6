#ifndef GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H
#define GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H

/*
Need to update the implementation to something like nested containers to support group hierarchy
*/
namespace mystd
{
	struct string_hash
	{
		using is_transparent = void;
		[[nodiscard]] size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
	};

	template <typename T>
	class grouped_container
	{
	public:
		using ItemList = std::vector<T*>;
		using SubgroupMap = std::unordered_map<std::string, ItemList, string_hash, std::equal_to<>>;
		using GroupMap = std::unordered_map<std::string, SubgroupMap, string_hash, std::equal_to<>>;

	private:
		GroupMap m_Data;
		std::string m_DefaultSubgroupKey;

	public:
		// constructor
		explicit grouped_container(std::string defaultSubgroupKeyVal = "Uncategorized")
			: m_DefaultSubgroupKey(std::move(defaultSubgroupKeyVal))
		{
			m_DefaultSubgroupKey = defaultSubgroupKeyVal.empty() ? "Uncategorized" : defaultSubgroupKeyVal;
		}

		void set_default_subgroup_key(std::string_view key)
		{
			m_DefaultSubgroupKey = key.empty() ? "Uncategorized" : key;
		}

		[[nodiscard]] const std::string& get_default_subgroup_key() const noexcept
		{
			return m_DefaultSubgroupKey;
		}

		// inserting
		void add_item(std::string_view group, std::string_view subgroup, T* pItem)
		{
			if(!pItem)
				return;

			auto groupIter = m_Data.find(group);
			if(groupIter == m_Data.end())
				groupIter = m_Data.emplace(std::string(group), SubgroupMap {}).first;

			auto& subgroupMap = groupIter->second;
			std::string_view actualSubgroup = subgroup.empty() ? std::string_view(m_DefaultSubgroupKey) : subgroup;
			auto subgroupIter = subgroupMap.find(actualSubgroup);
			if(subgroupIter == subgroupMap.end())
				subgroupIter = subgroupMap.emplace(std::string(actualSubgroup), ItemList {}).first;

			subgroupIter->second.push_back(pItem);
		}

		void add_item(std::string_view group, T* pItem)
		{
			add_item(group, m_DefaultSubgroupKey, pItem);
		}

		// getters items
		[[nodiscard]] ItemList* get_items(std::string_view group, std::string_view subgroup)
		{
			return const_cast<ItemList*>(std::as_const(*this).get_items(group, subgroup));
		}

		[[nodiscard]] const ItemList* get_items(std::string_view group, std::string_view subgroup) const
		{
			if(auto groupIter = m_Data.find(group); groupIter != m_Data.end())
			{
				const auto& subgroupMap = groupIter->second;
				if(auto subgroupIter = subgroupMap.find(subgroup); subgroupIter != subgroupMap.end())
					return &subgroupIter->second;
			}

			return nullptr;
		}

		[[nodiscard]] SubgroupMap* get_subgroups(std::string_view group)
		{
			return const_cast<SubgroupMap*>(std::as_const(*this).get_subgroups(group));
		}

		[[nodiscard]] const SubgroupMap* get_subgroups(std::string_view group) const
		{
			if(auto groupIter = m_Data.find(group); groupIter != m_Data.end())
				return &groupIter->second;

			return nullptr;
		}

		[[nodiscard]] const GroupMap& get_all_data() const noexcept
		{
			return m_Data;
		}

		[[nodiscard]] GroupMap& get_all_data() noexcept
		{
			return m_Data;
		}

		[[nodiscard]] auto get_group_keys() const
		{
			return m_Data | std::views::keys;
		}

		[[nodiscard]] std::vector<std::string> get_subgroup_keys(std::string_view group) const
		{
			if(const auto* pSubgroupMap = get_subgroups(group))
			{
				std::vector<std::string> keys;
				keys.reserve(pSubgroupMap->size());
				for(const auto& key : *pSubgroupMap | std::views::keys)
					keys.push_back(key);

				return keys;
			}
			return {};
		}

		// groups checking
		[[nodiscard]] bool has_group(std::string_view group) const
		{
			return m_Data.contains(group);
		}

		[[nodiscard]] bool has_subgroup(std::string_view group, std::string_view subgroup) const
		{
			if(auto groupIter = m_Data.find(group); groupIter != m_Data.end())
				return groupIter->second.contains(subgroup);

			return false;
		}
		[[nodiscard]] bool group_has_only_default_subgroup(std::string_view groupName) const
		{
			const auto* pSubgroupMap = get_subgroups(groupName);
			if(!pSubgroupMap || pSubgroupMap->size() != 1)
				return false;

			return pSubgroupMap->contains(m_DefaultSubgroupKey);
		}

		// counting
		[[nodiscard]] bool is_empty() const noexcept
		{
			return m_Data.empty();
		}

		[[nodiscard]] size_t get_group_count() const noexcept
		{
			return m_Data.size();
		}

		[[nodiscard]] size_t get_subgroup_count(std::string_view groupName) const
		{
			const auto* pSubgroupMap = get_subgroups(groupName);
			return pSubgroupMap ? pSubgroupMap->size() : 0;
		}

		[[nodiscard]] size_t get_item_count(std::string_view groupName, std::string_view subgroupName) const
		{
			const auto* pItems = get_items(groupName, subgroupName);
			return pItems ? pItems->size() : 0;
		}

		[[nodiscard]] size_t get_item_group_count(std::string_view groupName) const
		{
			const SubgroupMap* pSubgroupMap = get_subgroups(groupName);
			if(!pSubgroupMap)
				return 0;

			auto itemCounts = *pSubgroupMap | std::views::values | std::views::transform(&ItemList::size);
			return std::accumulate(itemCounts.begin(), itemCounts.end(), size_t { 0 });
		}

		// clear
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
			if(auto groupIter = m_Data.find(group); groupIter != m_Data.end())
			{
				groupIter->second.erase(std::string(subgroup));
				if(groupIter->second.empty())
					m_Data.erase(groupIter);
			}
		}

		// other
		void sort_all_items(const std::function<bool(const T* a, const T* b)>& comparator)
		{
			for(auto& groupPair : m_Data)
			{
				for(auto& subgroupPair : groupPair.second)
					std::ranges::sort(subgroupPair.second, comparator);
			}
		}
	};
};

#endif // GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H