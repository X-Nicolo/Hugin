/**
 * @file StringAdapter.h
 * @brief
 *  Created on: Jul 23, 2010
 * @author Florian Achleitner <florian.achleitner.2.6.31@gmail.com>
 */

#ifndef STRINGADAPTER_H_
#define STRINGADAPTER_H_

#include <string>
#include <sstream>


namespace makefile
{
/**
 * Adapts a string of wide or narrow characters to a narrow character std::string.
 * It uses the narrow method of standard iostreams, it doesn't do any code conversion.
 * It's purpose is to allow exception texts to contain wide strings, regardless of
 * USE_WCHAR.
 */
class StringAdapter : public std::string
{
	void use_narrow(const wchar_t* ws)
	{
		std::ostringstream ostr;
		for(const wchar_t* i = ws; *i; i++)
		{
			ostr.put(ostr.narrow(*i, '?'));
		}
		assign(ostr.str());
	}
public:
	StringAdapter(std::wstring& ws)
	{
		use_narrow(ws.c_str());
	}
	StringAdapter(const std::string& s)
	: std::string(s)
	{}
	StringAdapter(const wchar_t* ws)
	{
		use_narrow(ws);
	}
	StringAdapter(const char* s)
	: std::string(s)
	{}
	virtual ~StringAdapter()
	{
	}
};

}

#endif /* STRINGADAPTER_H_ */
