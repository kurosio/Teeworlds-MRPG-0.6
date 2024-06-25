/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TEMPLATE_UTILS_H
#define GAME_SERVER_CORE_TEMPLATE_UTILS_H

using ByteArray = std::basic_string<std::byte>;

namespace mrpgstd
{
	// сoncept to check if a type has a clear() function
	template<typename T>
	concept is_has_clear_function = requires(T & c) {
		{ c.clear() } -> std::same_as<void>;
	};

	// concept to check if a type is a smart pointer functions
	template<typename T>
	concept is_smart_pointer = requires(T & c) {
		{ c.get() } -> std::convertible_to<typename T::element_type*>;
		{ c.reset() } noexcept -> std::same_as<void>;
	};

	// сoncept to check if a type is a container
	template <typename T>
	concept is_container = requires(T & c) {
		typename T::value_type;
		typename T::iterator;
		{ c.begin() } -> std::convertible_to<typename T::iterator>;
		{ c.end() } -> std::convertible_to<typename T::iterator>;
	};

	// сoncept to check if a type is a map container
	template<typename T>
	concept is_map_container = requires(T & c) {
		typename T::key_type;
		typename T::mapped_type;
			requires is_container<T>;
			requires std::same_as<typename T::value_type, std::pair<const typename T::key_type, typename T::mapped_type>>;
	};

	// function to clear a container
	template<is_container T>
	void free_container(T& container)
	{
		static_assert(is_has_clear_function<T>, "One or more types do not have clear() function");

		if constexpr(is_map_container<T>)
		{
			if constexpr(is_container<typename T::mapped_type>)
				std::ranges::for_each(container, [](auto& element) { free_container(element.second); });
			else if constexpr(std::is_pointer_v<typename T::mapped_type> && !is_smart_pointer<typename T::mapped_type>)
				std::ranges::for_each(container, [](auto& element) { delete element.second; });
		}
		else
		{
			if constexpr(is_container<typename T::value_type>)
				std::ranges::for_each(container, [](auto& element) { free_container(element); });
			else if constexpr(std::is_pointer_v<typename T::value_type> && !is_smart_pointer<typename T::value_type>)
				std::ranges::for_each(container, [](auto& element) { delete element; });
		}
		container.clear();
	}

	// clear multiple containers uses fold expression
	template<is_container... Containers>
	void free_container(Containers&... args) { (free_container(args), ...); }
}

// aesthetic utils (TODO: remove)
namespace Utils::Aesthetic
{
	/* BORDURES */
	// Example: ───※ ·· ※───
	inline std::string B_PILLAR(int iter, bool right)
	{
		std::string result {};
		if(right) { result += "\u203B \u00B7"; }
		for(int i = 0; i < iter; i++) { result += "\u2500"; }
		if(!right) { result += "\u00B7 \u203B"; }
		return std::move(result);
	}

	// Example: ┏━━━━━ ━━━━━┓
	inline std::string B_DEFAULT_TOP(int iter, bool right)
	{
		std::string result;
		if(!right) { result += "\u250F"; }
		for(int i = 0; i < iter; i++) { result += "\u2501"; }
		if(right) { result += "\u2513"; }
		return std::move(result);
	}

	// Example: ✯¸.•*•✿✿•*•.¸✯
	inline std::string B_FLOWER(bool right)
	{
		return right ? "\u273F\u2022*\u2022.\u00B8\u272F" : "\u272F\u00B8.\u2022*\u2022\u273F";
	}

	// Example: ──⇌ • • ⇋──
	inline std::string B_CONFIDENT(int iter, bool right)
	{
		std::string result;
		if(right) { result += " \u21CC \u2022"; }
		for(int i = 0; i < iter; i++) { result += "\u2500"; }
		if(!right) { result += "\u2022 \u21CB"; }
		return std::move(result);
	}

	/* LINES */
	// Example: ━━━━━━
	inline std::string L_DEFAULT(int iter)
	{
		std::string result;
		for(int i = 0; i < iter; i++) { result += "\u2501"; }
		return std::move(result);
	}
	// Example: ︵‿︵‿
	inline std::string L_WAVES(int iter, bool post)
	{
		std::string result;
		for(int i = 0; i < iter; i++) { result += "\uFE35\u203F"; }
		return std::move(result);
	}

	/* WRAP LINES */
	// Example:  ────⇌ • • ⇋────
	inline std::string SWL_CONFIDENT(int iter)
	{
		return B_CONFIDENT(iter, false) + B_CONFIDENT(iter, true);
	}
	// Example: ───※ ·· ※───
	inline std::string SWL_PILAR(int iter)
	{
		return B_PILLAR(iter, false) + B_PILLAR(iter, true);
	}

	/* QUOTES */
	// Example: -ˏˋ Text here ˊˎ
	inline std::string Q_DEFAULT(bool right)
	{
		return right ? "\u02CA\u02CE" : "-\u02CF\u02CB";
	}

	/* SYMBOL SMILIES */
	// Example: 『•✎•』
	inline std::string S_EDIT(const char* pBody)
	{
		return std::string("\u300E " + std::string(pBody) + " \u300F");
	}
	// Example: ᴄᴏᴍᴘʟᴇᴛᴇ!
	inline std::string S_COMPLETE()
	{
		return "\u1D04\u1D0F\u1D0D\u1D18\u029F\u1D07\u1D1B\u1D07!";
	}
}

// string utils
namespace Utils::String
{
	inline std::string progressBar(int max_value, int current_value, int step, const std::string& UTF_fill_symbol, const std::string& UTF_empty_symbol)
	{
		std::string ProgressBar;
		int numFilled = current_value / step;
		int numEmpty = max_value / step - numFilled;
		ProgressBar.reserve(numFilled + numEmpty);

		for(int i = 0; i < numFilled; i++)
			ProgressBar += UTF_fill_symbol;

		for(int i = 0; i < numEmpty; i++)
			ProgressBar += UTF_empty_symbol;

		return ProgressBar;
	}
}

// json utils
namespace Utils::Json
{
	inline void parseFromString(const std::string& Data, const std::function<void(nlohmann::json& pJson)>& pFuncCallback)
	{
		if(!Data.empty())
		{
			try
			{
				nlohmann::json JsonData = nlohmann::json::parse(Data);
				pFuncCallback(JsonData);
			}
			catch(nlohmann::json::exception& s)
			{
				dbg_assert(false, fmt("[json parse] Invalid json: {}", s.what()).c_str());
			}
		}
	}
}

// files utils
namespace Utils::Files
{
	enum Result : int
	{
		ERROR_FILE,
		SUCCESSFUL,
	};

	inline Result loadFile(const char* pFile, ByteArray* pData)
	{
		IOHANDLE File = io_open(pFile, IOFLAG_READ);
		if(!File)
			return ERROR_FILE;

		pData->resize((unsigned)io_length(File));
		io_read(File, pData->data(), (unsigned)pData->size());
		io_close(File);
		return SUCCESSFUL;
	}

	inline Result deleteFile(const char* pFile)
	{
		int Result = fs_remove(pFile);
		return Result == 0 ? SUCCESSFUL : ERROR_FILE;
	}

	inline Result saveFile(const char* pFile, const void* pData, unsigned size)
	{
		// delete old file
		deleteFile(pFile);

		IOHANDLE File = io_open(pFile, IOFLAG_WRITE);
		if(!File)
			return ERROR_FILE;

		io_write(File, pData, size);
		io_close(File);
		return SUCCESSFUL;
	}
}

// base class multiworld identifiable data
class _BaseMultiworldIdentifiableData
{
	inline static class IServer* m_pServer {};

public:
	IServer* Server() const { return m_pServer; }
	static void Init(IServer* pServer) { m_pServer = pServer; }
};

// class multiworld identifiable data
template < typename T >
class MultiworldIdentifiableData : public _BaseMultiworldIdentifiableData
{
protected:
	static inline T m_pData {};

public:
	static T& Data() { return m_pData; }
};

#endif
