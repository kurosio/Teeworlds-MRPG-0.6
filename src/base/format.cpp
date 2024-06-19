#include "format.h"
#include <base/system.h>

void fmt_init_handler_func(HandlerFmtCallbackFunc* pCallback, void* pData)
{
	struct_format_implement::handler_fmt::init(pCallback, pData);
}

void fmt_set_flags(int flags)
{
	struct_format_implement::handler_fmt::use_flags(flags);
}

std::string pluralize(int count, const std::vector<std::string>& forms)
{
	// initialize variables
	const size_t numForms = forms.size();

	// special case for exactly 2 forms (singular and plural)
	if(numForms == 2)
		return (count == 1) ? forms[0] : forms[1];

	// based on typical plural rules in many languages
	if(numForms == 3)
	{
		if(forms.size() == 1 || (count % 10 == 1 && count % 100 != 11))
			return forms[0];
		if(forms.size() == 2 || (count % 10 >= 2 && count % 10 <= 4 && (count % 100 < 10 || count % 100 >= 20)))
			return forms[1];
		return forms[2];
	}

	return "";
}

std::vector<std::string> collect_argument_variants(size_t startPos, const std::string& Argument)
{
	// initialize variables
	std::vector<std::string> values;
	size_t pipePos = Argument.find('|', startPos);

	// collect data
	while(pipePos != std::string::npos)
	{
		values.emplace_back(Argument.substr(startPos, pipePos - startPos));
		startPos = pipePos + 1;
		pipePos = Argument.find('|', startPos);
	}

	// collect last data
	if(startPos < Argument.length())
		values.emplace_back(Argument.substr(startPos));

	return values;
}


void struct_format_implement::prepare_result(const description&, const std::string& Text, std::string* pResult, std::vector<std::string>& vPack)
{
	enum { arg_default, arg_plural, arg_truncate, arg_truncate_full, arg_truncate_custom };
	std::string argument;
	size_t argumentPosition = 0;
	bool argumentProcessing = false;
	int argumentType = arg_default;

	for(char iterChar : Text)
	{
		// start argument processing
		if(iterChar == '{')
		{
			argumentProcessing = true;
			continue;
		}

		// get argument type
		if(argumentType == arg_default)
		{
			// truncate type
			if(iterChar == '~')
				argumentType = arg_truncate;

			// plural type
			if(iterChar == '#')
				argumentType = arg_plural;
		}

		// end argument processing
		if(iterChar == '}')
		{
			argumentProcessing = false;
			if(argumentPosition < vPack.size())
			{
				std::string argumentResult = vPack[argumentPosition++];

				// truncate type
				if(argumentType == arg_truncate)
				{
					std::string truncationString;
					char truncationAfterChar = '\0';

					for(char j : argument)
					{
						if(j == '%' && argumentPosition < vPack.size())
						{
							truncationString = argumentResult;
							argumentResult = vPack[argumentPosition++];
							continue;
						}
						if(isdigit(j))
						{
							truncationString += j;
							continue;
						}
						truncationAfterChar = j;
					}

					const int truncateNum = truncationString.empty() ? 0 : str_toint(truncationString.c_str());
					argumentType = (truncationAfterChar == '\0') ? arg_truncate_full : arg_truncate_custom;
					if(argumentType == arg_truncate_full && argumentResult.size() > static_cast<size_t>(truncateNum))
					{
						argumentResult.resize(truncateNum);
					}
					else if(argumentType == arg_truncate_custom && argumentResult.find(truncationAfterChar) != std::string::npos)
					{
						const size_t dotPos = argumentResult.find(truncationAfterChar) + 1;
						argumentResult = argumentResult.substr(0, dotPos + truncateNum);
					}
				}
				// plural type TODO: skip result
				else if(argumentType == arg_plural)
				{
					bool parsePlural = false;
					std::string resultPlural {};
					std::string variantPlural {};

					for(auto& c : argument)
					{
						if(c == '#')
						{
							resultPlural += argumentResult;
						}
						else if(c == '(')
						{
							parsePlural = true;
						}
						else if(c == ')')
						{
							resultPlural += pluralize(str_toint(argumentResult.c_str()), collect_argument_variants(0, variantPlural));
							parsePlural = false;
						}
						else if(!parsePlural)
						{
							resultPlural += c;
						}
						else
						{
							variantPlural += c;
						}
					}

					argumentResult = resultPlural;
				}

				// reset and add result
				argumentType = arg_default;
				(*pResult) += argumentResult;
			}

			argument.clear();
			continue;
		}

		// collect
		if(argumentProcessing)
			argument += iterChar;
		else
			(*pResult) += iterChar;
	}
}