// Validate.h
//
// Copyright (c) 2025-2026 Quentin Dumont
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// Compile-time configuration sanity checks.
//
#pragma once

#ifdef SH1106_ON
  #define OLED_DISP_ON
#endif
#ifdef SSD1306_ON
  #ifdef OLED_DISP_ON
    #error "Select only one display type"
  #endif
  #define OLED_DISP_ON
#endif
#ifndef OLED_DISP_ON
  #error "Please select one display type"
#endif
