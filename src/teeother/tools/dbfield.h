/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMO_UTILS_FIELD_DATA_H
#define GAME_SERVER_MMO_UTILS_FIELD_DATA_H

#include <engine/server/db/db_result.h>

/*
 * Like all data structures must have setters and getters
 * This is just a template for more convenient work with database data
 * TODO: try using constexpr and a kind of variation pattern
 * std::variant and std::any are used partially at compile time and runtime.
 * And there is no way to give exact information about the returned object
 *
 * Can use container how only for work inside db
 * struct DataStructure : pulbic DBFieldContainer
 * {
 *		DBField<int> m_Level { "Level", "info", this };
 * } m_Data;
 *
 * m_Data.initFields(Result);
 */

template < typename T >
class DBField
{
	friend class DBFieldContainer;

	size_t m_UniqueID{};
	std::string_view m_FieldName {};
	std::string_view m_Description {};

public:
	DBField() = delete;
    DBField(size_t UniqueID, const char* pFieldName, const char* pDescription, T DefaultValue = {}, T MaxValue = {})
		: m_Value(DefaultValue), m_MaxValue(MaxValue) { init(UniqueID, pFieldName, pDescription); }

	T m_Value{};
    T m_MaxValue{};

	// functions
	void init(size_t UniqueID, const char* pFieldName, const char* pDescription)
	{
		m_UniqueID = UniqueID;
		m_FieldName = pFieldName;
		m_Description = pDescription;
	}

	const T& getValue() const
	{
		return m_Value;
	}

	const char* getFieldName() const
	{
		return m_FieldName.data();
	}

	const char* getDescription() const
	{
		return m_Description.data();
	}
};

template<class> inline constexpr bool always_false_v = false;

class DBFieldContainer
{
    using FieldVariant = std::variant<
        DBField<int>,
        DBField<unsigned int>,
        DBField<int64_t>,
        DBField<uint64_t>,
        DBField<float>,
        DBField<double>,
        DBField<std::string>,
        DBField<bool>,
        DBField<BigInt>
    >;

    std::list<FieldVariant> m_VariantsData;

public:
    DBFieldContainer() = default;
    DBFieldContainer(std::initializer_list<FieldVariant>&& pField)
    {
        m_VariantsData.insert(m_VariantsData.end(), pField.begin(), pField.end());
    }

    template <typename T>
    T& getRef(size_t UniqueID)
    {
        auto it = std::find_if(m_VariantsData.begin(), m_VariantsData.end(), [UniqueID](auto& p)
        {
            try
            {
                return std::get<DBField<T>>(p).m_UniqueID == UniqueID;
            }
            catch(const std::bad_variant_access&)
            {
                return false;
            }
        });

        assert(it != m_VariantsData.end() && "Error: could not find variant data DBField");
        return std::get<DBField<T>>(*it).m_Value;
    }

    template <typename T>
    DBField<T>& getField(size_t UniqueID)
    {
        auto it = std::find_if(m_VariantsData.begin(), m_VariantsData.end(), [UniqueID](auto& p)
        {
            try
            {
                return std::get<DBField<T>>(p).m_UniqueID == UniqueID;
            }
            catch(const std::bad_variant_access&)
            {
                return false;
            }
        });

        assert(it != m_VariantsData.end() && "Error: could not find variant data DBField");
        return std::get<DBField<T>>(*it);
    }

    // Reads every field of the container from one row of a result.
    void initFields(const Db::CResultSet& Result)
    {
        for(auto& p : m_VariantsData)
        {
            std::visit([&Result](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr(std::is_same_v<T, DBField<int>>)
                    arg.m_Value = Result.GetInt(arg.getFieldName());
                else if constexpr(std::is_same_v<T, DBField<unsigned int>>)
                    arg.m_Value = Result.GetUInt(arg.getFieldName());
                else if constexpr(std::is_same_v<T, DBField<int64_t>>)
                    arg.m_Value = Result.GetInt64(arg.getFieldName());
                else if constexpr(std::is_same_v<T, DBField<uint64_t>>)
                    arg.m_Value = Result.GetUInt64(arg.getFieldName());
                else if constexpr(std::is_same_v<T, DBField<float>>)
                    arg.m_Value = static_cast<float>(Result.GetDouble(arg.getFieldName()));
                else if constexpr(std::is_same_v<T, DBField<double>>)
                    arg.m_Value = Result.GetDouble(arg.getFieldName());
                else if constexpr(std::is_same_v<T, DBField<std::string>>)
                    arg.m_Value = Result.GetString(arg.getFieldName());
                else if constexpr(std::is_same_v<T, DBField<bool>>)
                    arg.m_Value = Result.GetBoolean(arg.getFieldName());
                else if constexpr(std::is_same_v<T, DBField<BigInt>>)
                    arg.m_Value = BigInt(Result.GetString(arg.getFieldName()));
                else
                    dbg_assert(always_false_v<T>, "non-exhaustive visitor!");
            }, p);
        }
    }
};

#endif