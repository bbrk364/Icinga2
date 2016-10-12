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

#ifndef OPENTSDBWRITER_H
#define OPENTSDBWRITER_H

#include "perfdata/opentsdbwriter.thpp"
#include "icinga/service.hpp"
#include "base/configobject.hpp"
#include "base/tcpsocket.hpp"
#include "base/timer.hpp"
#include <fstream>
#include <boost/regex.hpp>

namespace icinga
{

/**
 * An Icinga opentsdb writer.
 *
 * @ingroup perfdata
 */
class OpenTsdbWriter : public ObjectImpl<OpenTsdbWriter>
{
public:
	DECLARE_OBJECT(OpenTsdbWriter);
	DECLARE_OBJECTNAME(OpenTsdbWriter);

	static void StatsFunc(const Dictionary::Ptr& status, const Array::Ptr& perfdata);

protected:
	virtual void Start(bool runtimeCreated) override;

private:
	Stream::Ptr m_Stream;

	Timer::Ptr m_ReconnectTimer;

	/* for metric and tag name rules, see
	* http://opentsdb.net/docs/build/html/user_guide/writing.html#metrics-and-tags
	* as of writing this the rule is:
	*
	* - Strings are case sensitive, i.e. "Sys.Cpu.User" will be stored separately from "sys.cpu.user"
	* - Spaces are not allowed
	* - Only the following characters are allowed: a to z, A to Z, 0 to 9, -, _, ., /
	*   or Unicode letters (as per the specification)
	* - Metric and tags are not limited in length, though you should try to keep the values fairly short.
	*/
	const boost::regex replaceMetricTagRe("[^a-zA-Z0-9_./-]");


	void CheckResultHandler(const Checkable::Ptr& checkable, const CheckResult::Ptr& cr);
	void SendMetric(const String& metric, const std::map<String, String>& tags, double value, double ts);
	void SendPerfdata(const String& metric, const std::map<String, String>& tags, const CheckResult::Ptr& cr, double ts);
	static String EscapeMetricTag(const String& str);

	void ReconnectTimerHandler(void);
};

}

#endif /* OPENTSDBWRITER_H */
