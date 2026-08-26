/*
 * helper_functions.h
 *
 * Copyright (C) 2023 charlie-foxtrot
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _HELPER_FUNCTIONS_H
#define _HELPER_FUNCTIONS_H

#include <cstddef>  // size_t
#include <ctime>    // struct tm
#include <string>

// Bytes of unread data in an input ring buffer, where `start` is the read
// position, `end` the write position and `size` the buffer length.
size_t ring_buffer_used(size_t start, size_t end, size_t size);

// Output wave samples that predate a retune: everything still queued in the input
// ring buffer plus whatever the driver has captured but not yet delivered.
size_t stale_samples_after_retune(size_t buffered_bytes, size_t latency_bytes, size_t bytes_per_wave_sample);

bool dir_exists(const std::string& dir_path);
bool file_exists(const std::string& file_path);
bool make_dir(const std::string& dir_path);
bool make_subdirs(const std::string& basedir, const std::string& subdirs);
std::string make_dated_subdirs(const std::string& basedir, const struct tm* time);

#endif /* _HELPER_FUNCTIONS_H */
