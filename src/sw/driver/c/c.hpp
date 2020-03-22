/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "c.h"
#include "../driver.h"

namespace sw
{

/*struct SW_CORE_API CDriver : IDriver
{
    using create_driver = sw_driver_t(*)(void);

    CDriver(create_driver cd);
    virtual ~CDriver();

    PackageId getPackageId() const override;
    bool canLoad(RawInputData &) const override;
    EntryPointsVector createEntryPoints(SwContext &, const std::vector<RawInput> &) const override;

private:
    sw_driver_t d;
};*/

}
