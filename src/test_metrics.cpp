/*
 * test_metrics.cpp
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

#include "test_base_class.h"

#include <stdio.h>
#include <stdlib.h>

#include "metrics.h"

using namespace std;

class MetricsTest : public TestBaseClass {
   protected:
    // Runs the given writer against an in-memory FILE* and returns what it wrote
    template <typename F>
    string capture(F writer) {
        char* buf = NULL;
        size_t len = 0;
        FILE* f = open_memstream(&buf, &len);
        writer(f);
        fclose(f);
        string out(buf, len);
        free(buf);
        return out;
    }
};

TEST_F(MetricsTest, histogram_starts_empty) {
    Histogram h({1.0, 2.0});
    ASSERT_EQ(h.bucket_count(), 2u);
    EXPECT_EQ(h.cumulative_count(0), 0u);
    EXPECT_EQ(h.cumulative_count(1), 0u);
    EXPECT_EQ(h.cumulative_count(2), 0u);
    EXPECT_EQ(h.sum(), 0.0);
}

TEST_F(MetricsTest, histogram_bucket_edges_are_inclusive) {
    Histogram h({1.0, 2.0});
    h.observe(0.5);
    h.observe(1.0);  // le="1" is inclusive
    h.observe(1.5);
    h.observe(2.0);
    h.observe(7.0);  // only in +Inf

    EXPECT_EQ(h.cumulative_count(0), 2u);
    EXPECT_EQ(h.cumulative_count(1), 4u);
    EXPECT_EQ(h.cumulative_count(2), 5u);
    EXPECT_DOUBLE_EQ(h.sum(), 12.0);
}

TEST_F(MetricsTest, transmission_stats_buckets) {
    TransmissionStats stats;

    ASSERT_EQ(stats.snr_db.bucket_count(), 20u);
    EXPECT_EQ(stats.snr_db.upper_bound(0), 3.0);
    EXPECT_EQ(stats.snr_db.upper_bound(19), 60.0);

    ASSERT_EQ(stats.duration_seconds.bucket_count(), 11u);
    EXPECT_EQ(stats.duration_seconds.upper_bound(0), 0.25);
    EXPECT_EQ(stats.duration_seconds.upper_bound(10), 60.0);
}

TEST_F(MetricsTest, transmission_stats_record) {
    TransmissionStats stats;
    stats.record(25.0f, 4.0f);
    stats.record(70.0f, 1.5f);

    EXPECT_EQ(stats.snr_db.cumulative_count(stats.snr_db.bucket_count()), 2u);
    // 70 dB is above the top (60 dB) bucket, so only visible in +Inf
    EXPECT_EQ(stats.snr_db.cumulative_count(stats.snr_db.bucket_count() - 1), 1u);
    EXPECT_DOUBLE_EQ(stats.snr_db.sum(), 95.0);
    EXPECT_DOUBLE_EQ(stats.duration_seconds.sum(), 5.5);
}

TEST_F(MetricsTest, input_stats_u8_clipping) {
    InputStats stats;
    // 4 complex samples: clean, I clipped low, Q clipped high, both clipped
    const unsigned char buf[] = {128, 127, 0, 140, 100, 255, 255, 0};
    stats.add_8bit(buf, 4, false);

    EXPECT_EQ(stats.samples.load(), 4u);
    EXPECT_EQ(stats.clipped_samples.load(), 3u);
}

TEST_F(MetricsTest, input_stats_s8_clipping) {
    InputStats stats;
    // signed rails are -128 (0x80) and 127 (0x7f); 0 and 0xff (-1) are mid-scale
    const unsigned char buf[] = {0x00, 0xff, 0x80, 0x01, 0x05, 0x7f, 0x10, 0x20};
    stats.add_8bit(buf, 4, true);

    EXPECT_EQ(stats.samples.load(), 4u);
    EXPECT_EQ(stats.clipped_samples.load(), 2u);
}

TEST_F(MetricsTest, input_stats_power) {
    InputStats stats;

    // mid-scale has (almost) no power
    const unsigned char quiet[] = {127, 128, 128, 127};
    stats.add_8bit(quiet, 2, false);
    EXPECT_LT(stats.power_sum.load(), 1e-4);

    // full scale on both I and Q is 1.0 per sample (0 dBFS)
    InputStats full;
    const unsigned char rails[] = {0, 255, 255, 0};
    full.add_8bit(rails, 2, false);
    EXPECT_DOUBLE_EQ(full.power_sum.load(), 2.0);

    // accumulates across calls
    full.add_8bit(rails, 2, false);
    EXPECT_DOUBLE_EQ(full.power_sum.load(), 4.0);
    EXPECT_EQ(full.samples.load(), 4u);
}

TEST_F(MetricsTest, escape_label_value) {
    EXPECT_EQ(escape_label_value("TOWER 118.805"), "TOWER 118.805");
    EXPECT_EQ(escape_label_value("say \"hi\""), "say \\\"hi\\\"");
    EXPECT_EQ(escape_label_value("a\\b"), "a\\\\b");
    EXPECT_EQ(escape_label_value("line1\nline2"), "line1\\nline2");
}

TEST_F(MetricsTest, write_metric_header) {
    string out = capture([](FILE* f) { write_metric_header(f, "foo_total", "counter", "Number of foos."); });
    EXPECT_EQ(out, "# HELP foo_total Number of foos.\n# TYPE foo_total counter\n");
}

TEST_F(MetricsTest, write_sample_formats) {
    // same layout as the existing stats file: name{labels}<TAB>value
    EXPECT_EQ(capture([](FILE* f) { write_sample(f, "level", "freq=\"1.000\"", 1.23456); }), "level{freq=\"1.000\"}\t1.235\n");
    EXPECT_EQ(capture([](FILE* f) { write_sample(f, "count", "device=\"0\"", (uint64_t)42); }), "count{device=\"0\"}\t42\n");
    EXPECT_EQ(capture([](FILE* f) { write_sample(f, "info", "", (uint64_t)1); }), "info\t1\n");
}

TEST_F(MetricsTest, write_histogram) {
    Histogram h({-3.0, 0.5});
    h.observe(-10.0);
    h.observe(0.25);
    h.observe(2.0);

    string out = capture([&h](FILE* f) { write_histogram(f, "x", "freq=\"1.000\"", h); });
    EXPECT_EQ(out,
              "x_bucket{freq=\"1.000\",le=\"-3\"}\t1\n"
              "x_bucket{freq=\"1.000\",le=\"0.5\"}\t2\n"
              "x_bucket{freq=\"1.000\",le=\"+Inf\"}\t3\n"
              "x_sum{freq=\"1.000\"}\t-7.750\n"
              "x_count{freq=\"1.000\"}\t3\n");
}
