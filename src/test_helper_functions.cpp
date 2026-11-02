/*
 * test_output.cpp
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

#include "test_base_class.h"

#include <libconfig.h++>

#include "helper_functions.h"

using namespace std;

class HelperFunctionsTest : public TestBaseClass {
   protected:
    void SetUp(void) { TestBaseClass::SetUp(); }

    void create_file(const string& filepath) {
        fclose(fopen(filepath.c_str(), "wb"));
        EXPECT_TRUE(file_exists(filepath));
    }
};

TEST_F(HelperFunctionsTest, dir_exists_true) {
    EXPECT_TRUE(dir_exists(temp_dir));
}

TEST_F(HelperFunctionsTest, dir_exists_false) {
    EXPECT_FALSE(dir_exists("/not/a/real/dir"));
}

TEST_F(HelperFunctionsTest, dir_exists_not_dir) {
    string file_in_dir = temp_dir + "/some_file";
    create_file(file_in_dir);
    EXPECT_FALSE(dir_exists(file_in_dir));
}

TEST_F(HelperFunctionsTest, file_exists_true) {
    string file_in_dir = temp_dir + "/some_file";
    create_file(file_in_dir);
    EXPECT_TRUE(file_exists(file_in_dir));
}

TEST_F(HelperFunctionsTest, file_exists_false) {
    EXPECT_FALSE(file_exists(temp_dir + "/nothing"));
}

TEST_F(HelperFunctionsTest, file_exists_not_file) {
    EXPECT_FALSE(file_exists(temp_dir));
    EXPECT_TRUE(dir_exists(temp_dir));
}

TEST_F(HelperFunctionsTest, make_dir_normal) {
    const string dir_path = temp_dir + "/a";
    EXPECT_FALSE(dir_exists(dir_path));
    EXPECT_TRUE(make_dir(dir_path));
    EXPECT_TRUE(dir_exists(dir_path));
}

TEST_F(HelperFunctionsTest, make_dir_exists) {
    EXPECT_TRUE(dir_exists(temp_dir));
    EXPECT_TRUE(make_dir(temp_dir));
    EXPECT_TRUE(dir_exists(temp_dir));
}

TEST_F(HelperFunctionsTest, make_dir_empty) {
    EXPECT_FALSE(make_dir(""));
}

TEST_F(HelperFunctionsTest, make_dir_fail) {
    EXPECT_FALSE(make_dir("/this/path/does/not/exist"));
}

TEST_F(HelperFunctionsTest, make_dir_file_in_the_way) {
    const string file_path = temp_dir + "/some_file";
    create_file(file_path);
    EXPECT_FALSE(make_dir(file_path));
}

TEST_F(HelperFunctionsTest, make_subdirs_exists) {
    EXPECT_TRUE(dir_exists(temp_dir));
    EXPECT_TRUE(make_subdirs(temp_dir, ""));
    EXPECT_TRUE(dir_exists(temp_dir));
}

TEST_F(HelperFunctionsTest, make_subdirs_one_subdir) {
    const string path = "bob";
    EXPECT_FALSE(dir_exists(temp_dir + "/" + path));
    EXPECT_TRUE(make_subdirs(temp_dir, path));
    EXPECT_TRUE(dir_exists(temp_dir + "/" + path));
}

TEST_F(HelperFunctionsTest, make_subdirs_multiple_subdir) {
    const string path = "bob/joe/sam";
    EXPECT_FALSE(dir_exists(temp_dir + "/" + path));
    EXPECT_TRUE(make_subdirs(temp_dir, path));
    EXPECT_TRUE(dir_exists(temp_dir + "/" + path));
}

TEST_F(HelperFunctionsTest, make_subdirs_file_in_the_way) {
    const string file_in_dir = temp_dir + "/some_file";
    create_file(file_in_dir);
    EXPECT_TRUE(file_exists(file_in_dir));
    EXPECT_FALSE(make_subdirs(temp_dir, "some_file/some_dir"));
    EXPECT_FALSE(dir_exists(file_in_dir));
    EXPECT_TRUE(file_exists(file_in_dir));
}

TEST_F(HelperFunctionsTest, make_subdirs_create_base) {
    EXPECT_FALSE(dir_exists(temp_dir + "/base_dir/a"));
    EXPECT_TRUE(make_subdirs(temp_dir + "/base_dir", "a"));
    EXPECT_TRUE(dir_exists(temp_dir + "/base_dir/a"));
}

TEST_F(HelperFunctionsTest, make_subdirs_extra_slashes) {
    EXPECT_FALSE(dir_exists(temp_dir + "/a/b/c/d"));
    EXPECT_TRUE(make_subdirs(temp_dir, "///a/b////c///d"));
    EXPECT_TRUE(dir_exists(temp_dir + "/a/b/c/d"));
}

TEST_F(HelperFunctionsTest, make_dated_subdirs_normal) {
    struct tm time_struct;

    strptime("2010-3-7", "%Y-%m-%d", &time_struct);

    const string dir_path = temp_dir + "/2010/03/07";

    EXPECT_FALSE(dir_exists(dir_path));
    EXPECT_EQ(make_dated_subdirs(temp_dir, &time_struct), dir_path);
    EXPECT_TRUE(dir_exists(dir_path));
}

TEST_F(HelperFunctionsTest, make_dated_subdirs_fail) {
    struct tm time_struct;

    strptime("2010-3-7", "%Y-%m-%d", &time_struct);

    EXPECT_EQ(make_dated_subdirs("/invalid/base/dir", &time_struct), "");
}

TEST_F(HelperFunctionsTest, make_dated_subdirs_some_exist) {
    struct tm time_struct;

    const string dir_through_month = temp_dir + "/2010/03/";

    strptime("2010-3-7", "%Y-%m-%d", &time_struct);
    EXPECT_EQ(make_dated_subdirs(temp_dir, &time_struct), dir_through_month + "07");

    EXPECT_TRUE(dir_exists(dir_through_month));
    EXPECT_FALSE(dir_exists(dir_through_month + "08"));

    strptime("2010-3-8", "%Y-%m-%d", &time_struct);
    EXPECT_EQ(make_dated_subdirs(temp_dir, &time_struct), dir_through_month + "08");
    EXPECT_TRUE(dir_exists(dir_through_month + "08"));
}

TEST_F(HelperFunctionsTest, ring_buffer_used_no_wrap) {
    EXPECT_EQ(ring_buffer_used(0, 0, 1000), (size_t)0);
    EXPECT_EQ(ring_buffer_used(100, 100, 1000), (size_t)0);
    EXPECT_EQ(ring_buffer_used(0, 250, 1000), (size_t)250);
    EXPECT_EQ(ring_buffer_used(100, 900, 1000), (size_t)800);
}

TEST_F(HelperFunctionsTest, ring_buffer_used_wrapped) {
    EXPECT_EQ(ring_buffer_used(900, 100, 1000), (size_t)200);
    EXPECT_EQ(ring_buffer_used(999, 0, 1000), (size_t)1);
}

TEST_F(HelperFunctionsTest, stale_samples_after_retune) {
    // buffered data and driver latency both count towards the stale window
    EXPECT_EQ(stale_samples_after_retune(0, 0, 640), (size_t)0);
    EXPECT_EQ(stale_samples_after_retune(6400, 0, 640), (size_t)10);
    EXPECT_EQ(stale_samples_after_retune(0, 320000, 640), (size_t)500);
    EXPECT_EQ(stale_samples_after_retune(6400, 320000, 640), (size_t)510);
}

TEST_F(HelperFunctionsTest, stale_samples_after_retune_partial_sample) {
    // a partial wave sample cannot be dropped, so it rounds down
    EXPECT_EQ(stale_samples_after_retune(639, 0, 640), (size_t)0);
    EXPECT_EQ(stale_samples_after_retune(1279, 0, 640), (size_t)1);
}

TEST_F(HelperFunctionsTest, stale_samples_after_retune_zero_bytes_per_sample) {
    // guard against a division by zero if the input rate is not known yet
    EXPECT_EQ(stale_samples_after_retune(6400, 320000, 0), (size_t)0);
}

TEST_F(HelperFunctionsTest, stale_window_tracks_first_and_last_stale_sample) {
    stale_window_t w;
    stale_window_reset(&w);
    EXPECT_EQ(w.begin, -1);

    stale_window_add(&w, 10, 0);  // nothing stale yet
    EXPECT_EQ(w.begin, -1);

    stale_window_add(&w, 600, 1);
    stale_window_add(&w, 601, 1);
    stale_window_add(&w, 602, 1);
    EXPECT_EQ(w.begin, 600);
    EXPECT_EQ(w.end, 603);

    // GPU FFT builds consume FFT_BATCH samples per iteration
    stale_window_reset(&w);
    stale_window_add(&w, 250, 250);
    stale_window_add(&w, 500, 100);
    EXPECT_EQ(w.begin, 250);
    EXPECT_EQ(w.end, 600);
}

TEST_F(HelperFunctionsTest, plan_retune_batch_no_retune) {
    stale_window_t w;
    stale_window_reset(&w);
    retune_plan_t p = plan_retune_batch(w, false, 0, 1000);
    EXPECT_EQ(p.discard_from, p.discard_to);
    EXPECT_FALSE(p.switch_freq);
}

TEST_F(HelperFunctionsTest, plan_retune_batch_retune_mid_batch) {
    // Retune after 600 samples of the batch, 300 stale samples follow, then 100 fresh ones.
    // The first 600 are from the old frequency as well, so they must not reach the new
    // frequency's squelch - only the last 100 may.
    stale_window_t w;
    stale_window_reset(&w);
    for (int pos = 600; pos < 900; pos++) {
        stale_window_add(&w, pos, 1);
    }
    retune_plan_t p = plan_retune_batch(w, true, 0, 1000);
    EXPECT_TRUE(p.switch_freq);
    EXPECT_EQ(p.discard_from, 0);
    EXPECT_EQ(p.discard_to, 900);
}

TEST_F(HelperFunctionsTest, plan_retune_batch_stale_continues_into_next_batch) {
    // Retune after 600 samples, still stale at the end of the batch: the first 600 samples
    // belong to the old frequency and are processed as such, the rest is dropped.
    stale_window_t w;
    stale_window_reset(&w);
    for (int pos = 600; pos < 1000; pos++) {
        stale_window_add(&w, pos, 1);
    }
    retune_plan_t p = plan_retune_batch(w, true, 250, 1000);
    EXPECT_FALSE(p.switch_freq);
    EXPECT_EQ(p.discard_from, 600);
    EXPECT_EQ(p.discard_to, 1000);
}

TEST_F(HelperFunctionsTest, plan_retune_batch_stale_ends_in_following_batch) {
    // continuation of the case above: 250 stale samples at the head of the next batch
    stale_window_t w;
    stale_window_reset(&w);
    for (int pos = 0; pos < 250; pos++) {
        stale_window_add(&w, pos, 1);
    }
    retune_plan_t p = plan_retune_batch(w, true, 0, 1000);
    EXPECT_TRUE(p.switch_freq);
    EXPECT_EQ(p.discard_from, 0);
    EXPECT_EQ(p.discard_to, 250);
}

TEST_F(HelperFunctionsTest, plan_retune_batch_whole_batch_stale) {
    stale_window_t w;
    stale_window_reset(&w);
    for (int pos = 0; pos < 1000; pos++) {
        stale_window_add(&w, pos, 1);
    }
    retune_plan_t p = plan_retune_batch(w, true, 300, 1000);
    EXPECT_FALSE(p.switch_freq);
    EXPECT_EQ(p.discard_from, 0);
    EXPECT_EQ(p.discard_to, 1000);
}

TEST_F(HelperFunctionsTest, plan_retune_batch_retune_after_last_sample_of_batch) {
    // The retune was marked but none of its stale samples are in this batch yet: the
    // whole batch is from the old frequency.
    stale_window_t w;
    stale_window_reset(&w);
    retune_plan_t p = plan_retune_batch(w, true, 500, 1000);
    EXPECT_FALSE(p.switch_freq);
    EXPECT_EQ(p.discard_from, p.discard_to);
}

TEST_F(HelperFunctionsTest, plan_retune_batch_retune_without_stale_samples) {
    // Nothing was buffered (e.g. file input), so it is unknown where in the batch the
    // retune happened - drop the whole batch rather than mix the two frequencies.
    stale_window_t w;
    stale_window_reset(&w);
    retune_plan_t p = plan_retune_batch(w, true, 0, 1000);
    EXPECT_TRUE(p.switch_freq);
    EXPECT_EQ(p.discard_from, 0);
    EXPECT_EQ(p.discard_to, 1000);
}

TEST_F(HelperFunctionsTest, mp3_bitrate_is_valid) {
    EXPECT_TRUE(mp3_bitrate_is_valid(8));
    EXPECT_TRUE(mp3_bitrate_is_valid(16));
    EXPECT_TRUE(mp3_bitrate_is_valid(48));
    EXPECT_TRUE(mp3_bitrate_is_valid(160));
}

TEST_F(HelperFunctionsTest, mp3_bitrate_is_invalid) {
    // rates that only exist for MPEG-1, plus values that are not mp3 rates at all
    EXPECT_FALSE(mp3_bitrate_is_valid(192));
    EXPECT_FALSE(mp3_bitrate_is_valid(320));
    EXPECT_FALSE(mp3_bitrate_is_valid(20));
    EXPECT_FALSE(mp3_bitrate_is_valid(0));
    EXPECT_FALSE(mp3_bitrate_is_valid(-16));
}

TEST_F(HelperFunctionsTest, should_close_split_file_max_duration) {
    EXPECT_TRUE(should_close_split_file(6001.0, 0.0, 1.0, 6000.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_duration_at_max) {
    EXPECT_FALSE(should_close_split_file(6000.0, 0.0, 1.0, 6000.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_after_min_duration) {
    EXPECT_TRUE(should_close_split_file(10.0, 0.6, 1.0, 6000.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_at_max) {
    EXPECT_FALSE(should_close_split_file(10.0, 0.5, 1.0, 6000.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_before_min_duration) {
    EXPECT_FALSE(should_close_split_file(0.5, 10.0, 1.0, 6000.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_duration_at_min) {
    EXPECT_FALSE(should_close_split_file(1.0, 10.0, 1.0, 6000.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_no_idle) {
    EXPECT_FALSE(should_close_split_file(10.0, 0.1, 1.0, 6000.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_max_time_past_max) {
    EXPECT_TRUE(should_close_split_file_max_time(6001.0, 6000.0));
}

TEST_F(HelperFunctionsTest, should_close_split_file_max_time_at_max) {
    EXPECT_FALSE(should_close_split_file_max_time(6000.0, 6000.0));
}

TEST_F(HelperFunctionsTest, should_close_split_file_max_time_under_max) {
    EXPECT_FALSE(should_close_split_file_max_time(10.0, 6000.0));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_past_max_idle) {
    EXPECT_TRUE(should_close_split_file_idle(10.0, 0.6, 1.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_at_max_idle) {
    EXPECT_FALSE(should_close_split_file_idle(10.0, 0.5, 1.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_at_min_duration) {
    EXPECT_FALSE(should_close_split_file_idle(1.0, 10.0, 1.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_under_min_duration) {
    EXPECT_FALSE(should_close_split_file_idle(0.5, 10.0, 1.0, 0.5));
}

TEST_F(HelperFunctionsTest, should_close_split_file_idle_ignores_max_time) {
    EXPECT_FALSE(should_close_split_file_idle(6001.0, 0.1, 1.0, 0.5));
}

TEST_F(HelperFunctionsTest, valid_split_file_times_ok) {
    EXPECT_TRUE(valid_split_file_times(1.0, 60.0, 0.5));
}

TEST_F(HelperFunctionsTest, valid_split_file_times_min_too_small) {
    EXPECT_FALSE(valid_split_file_times(0.5, 60.0, 0.5));
}

TEST_F(HelperFunctionsTest, valid_split_file_times_max_not_greater) {
    EXPECT_FALSE(valid_split_file_times(60.0, 60.0, 0.5));
    EXPECT_FALSE(valid_split_file_times(60.0, 30.0, 0.5));
}

TEST_F(HelperFunctionsTest, valid_split_file_times_idle_not_positive) {
    EXPECT_FALSE(valid_split_file_times(1.0, 60.0, 0.0));
    EXPECT_FALSE(valid_split_file_times(1.0, 60.0, -1.0));
}

TEST_F(HelperFunctionsTest, valid_stats_update_interval_positive) {
    EXPECT_TRUE(valid_stats_update_interval(15.0));
    EXPECT_TRUE(valid_stats_update_interval(0.5));
}

TEST_F(HelperFunctionsTest, valid_stats_update_interval_not_positive) {
    EXPECT_FALSE(valid_stats_update_interval(0.0));
    EXPECT_FALSE(valid_stats_update_interval(-1.0));
}

TEST_F(HelperFunctionsTest, setting_as_double_int) {
    libconfig::Config config;
    libconfig::Setting& s = config.getRoot().add("value", libconfig::Setting::TypeInt);
    s = 5;
    double value = 0.0;
    EXPECT_TRUE(setting_as_double(s, &value));
    EXPECT_DOUBLE_EQ(value, 5.0);
}

TEST_F(HelperFunctionsTest, setting_as_double_float) {
    libconfig::Config config;
    libconfig::Setting& s = config.getRoot().add("value", libconfig::Setting::TypeFloat);
    // 0.3 has no exact float representation, so this fails if the value is narrowed on the way out
    s = 0.3;
    double value = 0.0;
    EXPECT_TRUE(setting_as_double(s, &value));
    EXPECT_DOUBLE_EQ(value, 0.3);
}

TEST_F(HelperFunctionsTest, setting_as_double_bool_rejected) {
    libconfig::Config config;
    libconfig::Setting& s = config.getRoot().add("value", libconfig::Setting::TypeBoolean);
    s = true;
    double value = 0.0;
    EXPECT_FALSE(setting_as_double(s, &value));
}

TEST_F(HelperFunctionsTest, setting_as_double_string_rejected) {
    libconfig::Config config;
    libconfig::Setting& s = config.getRoot().add("value", libconfig::Setting::TypeString);
    s = "5.0";
    double value = 0.0;
    EXPECT_FALSE(setting_as_double(s, &value));
}

TEST_F(HelperFunctionsTest, setting_as_double_or_absent_uses_fallback) {
    libconfig::Config config;
    double value = 0.0;
    EXPECT_TRUE(setting_as_double_or(config.getRoot(), "split_max_idle_time", 0.5, &value));
    EXPECT_DOUBLE_EQ(value, 0.5);
}

TEST_F(HelperFunctionsTest, setting_as_double_or_present_overrides_fallback) {
    libconfig::Config config;
    config.getRoot().add("split_max_file_time", libconfig::Setting::TypeInt) = 1800;
    double value = 0.0;
    EXPECT_TRUE(setting_as_double_or(config.getRoot(), "split_max_file_time", 3600.0, &value));
    EXPECT_DOUBLE_EQ(value, 1800.0);
}

TEST_F(HelperFunctionsTest, setting_as_double_or_rejects_non_number) {
    libconfig::Config config;
    config.getRoot().add("split_max_idle_time", libconfig::Setting::TypeString) = "2.0";
    double value = 0.0;
    EXPECT_FALSE(setting_as_double_or(config.getRoot(), "split_max_idle_time", 0.5, &value));
}

TEST_F(HelperFunctionsTest, setting_as_bool_or_absent_uses_fallback) {
    libconfig::Config config;
    bool value = false;
    EXPECT_TRUE(setting_as_bool_or(config.getRoot(), "split_include_transmission_start", true, &value));
    EXPECT_TRUE(value);
}

TEST_F(HelperFunctionsTest, setting_as_bool_or_present_overrides_fallback) {
    libconfig::Config config;
    config.getRoot().add("split_include_transmission_start", libconfig::Setting::TypeBoolean) = false;
    bool value = true;
    EXPECT_TRUE(setting_as_bool_or(config.getRoot(), "split_include_transmission_start", true, &value));
    EXPECT_FALSE(value);
}

TEST_F(HelperFunctionsTest, setting_as_bool_or_rejects_non_bool) {
    libconfig::Config config;
    config.getRoot().add("int_value", libconfig::Setting::TypeInt) = 1;
    config.getRoot().add("string_value", libconfig::Setting::TypeString) = "true";
    bool value = false;
    EXPECT_FALSE(setting_as_bool_or(config.getRoot(), "int_value", false, &value));
    EXPECT_FALSE(setting_as_bool_or(config.getRoot(), "string_value", false, &value));
}

TEST_F(HelperFunctionsTest, valid_split_tx_drop_shorter_than_disabled) {
    EXPECT_TRUE(valid_split_tx_drop_shorter_than(0.0, 60.0));
}

TEST_F(HelperFunctionsTest, valid_split_tx_drop_shorter_than_below_max) {
    EXPECT_TRUE(valid_split_tx_drop_shorter_than(59.9, 60.0));
}

TEST_F(HelperFunctionsTest, valid_split_tx_drop_shorter_than_at_max) {
    EXPECT_FALSE(valid_split_tx_drop_shorter_than(60.0, 60.0));
}

TEST_F(HelperFunctionsTest, valid_split_tx_drop_shorter_than_negative) {
    EXPECT_FALSE(valid_split_tx_drop_shorter_than(-0.1, 60.0));
}

TEST_F(HelperFunctionsTest, should_drop_split_tx_short_whole_transmission) {
    EXPECT_TRUE(should_drop_split_tx(true, false, 0.4, 1.0));
}

TEST_F(HelperFunctionsTest, should_drop_split_tx_span_at_limit) {
    EXPECT_FALSE(should_drop_split_tx(true, false, 1.0, 1.0));
}

TEST_F(HelperFunctionsTest, should_drop_split_tx_long_transmission) {
    EXPECT_FALSE(should_drop_split_tx(true, false, 3.0, 1.0));
}

TEST_F(HelperFunctionsTest, should_drop_split_tx_disabled) {
    EXPECT_FALSE(should_drop_split_tx(true, false, 0.0, 0.0));
}

TEST_F(HelperFunctionsTest, should_drop_split_tx_keeps_first_part_that_continues) {
    EXPECT_FALSE(should_drop_split_tx(true, true, 0.4, 1.0));
}

TEST_F(HelperFunctionsTest, should_drop_split_tx_keeps_short_later_part) {
    EXPECT_FALSE(should_drop_split_tx(false, false, 0.4, 1.0));
}
