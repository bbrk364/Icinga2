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

#ifndef IOSERVICE_H
#define IOSERVICE_H

#include "base/i2-base.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/smart_ptr/make_shared.hpp>

namespace icinga
{

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> SslSocket;
typedef boost::shared_ptr<SslSocket> SslSocketPtr;

typedef boost::asio::ip::tcp::acceptor TcpAcceptor;
typedef boost::shared_ptr<TcpAcceptor> TcpAcceptorPtr;

class IOService
{
public:
	static boost::asio::io_service& GetInstance(void);

private:
	static void StaticInitialize(void);
	static boost::asio::io_service& GetInstanceInternal(void);
};

}

#endif /* IOSERVICE_H */
