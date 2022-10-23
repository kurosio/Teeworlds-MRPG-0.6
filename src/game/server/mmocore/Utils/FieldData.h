/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMO_UTILS_FIELD_DATA_H
#define GAME_SERVER_MMO_UTILS_FIELD_DATA_H

/*
 * Like all data structures must have setters and getters
 * This is just a template for more convenient work with database data
 * TODO: try using constexpr and a kind of variation pattern
 * std::variant and std::any are used partially at compile time and runtime.
 * And there is no way to give exact information about the returned object
 *
 * Can use container how only for work inside db
 * struct DataStructure : pulbic CFieldContainer
 * {
 *		CFieldData<int> m_Level { "Level", "info", this };
 * } m_Data;
 *
 * m_Data.initFields();
 */

template < typename T >
class CFieldData
{
	friend class CFieldContainer;

	size_t m_UniqueID{};
	char m_aFieldName[128] = {};
	char m_aDescription[128] = {};

public:
	CFieldData() = default;
	CFieldData(size_t UniqueID, const char* pFieldName, const char* pDescription) { init(UniqueID, pFieldName, pDescription); }

	T m_Value{};

	// functions
	void init(size_t UniqueID, const char* pFieldName, const char* pDescription)
	{
		m_UniqueID = UniqueID;
		str_copy(m_aFieldName, pFieldName, sizeof(m_aFieldName));
		str_copy(m_aDescription, pDescription, sizeof(m_aDescription));
		m_Value = T(); // default constructor
	}
	const char* getFieldName() const { return m_aFieldName; };
	const char* getDescription() const { return m_aDescription; }
};

template<class> inline constexpr bool always_false_v = false;

class CFieldContainer
{
	using FieldVariant = std::variant < CFieldData<int>, CFieldData<float>, CFieldData<double>, CFieldData<std::string> >;

	std::list < FieldVariant > m_VariantsData;

public:
	CFieldContainer() = default;
	CFieldContainer(std::initializer_list<FieldVariant>&& pField) { m_VariantsData.insert(m_VariantsData.end(), pField.begin(), pField.end()); }

	template < typename T >
	CFieldData<T>& operator()(size_t UniqueID, [[maybe_unused]]T DefaultValue)
	{
		auto it = std::find_if(m_VariantsData.begin(), m_VariantsData.end(), [UniqueID](auto& p)
		{
			try { return std::get<CFieldData<T>>(p).m_UniqueID == UniqueID; }
			catch([[maybe_unused]] const std::bad_variant_access& ex) { return false; }
		});

		dbg_assert(it != m_VariantsData.end(), "error get from variant data CFieldData");
		return std::get<CFieldData<T>>(*it);
	}
	
	std::string getUpdateField()
	{
		std::string Fields;
		for(auto& p : m_VariantsData)
		{
			std::visit([&Fields](auto&& arg)
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr(std::is_same_v<T, CFieldData<int>> || std::is_same_v<T, CFieldData<float>>|| std::is_same_v<T, CFieldData<double>>)
					Fields.append(arg.getFieldName()).append("=").append(std::to_string(arg.m_Value)).append(",");
				else if constexpr(std::is_same_v<T, CFieldData<std::string>>)
					Fields.append(arg.getFieldName()).append("=").append(arg.m_Value).append(",");
				else
					dbg_assert(always_false_v<T>, "non-exhaustive visitor!");
			}, p);
		}

		Fields.pop_back();
		return Fields;
	}

	void initFields(ResultPtr* pRes)
	{
		for(auto& p : m_VariantsData)
		{
			std::visit([&pRes](auto&& arg)
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr(std::is_same_v<T, CFieldData<int>>)
					arg.m_Value = pRes->get()->getInt(arg.getFieldName());
				else if constexpr(std::is_same_v<T, CFieldData<float>>)
					arg.m_Value = static_cast<float>(pRes->get()->getDouble(arg.getFieldName()));
				else if constexpr(std::is_same_v<T, CFieldData<double>>)
					arg.m_Value = pRes->get()->getDouble(arg.getFieldName());
				else if constexpr(std::is_same_v<T, CFieldData<std::string>>)
					arg.m_Value = pRes->get()->getString(arg.getFieldName());
				else
					dbg_assert(always_false_v<T>, "non-exhaustive visitor!");
			}, p);
		}
	}
};

#endif