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

// Batch positions of the pre-retune ("stale") wave samples in the batch being collected.
// A retune usually lands mid-batch, so they are preceded by samples that were consumed
// before the retune and are just as much from the old frequency.
struct stale_window_t {
    int begin;  // -1 if there are no stale samples in this batch
    int end;    // one past the last stale sample
};
void stale_window_reset(stale_window_t* w);
void stale_window_add(stale_window_t* w, int batch_pos, int count);

struct retune_plan_t {
    int discard_from;  // batch positions [discard_from, discard_to) are dropped
    int discard_to;
    bool switch_freq;  // apply the pending frequency to the samples after discard_to
};
// stale_remaining: stale samples of the pending retune not consumed yet
retune_plan_t plan_retune_batch(const stale_window_t& w, bool retune_pending, int stale_remaining, int batch_size);

// Whether `bitrate` (in kbps) is one the MPEG-2 / MPEG-2.5 Layer III sample rates
// that MP3_RATE selects can actually be encoded at.
bool mp3_bitrate_is_valid(int bitrate);

namespace libconfig {
class Setting;
}

bool dir_exists(const std::string& dir_path);
bool file_exists(const std::string& file_path);
bool make_dir(const std::string& dir_path);
bool make_subdirs(const std::string& basedir, const std::string& subdirs);
std::string make_dated_subdirs(const std::string& basedir, const struct tm* time);

bool should_close_split_file(double duration_sec, double idle_sec, double split_min_file_time, double split_max_file_time, double split_max_idle_time);
bool should_close_split_file_max_time(double duration_sec, double split_max_file_time);
bool should_close_split_file_idle(double duration_sec, double idle_sec, double split_min_file_time, double split_max_idle_time);

bool valid_split_file_times(double split_min_file_time, double split_max_file_time, double split_max_idle_time);
extern const char* const split_file_times_constraint;
bool valid_stats_update_interval(double stats_update_interval);
bool valid_split_tx_drop_shorter_than(double split_tx_drop_shorter_than, double split_max_file_time);
extern const char* const split_tx_drop_shorter_than_constraint;
bool should_drop_split_tx(bool first_file, bool continues, double tx_span_sec, double split_tx_drop_shorter_than);
bool setting_as_double(const libconfig::Setting& setting, double* value);
bool setting_as_double_or(const libconfig::Setting& parent, const char* key, double fallback, double* value);
bool setting_as_bool_or(const libconfig::Setting& parent, const char* key, bool fallback, bool* value);

#endif /* _HELPER_FUNCTIONS_H */
