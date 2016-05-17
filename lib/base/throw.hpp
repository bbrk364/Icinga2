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

#ifndef THROW_H
#define THROW_H

#include "base/i2-base.hpp"
#include <boost/throw_exception.hpp>

namespace icinga
{

typedef std::exception_ptr ExceptionPtr;

template<typename T>
#ifdef _WIN32
__declspec(noreturn)
#else /* _WIN32 */
__attribute__ ((noreturn))
#endif /* _WIN32 */
void ThrowException(const T& ex)
{
	throw ex;
}

inline ExceptionPtr CurrentException(void)
{
	return std::current_exception();
}

#ifdef _WIN32
__declspec(noreturn)
#else /* _WIN32 */
__attribute__ ((noreturn))
#endif /* _WIN32 */
inline void RethrowException(ExceptionPtr eptr)
{
	std::rethrow_exception(eptr);
}

}

#endif /* THROW_H */
