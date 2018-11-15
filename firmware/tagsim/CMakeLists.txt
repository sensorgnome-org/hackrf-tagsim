# Copyright 2012 Jared Boone <jared@sharebrained.com>
# Copyright 2018 John Brzustowski <admin@sensorgnome.org>
#
# This file is part of HackRF.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

cmake_minimum_required(VERSION 2.8.9)
set(CMAKE_TOOLCHAIN_FILE ../toolchain-arm-cortex-m.cmake)

project(tagsim)

include(../hackrf-common.cmake)

set(SRC_M4
	tagsim.c
	"${PATH_HACKRF_FIRMWARE_COMMON}/tuning.c"
	"${PATH_HACKRF_FIRMWARE_COMMON}/streaming.c"
	"${PATH_HACKRF_FIRMWARE_COMMON}/fault_handler.c"
	"${PATH_HACKRF_FIRMWARE_COMMON}/xapp058/lenval.c"
	"${PATH_HACKRF_FIRMWARE_COMMON}/xapp058/micro.c"
	"${PATH_HACKRF_FIRMWARE_COMMON}/xapp058/ports.c"
	"${PATH_HACKRF_FIRMWARE_COMMON}/rom_iap.c"
)

DeclareTargets()
