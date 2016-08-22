/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2016 Icinga Development Team (https://www.icinga.org/)  *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#ifndef STRING_H
#define STRING_H

#include "base/i2-base.hpp"
#include "base/object.hpp"
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/range/iterator.hpp>
#include <boost/functional/hash.hpp>
#include <string.h>
#include <functional>
#include <string>
#include <iosfwd>
#include <boost/flyweight.hpp>
#include <boost/flyweight/intermodule_holder.hpp>

namespace icinga {

class Value;

/**
 * String class.
 *
 * Rationale for having this: The std::string class has an ambiguous assignment
 * operator when used in conjunction with the Value class.
 */
class I2_BASE_API String
{
public:
	typedef std::string::iterator Iterator;
	typedef std::string::const_iterator ConstIterator;

	typedef std::string::iterator iterator;
	typedef std::string::const_iterator const_iterator;

	typedef std::string::reverse_iterator ReverseIterator;
	typedef std::string::const_reverse_iterator ConstReverseIterator;

	typedef std::string::reverse_iterator reverse_iterator;
	typedef std::string::const_reverse_iterator const_reverse_iterator;

	typedef std::string::size_type SizeType;

	inline String(void)
		: m_Data()
	{ }

	inline String(const char *data)
		: m_Data(data)
	{ }

	inline String(const std::string& data)
		: m_Data(data)
	{ }

	inline String(String::SizeType n, char c)
		: m_Data(n, c)
	{ }

	inline String(const String& other)
		: m_Data(other.m_Data)
	{ }

	inline ~String(void)
	{ }

	template<typename InputIterator>
	String(InputIterator begin, InputIterator end)
		: m_Data(std::string(begin, end))
	{ }

	inline String& operator=(const String& rhs)
	{
		m_Data = rhs.m_Data;
		return *this;
	}

	inline String& operator=(const std::string& rhs)
	{
		m_Data = rhs;
		return *this;
	}

	inline String& operator=(const char *rhs)
	{
		m_Data = rhs;
		return *this;
	}

	inline const char& operator[](SizeType pos) const
	{
		return GetData()[pos];
	}

	inline String& operator+=(const String& rhs)
	{
		std::string t = GetData();
		t += rhs;
		m_Data = t;
		return *this;
	}

	inline String& operator+=(const char *rhs)
	{
		std::string t = GetData();
		t += rhs;
		m_Data = t;
		return *this;
	}

	String& operator+=(const Value& rhs);

	inline String& operator+=(char rhs)
	{
		std::string t = GetData();
		t += rhs;
		m_Data = t;
		return *this;
	}

	inline bool IsEmpty(void) const
	{
		return GetData().empty();
	}

	inline bool operator<(const String& rhs) const
	{
		return GetData() < rhs.GetData();
	}

	inline operator const std::string&(void) const
	{
		return GetData();
	}

	inline const char *CStr(void) const
	{
		return GetData().c_str();
	}

	inline SizeType GetLength(void) const
	{
		return GetData().size();
	}

	inline const std::string& GetData(void) const
	{
		return m_Data.get();
	}

	inline SizeType Find(const String& str, SizeType pos = 0) const
	{
		return GetData().find(str, pos);
	}

	inline SizeType RFind(const String& str, SizeType pos = NPos) const
	{
		return GetData().rfind(str, pos);
	}

	inline SizeType FindFirstOf(const char *s, SizeType pos = 0) const
	{
		return GetData().find_first_of(s, pos);
	}

	inline SizeType FindFirstOf(char ch, SizeType pos = 0) const
	{
		return GetData().find_first_of(ch, pos);
	}

	inline SizeType FindFirstNotOf(const char *s, SizeType pos = 0) const
	{
		return GetData().find_first_not_of(s, pos);
	}

	inline SizeType FindFirstNotOf(char ch, SizeType pos = 0) const
	{
		return GetData().find_first_not_of(ch, pos);
	}

	inline SizeType FindLastOf(const char *s, SizeType pos = NPos) const
	{
		return GetData().find_last_of(s, pos);
	}

	inline SizeType FindLastOf(char ch, SizeType pos = NPos) const
	{
		return GetData().find_last_of(ch, pos);
	}

	inline String SubStr(SizeType first, SizeType len = NPos) const
	{
		return GetData().substr(first, len);
	}

	inline String Replace(SizeType first, SizeType second, const String& str)
	{
		std::string t = GetData();
		t.replace(first, second, str);
		return t;
	}

	inline String Trim(void) const
	{
		std::string t = GetData();
		boost::algorithm::trim(t);
		return t;
	}

	inline String TrimLeft(void) const
	{
		std::string t = GetData();
		boost::algorithm::trim_left(t);
		return t;
	}

	inline String TrimRight(void) const
	{
		std::string t = GetData();
		boost::algorithm::trim_right(t);
		return t;
	}

	inline String ToLower(void) const
	{
		std::string t = GetData();
		boost::algorithm::to_lower(t);
		return t;
	}

	inline String ToUpper(void) const
	{
		std::string t = GetData();
		boost::algorithm::to_upper(t);
		return t;
	}

	inline String Reverse(void) const
	{
		std::string t = GetData();
		std::reverse(t.begin(), t.end());
		return t;
	}

	template<typename T>
	inline std::vector<String> Split(const T& chars) const
	{
		std::string t = GetData();
		std::vector<String> res;
		boost::algorithm::split(res, t, boost::is_any_of(chars));
		return res;
	}

	inline bool Contains(const String& str) const
	{
		return (GetData().find(str) != std::string::npos);
	}

	inline void swap(String& str)
	{
		m_Data.swap(str.m_Data);
	}

	inline ConstIterator Begin(void) const
	{
		return GetData().begin();
	}

	inline ConstIterator End(void) const
	{
		return GetData().end();
	}

	static const SizeType NPos;

	static Object::Ptr GetPrototype(void);

private:
	boost::flyweight<std::string, boost::flyweights::intermodule_holder> m_Data;
};

inline std::ostream& operator<<(std::ostream& stream, const String& str)
{
	stream << str.GetData();
	return stream;
}

inline std::istream& operator>>(std::istream& stream, String& str)
{
	std::string tstr;
	stream >> tstr;
	str = tstr;
	return stream;
}

inline String operator+(const String& lhs, const String& rhs)
{
	return lhs.GetData() + rhs.GetData();
}

inline String operator+(const String& lhs, const char *rhs)
{
	return lhs.GetData() + rhs;
}

inline String operator+(const char *lhs, const String& rhs)
{
	return lhs + rhs.GetData();
}

inline bool operator==(const String& lhs, const String& rhs)
{
	return lhs.GetData() == rhs.GetData();
}

inline bool operator==(const String& lhs, const char *rhs)
{
	return lhs.GetData() == rhs;
}

inline bool operator==(const char *lhs, const String& rhs)
{
	return lhs == rhs.GetData();
}

inline bool operator<(const String& lhs, const char *rhs)
{
	return lhs.GetData() < rhs;
}

inline bool operator<(const char *lhs, const String& rhs)
{
	return lhs < rhs.GetData();
}

inline bool operator>(const String& lhs, const String& rhs)
{
	return lhs.GetData() > rhs.GetData();
}

inline bool operator>(const String& lhs, const char *rhs)
{
	return lhs.GetData() > rhs;
}

inline bool operator>(const char *lhs, const String& rhs)
{
	return lhs > rhs.GetData();
}

inline bool operator<=(const String& lhs, const String& rhs)
{
	return lhs.GetData() <= rhs.GetData();
}

inline bool operator<=(const String& lhs, const char *rhs)
{
	return lhs.GetData() <= rhs;
}

inline bool operator<=(const char *lhs, const String& rhs)
{
	return lhs <= rhs.GetData();
}

inline bool operator>=(const String& lhs, const String& rhs)
{
	return lhs.GetData() >= rhs.GetData();
}

inline bool operator>=(const String& lhs, const char *rhs)
{
	return lhs.GetData() >= rhs;
}

inline bool operator>=(const char *lhs, const String& rhs)
{
	return lhs >= rhs.GetData();
}

inline bool operator!=(const String& lhs, const String& rhs)
{
	return lhs.GetData() != rhs.GetData();
}

inline bool operator!=(const String& lhs, const char *rhs)
{
	return lhs.GetData() != rhs;
}

inline bool operator!=(const char *lhs, const String& rhs)
{
	return lhs != rhs.GetData();
}

inline String::ConstIterator range_begin(const String& x)
{
	return x.Begin();
}

inline String::ConstIterator range_end(const String& x)
{
	return x.End();
}

inline std::size_t hash_value(const String& s)
{
	boost::hash<std::string> hasher;
	return hasher(s.GetData());
}

}

namespace boost
{

template<>
struct range_const_iterator<icinga::String>
{
	typedef icinga::String::ConstIterator type;
};

}

#endif /* STRING_H */
