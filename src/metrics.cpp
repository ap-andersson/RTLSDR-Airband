/*
 * metrics.cpp
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

#include "metrics.h"

#include <inttypes.h>

using namespace std;

template <typename T>
static void relaxed_add(atomic<T>& a, T value) {
    a.store(a.load(memory_order_relaxed) + value, memory_order_relaxed);
}

Histogram::Histogram(initializer_list<double> upper_bounds) : bounds_(upper_bounds), counts_(new atomic<uint64_t>[upper_bounds.size() + 1]()), sum_(0.0) {}

void Histogram::observe(double value) {
    size_t i = 0;
    while (i < bounds_.size() && value > bounds_[i]) {
        i++;
    }
    relaxed_add<uint64_t>(counts_[i], 1);
    relaxed_add(sum_, value);
}

uint64_t Histogram::cumulative_count(size_t i) const {
    uint64_t total = 0;
    for (size_t b = 0; b <= i; b++) {
        total += counts_[b].load(memory_order_relaxed);
    }
    return total;
}

TransmissionStats::TransmissionStats() : snr_db({3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60}), duration_seconds({0.25, 0.5, 1, 2, 3, 5, 8, 13, 20, 30, 60}) {}

void TransmissionStats::record(float snr, float duration) {
    snr_db.observe(snr);
    duration_seconds.observe(duration);
}

InputStats::InputStats() : samples(0), clipped_samples(0), power_sum(0.0) {}

void InputStats::add_8bit(const unsigned char* buf, size_t sample_count, bool is_signed) {
    // Flipping the sign bit turns signed samples into offset binary, so both formats
    // have their rails at 0 and 255 and their midpoint at 127.5.
    const unsigned char flip = is_signed ? 0x80 : 0x00;
    uint64_t clipped = 0;
    uint64_t power = 0;
    for (size_t i = 0; i < sample_count; i++) {
        const unsigned char re = buf[2 * i] ^ flip;
        const unsigned char im = buf[2 * i + 1] ^ flip;
        // +1 wraps 255 to 0, so <= 1 matches exactly the two rails
        clipped += ((unsigned char)(re + 1) <= 1) | ((unsigned char)(im + 1) <= 1);
        const int32_t dre = 2 * re - 255;
        const int32_t dim = 2 * im - 255;
        power += (uint64_t)(dre * dre + dim * dim);
    }
    relaxed_add<uint64_t>(samples, sample_count);
    relaxed_add<uint64_t>(clipped_samples, clipped);
    relaxed_add(power_sum, (double)power / (2.0 * 255.0 * 255.0));
}

string escape_label_value(const char* value) {
    string out;
    for (const char* p = value; *p; p++) {
        switch (*p) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            default:
                out += *p;
        }
    }
    return out;
}

void write_metric_header(FILE* f, const char* name, const char* type, const char* help) {
    fprintf(f, "# HELP %s %s\n# TYPE %s %s\n", name, help, name, type);
}

static void write_name(FILE* f, const char* name, const string& labels) {
    if (labels.empty()) {
        fprintf(f, "%s", name);
    } else {
        fprintf(f, "%s{%s}", name, labels.c_str());
    }
}

void write_sample(FILE* f, const char* name, const string& labels, double value) {
    write_name(f, name, labels);
    fprintf(f, "\t%.3f\n", value);
}

void write_sample(FILE* f, const char* name, const string& labels, uint64_t value) {
    write_name(f, name, labels);
    fprintf(f, "\t%" PRIu64 "\n", value);
}

void write_histogram(FILE* f, const char* name, const string& labels, const Histogram& h) {
    const string bucket_name = string(name) + "_bucket";
    const string prefix = labels.empty() ? "" : labels + ",";
    char le[32];
    for (size_t i = 0; i <= h.bucket_count(); i++) {
        if (i < h.bucket_count()) {
            snprintf(le, sizeof(le), "%g", h.upper_bound(i));
        } else {
            snprintf(le, sizeof(le), "+Inf");
        }
        write_sample(f, bucket_name.c_str(), prefix + "le=\"" + le + "\"", h.cumulative_count(i));
    }
    write_sample(f, (string(name) + "_sum").c_str(), labels, h.sum());
    write_sample(f, (string(name) + "_count").c_str(), labels, h.cumulative_count(h.bucket_count()));
}
