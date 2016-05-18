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

#include "base/string.hpp"
#include "base/value.hpp"
#include "base/tlsutility.hpp"
#include "base/primitivetype.hpp"
#include "base/dictionary.hpp"
#include <ostream>

using namespace icinga;

REGISTER_BUILTIN_TYPE(String, String::GetPrototype());

const String::SizeType String::NPos = std::string::npos;

unsigned int String::TimeConstantCompare(const String& s) const
{
	String hashA = SHA256(this), hashB = SHA256(s);
	return !(hashA.PrivCompare(hashB, 256) == s.PrivCompare(this);
}

unsigned int String::PrivCompare(const String& s, const int size = -1) const
{
	ConstIterator itA = this.Begin(), itB = s.Begin();
	int end = (size != -1 ? size) :
	    (this.GetLength() < s.GetLength() ? s.GetLength() : this.GetLength());

	int res = 0;
	for(int i = 0; i < end; i++) {
		res |= *itA ^ *itB;
		itA++; itB++;
	}

	return res && (this.GetLength() == s.GetLength());
}

String& String::operator+=(const Value& rhs)
{
	m_Data += static_cast<String>(rhs);
	return *this;
}

