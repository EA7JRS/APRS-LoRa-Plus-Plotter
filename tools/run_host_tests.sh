#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later
# Compila y ejecuta las pruebas del nucleo en el PC (sin Arduino).
set -e
cd "$(dirname "$0")/.."
mkdir -p .build
g++ -std=gnu++17 -O2 -Wall -Wextra -Werror -Ilib/plotter_core/src lib/plotter_core/src/*.cpp test/host/test_core.cpp -o .build/test_core
./.build/test_core
