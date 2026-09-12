// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// This executable is intentionally invalid. It is built only by the sanitizer
// verification fixture and must never be linked into production or test code.
#include <memory>

int main(int argc, char**)
{
  auto values = std::make_unique<int[]>(1);
  const int invalidIndex = argc;  // argc is one when launched by CTest.
  values[invalidIndex] = 42;
  return values[0];
}
