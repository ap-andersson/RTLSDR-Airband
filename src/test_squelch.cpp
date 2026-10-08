/*
 * test_squelch.cpp
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

#include "generate_signal.h"
#include "test_base_class.h"

#include "squelch.h"

using namespace std;

class SquelchTest : public TestBaseClass {
   protected:
    void SetUp(void) {
        TestBaseClass::SetUp();

        raw_no_signal_sample = 0.05;
        raw_signal_sample = 0.75;
    }

    void TearDown(void) { TestBaseClass::TearDown(); }

    // send through "no signal" samples to get noise floor down
    void send_samples_for_noise_floor(Squelch& squelch) {
        while (squelch.noise_level() > 1.01 * raw_no_signal_sample) {
            squelch.process_raw_sample(raw_no_signal_sample);
        }
        ASSERT_LE(squelch.noise_level(), 1.01 * raw_no_signal_sample);
        ASSERT_GT(raw_signal_sample, squelch.squelch_level());
    }

    float raw_no_signal_sample;
    float raw_signal_sample;
};

TEST_F(SquelchTest, default_object) {
    Squelch squelch;
    EXPECT_EQ(squelch.open_count(), 0);
}

TEST_F(SquelchTest, noise_floor) {
    Squelch squelch;

    // noise floor starts high
    EXPECT_GT(squelch.noise_level(), 10.0 * raw_no_signal_sample);

    // noise floor drifts down towards (but never at) the incoming raw sample level
    float last_noise_level, this_noise_level;
    this_noise_level = squelch.noise_level();
    do {
        last_noise_level = this_noise_level;

        // not all samples update noise floor
        for (int j = 0; j < 25; ++j) {
            squelch.process_raw_sample(raw_no_signal_sample);
        }

        this_noise_level = squelch.noise_level();
        ASSERT_LE(this_noise_level, last_noise_level);
    } while (this_noise_level != last_noise_level);

    // noise floor ends up close to the incoming level
    EXPECT_LT(squelch.noise_level(), 1.01 * raw_no_signal_sample);
}

TEST_F(SquelchTest, normal_operation) {
    Squelch squelch;

    // send through "no signal" samples to get noise floor down
    send_samples_for_noise_floor(squelch);
    ASSERT_LE(squelch.noise_level(), 1.01 * raw_no_signal_sample);
    ASSERT_GT(raw_signal_sample, squelch.squelch_level());

    // send through "signal" samples and squelch should open shortly
    for (int i = 0; i < 500 && !squelch.is_open(); ++i) {
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.is_open());
    ASSERT_TRUE(squelch.should_process_audio());

    // send through a bunch more "signal" values and squelch stays open
    for (int i = 0; i < 1000; ++i) {
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.is_open());
    ASSERT_TRUE(squelch.should_process_audio());

    // send through "no signal" samples and squelch should close quickly
    for (int i = 0; i < 100 && squelch.is_open(); ++i) {
        squelch.process_raw_sample(raw_no_signal_sample);
    }
    ASSERT_FALSE(squelch.is_open());
    ASSERT_FALSE(squelch.should_process_audio());
}

TEST_F(SquelchTest, dead_spot) {
    Squelch squelch;

    send_samples_for_noise_floor(squelch);

    // send through "signal" samples and squelch should open shortly
    for (int i = 0; i < 500 && !squelch.is_open(); ++i) {
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.is_open());
    ASSERT_TRUE(squelch.should_process_audio());

    // send through a bunch more "signal" values and squelch stays open
    for (int i = 0; i < 1000; ++i) {
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.is_open());
    ASSERT_TRUE(squelch.should_process_audio());

    // send through a dead spot of "no signal" and squelch should stay open
    for (int i = 0; i < 50; ++i) {
        squelch.process_raw_sample(raw_no_signal_sample);
        ASSERT_TRUE(squelch.is_open());
        ASSERT_TRUE(squelch.should_process_audio());
    }

    // send go back to "signal" samples and squelch stays open
    for (int i = 0; i < 1000; ++i) {
        squelch.process_raw_sample(raw_signal_sample);
        ASSERT_TRUE(squelch.is_open());
        ASSERT_TRUE(squelch.should_process_audio());
    }
}

TEST_F(SquelchTest, should_process_audio) {
    Squelch squelch;

    send_samples_for_noise_floor(squelch);

    // should_process_audio is true as soon as squelch opens
    for (int i = 0; i < 500 && !squelch.is_open(); ++i) {
        ASSERT_FALSE(squelch.should_process_audio());
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.is_open());
    ASSERT_TRUE(squelch.should_process_audio());

    // and stays true until fully closed
    for (int i = 0; i < 100 && squelch.is_open(); ++i) {
        ASSERT_TRUE(squelch.should_process_audio());
        squelch.process_raw_sample(raw_no_signal_sample);
    }
    ASSERT_FALSE(squelch.is_open());
    ASSERT_FALSE(squelch.should_process_audio());
}

TEST_F(SquelchTest, good_ctcss) {
    float tone = CTCSS::standard_tones[5];
    float sample_rate = 8000;

    Squelch squelch;
    squelch.set_ctcss_freq(tone, sample_rate);
    send_samples_for_noise_floor(squelch);

    GenerateSignal signal_with_tone(sample_rate);
    signal_with_tone.add_tone(tone, Tone::NORMAL);

    // send through "signal" samples until its time to process audio
    for (int i = 0; i < 500 && !squelch.should_process_audio(); ++i) {
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_FALSE(squelch.is_open());
    ASSERT_TRUE(squelch.should_process_audio());

    // process audio samples and "signal" samples until squelch is open
    for (int i = 0; i < 500 && !squelch.is_open(); ++i) {
        squelch.process_audio_sample(signal_with_tone.get_sample());
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.is_open());
    ASSERT_TRUE(squelch.should_process_audio());

    // run through a lot more to ensure squelch stays open
    for (int i = 0; i < 100000; ++i) {
        squelch.process_audio_sample(signal_with_tone.get_sample());
        squelch.process_raw_sample(raw_signal_sample);
        ASSERT_TRUE(squelch.is_open());
        ASSERT_TRUE(squelch.should_process_audio());
    }

    EXPECT_GT(squelch.ctcss_count(), 0);
    EXPECT_EQ(squelch.no_ctcss_count(), 0);
}

TEST_F(SquelchTest, wrong_ctcss) {
    float actual_tone = CTCSS::standard_tones[0];
    float expected_tone = CTCSS::standard_tones[7];
    float sample_rate = 8000;

    Squelch squelch;
    squelch.set_ctcss_freq(expected_tone, sample_rate);
    send_samples_for_noise_floor(squelch);

    GenerateSignal signal_with_tone(sample_rate);
    signal_with_tone.add_tone(actual_tone, Tone::NORMAL);

    // send through "signal" samples until its time to process audio
    for (int i = 0; i < 500 && !squelch.should_process_audio(); ++i) {
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.should_process_audio());
    ASSERT_FALSE(squelch.is_open());

    // process lots of audio samples and "signal" samples and squelch never opens
    for (int i = 0; i < 100000; ++i) {
        squelch.process_audio_sample(signal_with_tone.get_sample());
        squelch.process_raw_sample(raw_signal_sample);
        ASSERT_TRUE(squelch.should_process_audio());
        ASSERT_FALSE(squelch.is_open());
    }

    EXPECT_EQ(squelch.ctcss_count(), 0);
    EXPECT_GT(squelch.no_ctcss_count(), 0);
}

TEST_F(SquelchTest, close_ctcss) {
    float actual_tone = CTCSS::standard_tones[5];
    float expected_tone = CTCSS::standard_tones[7];
    float sample_rate = 8000;

    Squelch squelch;
    squelch.set_ctcss_freq(expected_tone, sample_rate);
    send_samples_for_noise_floor(squelch);

    GenerateSignal signal_with_tone(sample_rate);
    signal_with_tone.add_tone(actual_tone, Tone::NORMAL);

    // send through "signal" samples until its time to process audio
    for (int i = 0; i < 500 && !squelch.should_process_audio(); ++i) {
        squelch.process_raw_sample(raw_signal_sample);
    }
    ASSERT_TRUE(squelch.should_process_audio());
    ASSERT_FALSE(squelch.is_open());

    // process of audio samples and "signal" samples until squelch opens
    for (int i = 0; i < 500 && !squelch.is_open(); ++i) {
        squelch.process_audio_sample(signal_with_tone.get_sample());
        squelch.process_raw_sample(raw_signal_sample);
        ASSERT_TRUE(squelch.should_process_audio());
    }
    ASSERT_TRUE(squelch.is_open());

    // keep processing samples until squelch closes again
    for (int i = 0; i < 3000 && squelch.is_open(); ++i) {
        squelch.process_audio_sample(signal_with_tone.get_sample());
        squelch.process_raw_sample(raw_signal_sample);
        ASSERT_TRUE(squelch.should_process_audio());
    }
    ASSERT_FALSE(squelch.is_open());

    // process lots of audio samples and "signal" samples and squelch stays closed
    for (int i = 0; i < 100000; ++i) {
        squelch.process_audio_sample(signal_with_tone.get_sample());
        squelch.process_raw_sample(raw_signal_sample);
        ASSERT_TRUE(squelch.should_process_audio());
        ASSERT_FALSE(squelch.is_open());
    }

    EXPECT_EQ(squelch.ctcss_count(), 0);
    EXPECT_GT(squelch.no_ctcss_count(), 0);
}

class SquelchTransmissionTest : public SquelchTest {
   protected:
    // Feeds sample until the squelch reports a finished transmission or max_samples is reached.
    // Returns the number of finished transmissions seen.
    int feed(Squelch& squelch, float sample, int max_samples) {
        int ended = 0;
        for (int i = 0; i < max_samples; ++i) {
            squelch.process_raw_sample(sample);
            if (squelch.transmission_ended()) {
                ended++;
            }
        }
        return ended;
    }
};

TEST_F(SquelchTransmissionTest, nothing_recorded_by_default) {
    Squelch squelch;
    EXPECT_FALSE(squelch.transmission_ended());
    EXPECT_EQ(feed(squelch, raw_no_signal_sample, 5000), 0);
}

TEST_F(SquelchTransmissionTest, transmission_recorded_once_on_close) {
    Squelch squelch;
    send_samples_for_noise_floor(squelch);
    const float noise_floor = squelch.noise_level();

    const int signal_samples = 4000;
    EXPECT_EQ(feed(squelch, raw_signal_sample, signal_samples), 0);
    ASSERT_TRUE(squelch.is_open());

    EXPECT_EQ(feed(squelch, raw_no_signal_sample, 2000), 1);
    ASSERT_FALSE(squelch.is_open());
    EXPECT_EQ(squelch.open_count(), 1u);

    const Squelch::Transmission& t = squelch.last_transmission();
    EXPECT_NEAR(t.peak_level, raw_signal_sample, 0.01 * raw_signal_sample);
    EXPECT_NEAR(t.noise_floor, noise_floor, 0.01 * noise_floor);
    // duration runs from OPEN (after the open delay) to CLOSED (after the close delay)
    EXPECT_GT(t.sample_count, (size_t)(signal_samples - 300));
    EXPECT_LT(t.sample_count, (size_t)(signal_samples + 500));
}

TEST_F(SquelchTransmissionTest, ended_flag_lasts_one_sample) {
    Squelch squelch;
    send_samples_for_noise_floor(squelch);
    feed(squelch, raw_signal_sample, 2000);

    int i = 0;
    for (; i < 2000 && !squelch.transmission_ended(); ++i) {
        squelch.process_raw_sample(raw_no_signal_sample);
    }
    ASSERT_TRUE(squelch.transmission_ended());

    squelch.process_raw_sample(raw_no_signal_sample);
    EXPECT_FALSE(squelch.transmission_ended());
}

TEST_F(SquelchTransmissionTest, blip_that_never_opens_is_not_recorded) {
    Squelch squelch;
    send_samples_for_noise_floor(squelch);

    int ended = feed(squelch, raw_signal_sample, 50);
    ended += feed(squelch, raw_no_signal_sample, 3000);

    EXPECT_EQ(squelch.open_count(), 0u);
    EXPECT_EQ(ended, 0);
}

TEST_F(SquelchTransmissionTest, one_record_per_transmission) {
    Squelch squelch;
    send_samples_for_noise_floor(squelch);

    int ended = 0;
    for (int n = 0; n < 3; ++n) {
        ended += feed(squelch, raw_signal_sample, 2000);
        ended += feed(squelch, raw_no_signal_sample, 2000);
    }

    EXPECT_EQ(squelch.open_count(), 3u);
    EXPECT_EQ(ended, 3);
}
