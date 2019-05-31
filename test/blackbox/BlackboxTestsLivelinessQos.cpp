// Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "BlackboxTests.hpp"

#include "PubSubReader.hpp"
#include "PubSubWriter.hpp"
#include "PublisingParticipant.hpp"
#include "SubscribingParticipant.hpp"
#include "ReqRepAsReliableHelloWorldRequester.hpp"
#include "ReqRepAsReliableHelloWorldReplier.hpp"

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

//! Tests that when kind is automatic liveliness is never lost, even if the writer never sends data
BLACKBOXTEST(LivelinessQos, Liveliness_Automatic_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Liveliness lease duration and announcement period
    uint32_t liveliness_ms = 20;
    Duration_t liveliness_s(liveliness_ms * 1e-3);
    Duration_t announcement_period(liveliness_ms * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    std::this_thread::sleep_for(std::chrono::milliseconds(liveliness_ms * 10));

    // When using automatic kind, liveliness on both publisher and subscriber should never be lost
    // It would only be lost if the publishing application crashed, which can't be reproduced in the tests
    EXPECT_EQ(writer.times_liveliness_lost(), 0u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 0u);
}

//! Same as above using best-effort reliability
BLACKBOXTEST(LivelinessQos, Liveliness_Automatic_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Liveliness lease duration and announcement period
    uint32_t liveliness_ms = 20;
    Duration_t liveliness_s(liveliness_ms * 1e-3);
    Duration_t announcement_period(liveliness_ms * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    std::this_thread::sleep_for(std::chrono::milliseconds(liveliness_ms * 10));

    // When using automatic kind, liveliness on both publisher and subscriber should never be lost
    // It would only be lost if the publishing application crashed, which can't be reproduced in the tests
    EXPECT_EQ(writer.times_liveliness_lost(), 0u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 0u);
}

//! Tests liveliness with the following paramters
//! Writer is reliable, and MANUAL_BY_PARTICIPANT
//! Reader is reliable, and MANUAL_BY_PARTICIPANT
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ShortLiveliness_ManualByParticipant_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples * 2);
}

//! Tests liveliness with the following paramters
//! Writer is best-effort, and MANUAL_BY_PARTICIPANT
//! Reader is best-effort, and MANUAL_BY_PARTICIPANT
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ShortLiveliness_ManualByParticipant_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);

    for (count = 0; count<num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples * 2);
}

//! Tests liveliness with the following paramters
//! Writer is best-effort, and MANUAL_BY_PARTICIPANT
//! Reader is best-effort, and MANUAL_BY_PARTICIPANT
//! Liveliness lease duration is long in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, LongLiveliness_ManualByParticipant_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 2.0 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 2.0 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 2u);
}

//! Tests liveliness with the following paramters
//! Writer is best-effort, and MANUAL_BY_PARTICIPANT
//! Reader is best-effort, and MANUAL_BY_PARTICIPANT
//! Liveliness lease duration is long in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, LongLiveliness_ManualByParticipant_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t writer_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 2.0 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 2.0 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(writer_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);

    for (count = 0; count < writer_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 2u);
}

//! Tests liveliness with the following paramters
//! Writer is reliable, and MANUAL_BY_TOPIC
//! Reader is reliable, and MANUAL_BY_TOPIC
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ShortLiveliness_ManualByTopic_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        // Send data
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples * 2);
}

//! Tests liveliness with the following paramters
//! Writer is best-effort, and MANUAL_BY_TOPIC
//! Reader is best-effort, and MANUAL_BY_TOPIC
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ShortLiveliness_ManualByTopic_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        // Send data
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    // Note that in MANUAL_BY_TOPIC liveliness, the assert_liveliness() method relies on sending a heartbeat
    // However best-effort writers don't send heartbeats, so the reader in this case will never get notified
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);
}

//! Tests liveliness with the following paramters
//! Writer is reliable, and MANUAL_BY_TOPIC
//! Reader is reliable, and MANUAL_BY_TOPIC
//! Liveliness lease duration is long in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, LongLiveliness_ManualByTopic_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 2 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 2 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        // Send data
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);

    for (count=0; count<num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 2u);
}

//! Tests liveliness with the following paramters
//! Writer is best-effort, and MANUAL_BY_TOPIC
//! Reader is best-effort, and MANUAL_BY_TOPIC
//! Liveliness lease duration is long in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, LongLiveliness_ManualByTopic_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 2 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 2 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        // Send data
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);

    for(count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Wait a bit longer
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 2u);
    // Note that MANUAL_BY_TOPIC liveliness relies on sending heartbeats when using the assert method
    // However best-effor writers do not send heartbeats, so the reader will never get notified
    EXPECT_EQ(reader.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);
}

//! Tests liveliness with the following parameters
//! Writer is reliable, liveliness is manual by participant
//! Reader is reliable, liveliness is automatic
//! Liveliness lease duration is long in comparison to the writer write/assert rate
BLACKBOXTEST(LivelinessQos, LongLiveliness_ManualByParticipant_Automatic_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 2 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 2 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        // Send data
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 2u);
}

//! Tests liveliness with the following parameters
//! Writer is reliable, liveliness is manual by participant
//! Reader is reliable, liveliness is automatic
//! Liveliness is short in comparison to the writer write/assert rate
BLACKBOXTEST(LivelinessQos, ShortLiveliness_ManualByParticipant_Automatic_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples * 2);
}

//! Tests liveliness with the following parameters
//! Writer is best-effort, liveliness is manual by participant
//! Reader is best-effort, liveliness is automatic
//! Liveliness is long in comparison to the writer write/assert rate
BLACKBOXTEST(LivelinessQos, LongLiveliness_ManualByParticipant_Automatic_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 2 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 2 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_lost(), 1u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 1u);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms * 2));
    EXPECT_EQ(writer.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_lost(), 2u);
    EXPECT_EQ(reader.times_liveliness_recovered(), 2u);
}

//! Tests liveliness with the following parameters
//! Writer is best-effort, liveliness is manual by participant
//! Reader is best-effort, liveliness is automatic
//! Liveliness is short in comparison to the writer write/assert rate
BLACKBOXTEST(LivelinessQos, ShortLiveliness_ManualByParticipant_Automatic_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);

    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples * 2);
}

//! Tests liveliness with the following parameters
//! Writer is reliable, and uses manual by topic liveliness kind
//! Reader is reliable, and uses automatic liveliness kind
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ManualByTopic_Automatic_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    // Write some samples
    size_t count = 0;
    for (auto data_sample : data)
    {
        // Send data
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Now use assert_liveliness() method
    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples * 2);
}

//! Tests liveliness with the following parameters
//! Writer is best-effort, and uses manual by topic liveliness kind
//! Reader is best-effort, and uses automatic liveliness kind
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ManualByTopic_Automatic_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    // Write some samples
    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Now use assert_liveliness() method
    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    // As best-effort readers do not process heartbeats the expected number of times liveliness was lost
    // and recovered corresponds to the bit in the test when we sent samples (not when we asserted liveliness)
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);
}

//! Tests liveliness with the following parameters
//! Writer is reliable, and uses manual by topic liveliness kind
//! Reader is reliable, and uses manual by participant liveliness kind
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ManualByTopic_ManualByParticipant_Reliable)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    // Write some samples
    size_t count = 0;
    for (auto data_sample : data)
    {
        // Send data
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Now use assert_liveliness() method
    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples * 2);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples * 2);
}

//! Tests liveliness with the following parameters
//! Writer is best-effort, and uses manual by topic liveliness kind
//! Reader is best-effort, and uses manual by participant liveliness kind
//! Liveliness lease duration is short in comparison to writer write/assert rate
BLACKBOXTEST(LivelinessQos, ManualByTopic_ManualByParticipant_BestEffort)
{
    PubSubReader<HelloWorldType> reader(TEST_TOPIC_NAME);
    PubSubWriter<HelloWorldType> writer(TEST_TOPIC_NAME);

    // Write rate in milliseconds and number of samples to write
    uint32_t writer_sleep_ms = 100;
    uint32_t num_samples = 3;

    // Liveliness lease duration and announcement period, in seconds
    Duration_t liveliness_s(writer_sleep_ms * 0.1 * 1e-3);
    Duration_t announcement_period(writer_sleep_ms * 0.1 * 1e-3 * 0.9);

    reader.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
            .liveliness_lease_duration(liveliness_s)
            .init();
    writer.reliability(BEST_EFFORT_RELIABILITY_QOS)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS)
            .liveliness_announcement_period(announcement_period)
            .liveliness_lease_duration(liveliness_s)
            .init();

    ASSERT_TRUE(reader.isInitialized());
    ASSERT_TRUE(writer.isInitialized());

    // Wait for discovery.
    writer.wait_discovery();
    reader.wait_discovery();

    auto data = default_helloworld_data_generator(num_samples);
    reader.startReception(data);

    // Write some samples
    size_t count = 0;
    for (auto data_sample : data)
    {
        writer.send_sample(data_sample);
        ++count;
        reader.block_for_at_least(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }
    // Now use assert_liveliness() method
    for (count = 0; count < num_samples; count++)
    {
        writer.assert_liveliness();
        std::this_thread::sleep_for(std::chrono::milliseconds(writer_sleep_ms));
    }

    EXPECT_EQ(writer.times_liveliness_lost(), num_samples * 2);
    // Note that, as best-effor readers do not proccess heartbeats and assert_liveliness() relies on sending a
    // heartbeat to assess liveliness, the expected number of times liveliness was lost and recovered
    // corresponds only to the bit in the test when the writer wrote samples
    EXPECT_EQ(reader.times_liveliness_lost(), num_samples);
    EXPECT_EQ(reader.times_liveliness_recovered(), num_samples);
}

//! Tests liveliness in the following scenario
//! A participant with two publishers and two topics
//! A participant with two subscribers and two topics
//! Manual by participant liveliness
//! Only one publisher asserts liveliness manually
BLACKBOXTEST(LivelinessQos, TwoWriters_TwoReaders_ManualByParticipant)
{
    unsigned int num_pub = 2;
    unsigned int num_sub = 2;
    unsigned int lease_duration_s = 1;
    unsigned int announcement_period_s = 0.5;

    // Publishers
    PublishingParticipant<HelloWorldType> publishers(num_pub, num_sub);
    ASSERT_TRUE(publishers.init_participant());
    publishers.topic_name(TEST_TOPIC_NAME + "1")
            .liveliness_announcement_period(announcement_period_s)
            .liveliness_lease_duration(lease_duration_s)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
    ASSERT_TRUE(publishers.init_publisher(0));
    publishers.topic_name(TEST_TOPIC_NAME + "2")
            .liveliness_announcement_period(announcement_period_s)
            .liveliness_lease_duration(lease_duration_s)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
    ASSERT_TRUE(publishers.init_publisher(1));

    // Subscribers
    SubscribingParticipant<HelloWorldType> subscribers(num_sub, num_pub);
    ASSERT_TRUE(subscribers.init_participant());
    subscribers.topic_name(TEST_TOPIC_NAME + "1")
            .liveliness_lease_duration(lease_duration_s)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
    ASSERT_TRUE(subscribers.init_subscriber(0));
    subscribers.topic_name(TEST_TOPIC_NAME + "2")
            .liveliness_lease_duration(lease_duration_s)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
    ASSERT_TRUE(subscribers.init_subscriber(1));

    publishers.wait_discovery();
    subscribers.wait_discovery();

    unsigned int num_assertions = 4;
    unsigned int assert_rate_ms = 50;
    for (unsigned int count = 0; count < num_assertions; count++)
    {
        publishers.assert_liveliness(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(assert_rate_ms));
    }
    // Only one publisher asserts liveliness but the other should be asserted by the QoS, as
    // liveliness kind is manual by participant
    EXPECT_EQ(publishers.times_liveliness_lost(), 0u);
    EXPECT_EQ(subscribers.times_liveliness_recovered(), num_pub);
    EXPECT_EQ(subscribers.times_liveliness_lost(), 0u);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(publishers.times_liveliness_lost(), num_pub);
    EXPECT_EQ(subscribers.times_liveliness_recovered(), num_pub);
    EXPECT_EQ(subscribers.times_liveliness_lost(), num_pub);
}

//! Tests liveliness in the same scenario as above but using manual by topic liveliness
//! A participant with two publishers and two topics
//! A participant with two subscribers and two topics
//! Manual by topic liveliness
//! Only one publisher asserts liveliness manually
BLACKBOXTEST(LivelinessQos, TwoWriters_TwoReaders_ManualByTopic)
{
    unsigned int num_pub = 2;
    unsigned int num_sub = 2;
    unsigned int lease_duration_ms = 500;
    unsigned int announcement_period_ms = 250;

    // Publishers
    PublishingParticipant<HelloWorldType> publishers(num_pub, num_sub);
    ASSERT_TRUE(publishers.init_participant());
    publishers.topic_name(TEST_TOPIC_NAME + "1")
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_announcement_period(announcement_period_ms * 1e-3)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS);
    ASSERT_TRUE(publishers.init_publisher(0));
    publishers.topic_name(TEST_TOPIC_NAME + "2")
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_announcement_period(announcement_period_ms * 1e-3)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS);
    ASSERT_TRUE(publishers.init_publisher(1));

    // Subscribers
    SubscribingParticipant<HelloWorldType> subscribers(num_sub, num_pub);
    ASSERT_TRUE(subscribers.init_participant());
    subscribers.topic_name(TEST_TOPIC_NAME + "1")
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS);
    ASSERT_TRUE(subscribers.init_subscriber(0));
    subscribers.topic_name(TEST_TOPIC_NAME + "2")
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS);
    ASSERT_TRUE(subscribers.init_subscriber(1));

    publishers.wait_discovery();
    subscribers.wait_discovery();

    unsigned int num_assertions = 4;
    unsigned int assert_rate_ms = 10;
    for (unsigned int count = 0; count < num_assertions; count++)
    {
        publishers.assert_liveliness(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(assert_rate_ms));
    }
    EXPECT_EQ(publishers.times_liveliness_lost(), 0u);
    EXPECT_EQ(subscribers.times_liveliness_recovered(), 1u);
    EXPECT_EQ(subscribers.times_liveliness_lost(), 0u);

    std::this_thread::sleep_for(std::chrono::milliseconds(lease_duration_ms * 2));
    EXPECT_EQ(publishers.times_liveliness_lost(), 1u);
    EXPECT_EQ(subscribers.times_liveliness_recovered(), 1u);
    EXPECT_EQ(subscribers.times_liveliness_lost(), 1u);
}

BLACKBOXTEST(LivelinessQos, ThreeWriters_ThreeReaders)
{
    unsigned int num_pub = 3;
    unsigned int num_sub = 3;
    unsigned int lease_duration_ms = 500;
    unsigned int announcement_period_ms = 250;

    // Publishers
    PublishingParticipant<HelloWorldType> publishers(num_pub, num_sub);
    ASSERT_TRUE(publishers.init_participant());
    publishers.topic_name(TEST_TOPIC_NAME)
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_announcement_period(announcement_period_ms * 1e-3)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS);
    ASSERT_TRUE(publishers.init_publisher(0));
    publishers.topic_name(TEST_TOPIC_NAME)
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_announcement_period(announcement_period_ms * 1e-3)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
    ASSERT_TRUE(publishers.init_publisher(1));
    publishers.topic_name(TEST_TOPIC_NAME)
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_announcement_period(announcement_period_ms * 1e-3)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS);
    ASSERT_TRUE(publishers.init_publisher(2));

    // Subscribers
    SubscribingParticipant<HelloWorldType> subscribers(num_sub, num_pub);
    ASSERT_TRUE(subscribers.init_participant());
    subscribers.topic_name(TEST_TOPIC_NAME)
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(AUTOMATIC_LIVELINESS_QOS);
    ASSERT_TRUE(subscribers.init_subscriber(0));
    subscribers.topic_name(TEST_TOPIC_NAME)
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS);
    ASSERT_TRUE(subscribers.init_subscriber(1));
    subscribers.topic_name(TEST_TOPIC_NAME)
            .reliability(RELIABLE_RELIABILITY_QOS)
            .liveliness_lease_duration(lease_duration_ms * 1e-3)
            .liveliness_kind(MANUAL_BY_TOPIC_LIVELINESS_QOS);
    ASSERT_TRUE(subscribers.init_subscriber(2));

    publishers.wait_discovery();
    subscribers.wait_discovery();

    // Manual by topic publisher asserts liveliness
    unsigned int num_assertions = 4;
    unsigned int assert_rate_ms = 10;
    for (unsigned int count = 0; count < num_assertions; count++)
    {
        publishers.assert_liveliness(2);
        std::this_thread::sleep_for(std::chrono::milliseconds(assert_rate_ms));
    }
    // All three subscribers are notified that liveliness was recovered
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    EXPECT_EQ(subscribers.times_liveliness_recovered(), 3u);
//    EXPECT_EQ(publishers.times_liveliness_lost(), 0u);
//    EXPECT_EQ(subscribers.times_liveliness_recovered(), 1u);
//    EXPECT_EQ(subscribers.times_liveliness_lost(), 0u);

//    std::this_thread::sleep_for(std::chrono::milliseconds(lease_duration_ms * 2));
//    EXPECT_EQ(publishers.times_liveliness_lost(), 1u);
//    EXPECT_EQ(subscribers.times_liveliness_recovered(), 1u);
//    EXPECT_EQ(subscribers.times_liveliness_lost(), 1u);
}

// TODO raquel Add tests with participants having more than one publisher/subscriber
// TODO raquel Test the case of two readers matched to a writer and one reader being removed
