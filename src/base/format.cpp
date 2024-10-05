#include "format.h"
#include <base/system.h>

std::string pluralize(const BigInt& count, const std::vector<std::string>& forms)
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

std::vector<std::string> collect_argument_plural(size_t startPos, const std::string& Argument)
{
	// initialize variables
	std::vector<std::string> values;
	size_t pipePos = Argument.find('|', startPos);

	// collect data
	while(pipePos != std::string::npos)
	{
		values.emplace_back(Argument, startPos, pipePos - startPos);
		startPos = pipePos + 1;
		pipePos = Argument.find('|', startPos);
	}

	// collect last data
	if(startPos < Argument.length())
		values.emplace_back(Argument.substr(startPos));

	return values;
}

void CFormatter::prepare_result(const std::string& Text, std::string* pResult, std::vector<std::pair<int, std::string>>& vPack) const
{
	// initialize variables
	enum { arg_default, arg_plural, arg_big_digit, arg_skip_handle, arg_truncate, arg_truncate_full, arg_truncate_custom };
	std::string argument;
	size_t argumentPosition = 0;
	bool argumentProcessing = false;
	bool argumentHandled = false;
	int argumentType = arg_default;
	auto handleArguments = [this, &argumentType, &argumentHandled](int argumentTypename, std::string& argumentResult, std::string argumentFrom)
	{
		if(argumentType != arg_skip_handle && get_flags() & FMTFLAG_HANDLE_ARGS && !argumentHandled)
		{
			if(argumentTypename == type_integers || argumentTypename == type_big_integers)
			{
				if(argumentType == arg_big_digit)
				{
					argumentResult = fmt_big_digit(std::move(argumentFrom));
				}
				else
				{
					argumentResult = fmt_digit(std::move(argumentFrom));
				}
			}
			else if(argumentTypename == type_string)
			{
				argumentResult = handle(argumentFrom);
			}

			argumentHandled = true;
		}
	};

	// arguments parsing
	for(char iterChar : Text)
	{
		// start argument processing
		if(iterChar == '{')
		{
			argumentType = arg_default;
			argumentProcessing = true;
			argumentHandled = false;
			continue;
		}

		// get argument type
		if(argumentType == arg_default)
		{
			switch(iterChar)
			{
				case '~': argumentType = arg_truncate; break;
				case '#': argumentType = arg_plural; break;
				case '-': argumentType = arg_skip_handle; break;
				case '$': argumentType = arg_big_digit; break;
				default: break;
			}
		}

		// end argument processing
		if(iterChar == '}')
		{
			argumentProcessing = false;
			if(argumentPosition < vPack.size())
			{
				// initialize variables
				auto& [argumentTypename, argumentResult] = vPack[argumentPosition++];

				// truncate type
				if(argumentType == arg_truncate)
				{
					// initialize variables
					std::string truncationString;
					char truncationAfterChar = '\0';

					// skip first '~' if present
					if(!argument.empty() && argument[0] == '~')
						argument = argument.substr(1);

					// parse truncate description
					for(char c : argument)
					{
						if(c == '%' && argumentTypename == type_integers && argumentPosition < vPack.size())
						{
							auto& [newTypename, newResult] = vPack[argumentPosition++];
							truncationString = argumentResult;
							argumentTypename = newTypename;
							argumentResult = newResult;
						}
						else if(isdigit(c))
							truncationString += c;
						else
							truncationAfterChar = c;
					}

					// truncate
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
				// plural type
				else if(argumentType == arg_plural)
				{
					if(argumentTypename == type_integers || argumentTypename == type_big_integers)
					{
						// initialize variables
						bool parsePlural = false;
						std::string resultPlural {};
						std::string variantPlural {};
						BigInt numberPlural(argumentResult);

						// parse plural description 
						for(auto& c : argument)
						{
							// plural argument position
							if(c == '#')
							{
								handleArguments(argumentTypename, argumentResult, argumentResult);
								resultPlural += argumentResult;
							}
							else if(c == '(')
							{
								parsePlural = true;
							}
							else if(c == ')')
							{
								resultPlural += pluralize(numberPlural, collect_argument_plural(0, variantPlural));
								parsePlural = false;
								variantPlural.clear();
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

						// update argument result by plural result
						argumentResult = resultPlural;
					}
				}

				// reset and append handled result
				handleArguments(argumentTypename, argumentResult, argumentResult);
				(*pResult) += argumentResult;
			}

			// clear argument
			argument.clear();
			continue;
		}

		// collect
		if(argumentProcessing)
		{
			argument += iterChar;
		}
		else
		{
			(*pResult) += iterChar;
		}
	}
}
