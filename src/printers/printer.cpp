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

#include "printer.h"

#include "cmake.h"
#include "settings.h"

const std::vector<String> configuration_types = { "DEBUG", "MINSIZEREL", "RELEASE", "RELWITHDEBINFO" };
const std::vector<String> configuration_types_normal = { "Debug", "MinSizeRel", "Release", "RelWithDebInfo" };
const std::vector<String> configuration_types_no_rel = { "DEBUG", "MINSIZEREL", "RELWITHDEBINFO" };

std::unique_ptr<Printer> Printer::create(PrinterType type)
{
    switch (type)
    {
    case PrinterType::CMake:
        return std::make_unique<CMakePrinter>();
    default:
        throw std::runtime_error("Undefined printer");
    }
}

Printer::Printer()
    : settings(Settings::get_local_settings())
{
}
