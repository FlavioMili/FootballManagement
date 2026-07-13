// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <string>

class DatabaseException : public std::runtime_error
{
 public:
  using std::runtime_error::runtime_error;
};
