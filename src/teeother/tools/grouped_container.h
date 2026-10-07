#ifndef GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H
#define GAME_SERVER_MMO_UTILS_GROUPED_CONTAINER_H

#include <teeother/tools/utilities.h>

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

	inline std::pair<std::string, std::string> parse_group_name(std::string_view value,
		std::string_view defaultSubgroup = "Uncategorized")
	{
		std::string text(value);
		if(text.empty())
			return {};

		if(text.front() == '|')
		{
			const auto mainEnd = text.find('|', 1);
			if(mainEnd != std::string::npos)
			{
				const std::string mainGroup = text.substr(1, mainEnd - 1);
				const std::string rest = text.substr(mainEnd + 1);
				auto [group, subgroup] = mystd::string::split_by_delimiter(rest, ':');
				if(group.empty())
					group = mainGroup;
				// Keep the real group as a separate selectable level. The main
				// group is decoded by the vote builders as a heading.
				return {mainGroup + "|" + group, subgroup.empty() ? std::string(defaultSubgroup) : subgroup};
			}
		}

		auto [group, subgroup] = mystd::string::split_by_delimiter(text, ':');
		return {std::move(group), subgroup.empty() ? std::string(defaultSubgroup) : std::move(subgroup)};
	}

	inline std::pair<std::string_view, std::string_view> split_main_group(std::string_view group) noexcept
	{
		const auto separator = group.find('|');
		if(separator == std::string_view::npos)
			return {{}, group};
		return {group.substr(0, separator), group.substr(separator + 1)};
	}

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
		explicit grouped_container(std::string defaultSubgroupKeyVal = "Uncategorized")
			: m_DefaultSubgroupKey(std::move(defaultSubgroupKeyVal))
		{
			m_DefaultSubgroupKey = defaultSubgroupKeyVal.empty() ? "Uncategorized" : defaultSubgroupKeyVal;
		}

		void set_default_subgroup_key(std::string_view key) { m_DefaultSubgroupKey = key.empty() ? "Uncategorized" : key; }
		[[nodiscard]] const std::string& get_default_subgroup_key() const noexcept { return m_DefaultSubgroupKey; }

		void add_item(std::string_view group, std::string_view subgroup, T* pItem)
		{
			if(!pItem) return;
			auto groupIter = m_Data.find(group);
			if(groupIter == m_Data.end()) groupIter = m_Data.emplace(std::string(group), SubgroupMap {}).first;
			auto& subgroupMap = groupIter->second;
			std::string_view actualSubgroup = subgroup.empty() ? std::string_view(m_DefaultSubgroupKey) : subgroup;
			auto subgroupIter = subgroupMap.find(actualSubgroup);
			if(subgroupIter == subgroupMap.end()) subgroupIter = subgroupMap.emplace(std::string(actualSubgroup), ItemList {}).first;
			subgroupIter->second.push_back(pItem);
		}
		void add_item(std::string_view group, T* pItem) { add_item(group, m_DefaultSubgroupKey, pItem); }

		[[nodiscard]] ItemList* get_items(std::string_view group, std::string_view subgroup) { return const_cast<ItemList*>(std::as_const(*this).get_items(group, subgroup)); }
		[[nodiscard]] const ItemList* get_items(std::string_view group, std::string_view subgroup) const
		{
			if(auto groupIter = m_Data.find(group); groupIter != m_Data.end())
				if(auto subgroupIter = groupIter->second.find(subgroup); subgroupIter != groupIter->second.end()) return &subgroupIter->second;
			return nullptr;
		}
		[[nodiscard]] SubgroupMap* get_subgroups(std::string_view group) { return const_cast<SubgroupMap*>(std::as_const(*this).get_subgroups(group)); }
		[[nodiscard]] const SubgroupMap* get_subgroups(std::string_view group) const
		{
			if(auto groupIter = m_Data.find(group); groupIter != m_Data.end()) return &groupIter->second;
			return nullptr;
		}
		[[nodiscard]] const GroupMap& get_all_data() const noexcept { return m_Data; }
		[[nodiscard]] GroupMap& get_all_data() noexcept { return m_Data; }
		[[nodiscard]] auto get_group_keys() const { return m_Data | std::views::keys; }
		[[nodiscard]] std::vector<std::string> get_subgroup_keys(std::string_view group) const
		{
			std::vector<std::string> keys;
			if(const auto* p = get_subgroups(group)) { keys.reserve(p->size()); for(const auto& key : *p | std::views::keys) keys.push_back(key); }
			return keys;
		}
		[[nodiscard]] bool has_group(std::string_view group) const { return m_Data.contains(group); }
		[[nodiscard]] bool has_subgroup(std::string_view group, std::string_view subgroup) const { return get_items(group, subgroup) != nullptr; }
		[[nodiscard]] bool group_has_only_default_subgroup(std::string_view groupName) const
		{
			const auto* p = get_subgroups(groupName); return p && p->size() == 1 && p->contains(m_DefaultSubgroupKey);
		}
		[[nodiscard]] bool is_empty() const noexcept { return m_Data.empty(); }
		[[nodiscard]] size_t get_group_count() const noexcept { return m_Data.size(); }
		[[nodiscard]] size_t get_subgroup_count(std::string_view groupName) const { const auto* p = get_subgroups(groupName); return p ? p->size() : 0; }
		[[nodiscard]] size_t get_item_count(std::string_view groupName, std::string_view subgroupName) const { const auto* p = get_items(groupName, subgroupName); return p ? p->size() : 0; }
		[[nodiscard]] size_t get_item_group_count(std::string_view groupName) const
		{
			const auto* p = get_subgroups(groupName); if(!p) return 0; size_t count = 0; for(const auto& [_, items] : *p) count += items.size(); return count;
		}
		void clear() noexcept { m_Data.clear(); }
		template<typename F> void sort_all_items(F&& comparator) { for(auto& [_, subgroups] : m_Data) for(auto& [__, items] : subgroups) std::ranges::sort(items, comparator); }
	};
}
#endif
