/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "date_time.h"

#include <boost/date_time/posix_time/posix_time.hpp>

TimePoint getUtc()
{
    boost::posix_time::ptime t(
        boost::gregorian::day_clock::universal_day(),
        boost::posix_time::second_clock::universal_time().time_of_day());
    return std::chrono::system_clock::from_time_t(boost::posix_time::to_time_t(t));
}

TimePoint string2timepoint(const String &s)
{
    auto t = boost::posix_time::time_from_string(s);
    return std::chrono::system_clock::from_time_t(boost::posix_time::to_time_t(t));
}

time_t string2time_t(const String &s)
{
    return std::chrono::system_clock::to_time_t(string2timepoint(s));
}
