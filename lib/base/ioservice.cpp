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

#include "base/ioservice.hpp"
#include <boost/thread/thread.hpp>
#include <boost/thread/once.hpp>

using namespace icinga;

static boost::once_flag l_IOOnceFlag = BOOST_ONCE_INIT;

void IOService::StaticInitialize(void)
{
	for (int i = 0; i < 4; i++) {
		boost::thread t(boost::bind(&boost::asio::io_service::run, boost::ref(GetInstanceInternal())));
		t.detach();
	}
}

boost::asio::io_service& IOService::GetInstanceInternal(void)
{
	static boost::asio::io_service service;
	return service;
}

boost::asio::io_service& IOService::GetInstance(void)
{
	boost::call_once(l_IOOnceFlag, &IOService::StaticInitialize);

	return GetInstanceInternal();
}
