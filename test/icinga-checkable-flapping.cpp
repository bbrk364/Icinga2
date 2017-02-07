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

#include <boost/test/unit_test.hpp>
#include "icinga/host.hpp"
//#include <iostream>

using namespace icinga;

#ifdef I2_DEBUG
static CheckResult::Ptr MakeCheckResult(ServiceState state)
{
	CheckResult::Ptr cr = new CheckResult();

	cr->SetState(state);

	double now = Utility::GetTime();
	cr->SetScheduleStart(now);
	cr->SetScheduleEnd(now);
	cr->SetExecutionStart(now);
	cr->SetExecutionEnd(now);

	return cr;
}

void LogFlapping(const Checkable::Ptr& obj)
{
	std::cout << "Flapping status: is_flapping: " << obj->IsFlapping() << " percent: " << obj->GetFlappingCurrent()
	    << " positive: " << obj->GetFlappingPositive()
	    << " negative: " << obj->GetFlappingNegative() << std::endl;
}

void LogHostStatus(const Host::Ptr &host)
{
	std::cout << "Current status: state: " << host->GetState() << " state_type: " << host->GetStateType()
	    << "check attempt: " << host->GetCheckAttempt() << "/" << host->GetMaxCheckAttempts() << std::endl;
}
#endif /* I2_DEBUG */

BOOST_AUTO_TEST_SUITE(icinga_checkable_flapping)

BOOST_AUTO_TEST_CASE(host_not_flapping)
{
#ifndef I2_DEBUG
    BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
	std::cout << "Running test with a non-flapping host...\n";

	Host::Ptr host = new Host();
	host->SetName("test");
	host->SetEnableFlapping(true);
	host->SetMaxCheckAttempts(5);

	Utility::SetTime(0);

	// check if our first result has been processed
	host->ProcessCheckResult(MakeCheckResult(ServiceOK));

	BOOST_CHECK(host->GetState() == 0);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetLastCheck() == Utility::GetTime());
	BOOST_CHECK(host->GetLastCheckResult()->GetOutput() == "test output");

	BOOST_CHECK(host->GetFlappingPositive() == 0);
	BOOST_CHECK(host->GetFlappingNegative() == 0);

	LogFlapping(host);
	LogHostStatus(host);
	Utility::IncrementTime(60);

	// watch the state being stable
	int i = 0;
	while (i++ < 10) {
		host->ProcessCheckResult(MakeCheckResult(ServiceOK));

		BOOST_CHECK(host->GetState() == 0);
		BOOST_CHECK(host->GetCheckAttempt() == 1);
		BOOST_CHECK(host->GetLastCheck() == Utility::GetTime());
		BOOST_CHECK(host->GetStateType() == StateTypeHard);

		// flapping detection should show negative state change
		BOOST_CHECK(host->GetFlappingPositive() == 0);
		BOOST_CHECK(host->GetFlappingNegative() == Utility::GetTime());

		LogFlapping(host);
		LogHostStatus(host);
		Utility::IncrementTime(60);
	}
#endif /* I2_DEBUG */
}

BOOST_AUTO_TEST_CASE(host_flapping_bad_hard)
{
#ifndef I2_DEBUG
    BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
	std::cout << "Running test with bad hard states...\n";

	// some configuration
	double interval = 60;
	int max_attempts = 5;
	int attempts_broken = max_attempts + 3;
	int attempts_ok = 3;

	Host::Ptr host = new Host();
	host->SetName("test");
	host->SetEnableFlapping(true);
	host->SetMaxCheckAttempts(max_attempts);

	Utility::SetTime(0);
	double now = 0;
	double flapping_positive = 0;
	double flapping_negative = 0;

	// now let the host get bad, any stay bad for a few minutes
	for (int i = 0; i <= attempts_broken + attempts_ok; i++) {
		ServiceState state = ServiceOK;

		if (i > 0 && i <= attempts_broken)
			state = ServiceCritical;

		host->ProcessCheckResult(MakeCheckResult(state));

		BOOST_CHECK(host->GetLastCheck() == now);

		if (i == 0) {
			BOOST_CHECK(host->GetState() == 0);
			BOOST_CHECK(host->GetCheckAttempt() == 1);
			BOOST_CHECK(host->GetStateType() == StateTypeHard);

		} else if (i <= max_attempts) {
			BOOST_CHECK(host->GetState() == 1);
			BOOST_CHECK(host->GetCheckAttempt() == i);
			BOOST_CHECK(host->GetStateType() == StateTypeSoft);

			BOOST_CHECK(host->GetFlappingPositive() == 0);
			BOOST_CHECK(host->GetFlappingNegative() == 0);
		} else if (i <= attempts_broken) {
			if (i == max_attempts + 1) // first hard state
				flapping_negative += (interval * max_attempts); // note: full time in soft counts as changing

			BOOST_CHECK(host->GetState() == 1);
			BOOST_CHECK(host->GetCheckAttempt() == 1);
			BOOST_CHECK(host->GetStateType() == StateTypeHard);

			flapping_negative += interval;
			BOOST_CHECK(host->GetFlappingPositive() == 0); // TODO: should this have been increased?
			BOOST_CHECK(host->GetFlappingNegative() == flapping_negative);
		} else {
			BOOST_CHECK(host->GetState() == 0);
			BOOST_CHECK(host->GetCheckAttempt() == 1);
			BOOST_CHECK(host->GetStateType() == StateTypeHard);

			if (i == attempts_broken + 1) // first ok state
				flapping_positive += interval; // state change
			else
				flapping_negative += interval; // no state change

			BOOST_CHECK(host->GetFlappingPositive() == flapping_positive);
			BOOST_CHECK(host->GetFlappingNegative() == flapping_negative);
		}

		LogFlapping(host);
		LogHostStatus(host);
		Utility::SetTime(now += interval);
	}
#endif /* I2_DEBUG */
}

BOOST_AUTO_TEST_CASE(host_flapping_bad_soft_ok)
{
#ifndef I2_DEBUG
    BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
    std::cout << "Running test with bad soft states...\n";

	// configuration
	double interval = 60;
	int max_attempts = 3;
	int checks = 10;

	Host::Ptr host = new Host();
	host->SetName("test");
	host->SetEnableFlapping(true);
	host->SetMaxCheckAttempts(max_attempts);

	Utility::SetTime(0);
	double now = 0;
	double flapping_positive = 0;
	double flapping_negative = 0;

	// let the host flap between 1st soft and an ok state
	for (int i = 0; i < checks; i++) {
		ServiceState state = (i % 2 == 0) ? ServiceOK : ServiceCritical;
		host->ProcessCheckResult(MakeCheckResult(state));

		BOOST_CHECK(host->GetState() == (state == ServiceCritical) ? ServiceWarning : ServiceOK); // note: host state here
		BOOST_CHECK(host->GetLastCheck() == now);

		if (i % 2 == 0) {
			BOOST_CHECK(host->GetCheckAttempt() == 1);
			BOOST_CHECK(host->GetStateType() == StateTypeHard);

			// only increments on hard states, after the first
			if (i > 0)
				flapping_positive += 2 * interval;
		}
		else {
			BOOST_CHECK(host->GetCheckAttempt() == 1);
			BOOST_CHECK(host->GetStateType() == StateTypeSoft);
		}

		BOOST_CHECK(host->GetFlappingPositive() == flapping_positive);
		BOOST_CHECK(host->GetFlappingNegative() == flapping_negative);

		LogFlapping(host);
		LogHostStatus(host);
		Utility::SetTime(now += interval);
	}
#endif /* I2_DEBUG */
}

BOOST_AUTO_TEST_SUITE_END()
