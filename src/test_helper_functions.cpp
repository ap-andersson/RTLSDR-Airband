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
