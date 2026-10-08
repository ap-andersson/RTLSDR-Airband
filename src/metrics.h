/*
 * metrics.h
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

#ifndef _METRICS_H
#define _METRICS_H

#include <stdint.h>
#include <stdio.h>

#include <atomic>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

// Everything here is written by a single demod thread and read concurrently by the
// stats writer, so updates are relaxed load + store rather than read-modify-write.

// Prometheus-style histogram with fixed upper bounds and an implicit +Inf bucket.
class Histogram {
   public:
    explicit Histogram(std::initializer_list<double> upper_bounds);

    void observe(double value);

    size_t bucket_count(void) const { return bounds_.size(); }
    double upper_bound(size_t i) const { return bounds_[i]; }
    uint64_t cumulative_count(size_t i) const;  // i == bucket_count() is the +Inf bucket
    double sum(void) const { return sum_.load(std::memory_order_relaxed); }

   private:
    std::vector<double> bounds_;
    std::unique_ptr<std::atomic<uint64_t>[]> counts_;  // per bucket, not cumulative
    std::atomic<double> sum_;
};

// Per frequency statistics of completed transmissions.  The duration histogram's
// _sum doubles as the total airtime.
struct TransmissionStats {
    TransmissionStats();
    void record(float snr_db, float duration_seconds);

    Histogram snr_db;
    Histogram duration_seconds;
};

// Per device statistics of the raw 8-bit I/Q samples.
struct InputStats {
    InputStats();

    // Counts sample_count complex (I, Q byte pair) samples starting at buf
    void add_8bit(const unsigned char* buf, size_t sample_count, bool is_signed);

    std::atomic<uint64_t> samples;
    std::atomic<uint64_t> clipped_samples;  // I or Q at either rail
    std::atomic<double> power_sum;          // sum of (I^2 + Q^2) / 2, normalized so full scale is 1.0
};

std::string escape_label_value(const char* value);

// Writers for the Prometheus text format, using the same "name{labels}<TAB>value" layout
// as the stats file always has.  labels is a pre-formatted (and escaped) label list, may be empty.
void write_metric_header(FILE* f, const char* name, const char* type, const char* help);
void write_sample(FILE* f, const char* name, const std::string& labels, double value);
void write_sample(FILE* f, const char* name, const std::string& labels, uint64_t value);
void write_histogram(FILE* f, const char* name, const std::string& labels, const Histogram& h);

#endif /* _METRICS_H */
