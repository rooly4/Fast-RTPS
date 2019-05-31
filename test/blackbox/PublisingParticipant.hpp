// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

/**
 * @file PubSubWriter.hpp
 *
 */

#ifndef _TEST_BLACKBOX_PUBLISHINGPARTICIPANT_HPP_
#define _TEST_BLACKBOX_PUBLISHINGPARTICIPANT_HPP_

#include <fastrtps/fastrtps_fwd.h>
#include <fastrtps/Domain.h>
#include <fastrtps/participant/Participant.h>
#include <fastrtps/participant/ParticipantListener.h>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/publisher/Publisher.h>
#include <fastrtps/publisher/PublisherListener.h>
#include <fastrtps/attributes/PublisherAttributes.h>

#include <asio.hpp>
#include <condition_variable>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace eprosima {
namespace fastrtps {

template<class TypeSupport>
class PublishingParticipant
{
    class Listener : public PublisherListener
    {
        friend class PublishingParticipant;

        public:

            Listener(PublishingParticipant* participant)
                : participant_(participant)
                , times_liveliness_lost()
            {}

            ~Listener()
            {}

            void onPublicationMatched(
                    Publisher* pub,
                    rtps::MatchingInfo &info) override
            {
                (void)pub;
                (info.status == rtps::MATCHED_MATCHING)? participant_->matched(): participant_->unmatched();
            }

            void on_liveliness_lost(
                    Publisher* pub,
                    const LivelinessLostStatus& status) override
            {
                (void)pub;
                (void)status;
                times_liveliness_lost++;
            }

        private:

            Listener& operator=(const Listener&) = delete;
            //! A pointer to the participant
            PublishingParticipant* participant_;
            //! Number of times liveliness was lost
            unsigned int times_liveliness_lost;

    };

public:

    typedef TypeSupport type_support;
    typedef typename type_support::type type;

    PublishingParticipant(
            unsigned int num_publishers,
            unsigned int num_expected_subscribers)
        : participant_(nullptr)
        , participant_attr_()
        , num_publishers_(num_publishers)
        , num_expected_subscribers_(num_expected_subscribers)
        , publishers_(num_publishers)
        , publisher_attr_()
        , listener_(this)
        , matched_(0)
    {

#if defined(PREALLOCATED_WITH_REALLOC_MEMORY_MODE_TEST)
        publisher_attr_.historyMemoryPolicy = rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
#elif defined(DYNAMIC_RESERVE_MEMORY_MODE_TEST)
        publisher_attr_.historyMemoryPolicy = rtps::DYNAMIC_RESERVE_MEMORY_MODE;
#else
        publisher_attr_.historyMemoryPolicy = rtps::PREALLOCATED_MEMORY_MODE;
#endif

        // By default, heartbeat period and nack response delay are 100 milliseconds.
        publisher_attr_.times.heartbeatPeriod.seconds = 0;
        publisher_attr_.times.heartbeatPeriod.nanosec = 100000000;
        publisher_attr_.times.nackResponseDelay.seconds = 0;
        publisher_attr_.times.nackResponseDelay.nanosec = 100000000;
    }

    ~PublishingParticipant()
    {
        if(participant_ != nullptr)
        {
            Domain::removeParticipant(participant_);
            participant_ = nullptr;
        }
    }

    /**
     * @brief Initializes the participant
     * @return True if participant was initialized correctly
     */
    bool init_participant()
    {
        participant_attr_.rtps.builtin.domainId = (uint32_t)GET_PID() % 230;
        participant_ = Domain::createParticipant(participant_attr_);
        if (participant_ != nullptr)
        {
            Domain::registerType(participant_, &type_);
            return true;
        }
        return false;
    }

    /**
     * @brief Initialises a given publisher
     * @param index The index of the publisher to initialize
     * @return True if publisher was initialized correctly
     */
    bool init_publisher(unsigned int index)
    {
        if (participant_ == nullptr)
        {
            return false;
        }
        if (index >= num_publishers_)
        {
            return false;
        }

        auto pub = Domain::createPublisher(participant_, publisher_attr_, &listener_);
        if (pub != nullptr)
        {
            publishers_[index] = pub;
            return true;
        }
        return false;
    }

    /**
     * @brief Sends a sample
     * @param msg The sample to send
     * @param index The index of the publisher sending the sample
     * @return True on success
     */
    bool send_sample(type& msg, unsigned int index = 0)
    {
        return publishers_[index]->write((void*)&msg);
    }

    /**
     * @brief Calls assert_liveliness() on specified publisher
     * @param index The index of the publisher
     */
    void assert_liveliness(unsigned int index = 0)
    {
        publishers_[index]->assert_liveliness();
    }

    void wait_discovery(std::chrono::seconds timeout = std::chrono::seconds::zero())
    {
        std::unique_lock<std::mutex> lock(mutexDiscovery_);

        std::cout << "Publisher is waiting discovery..." << std::endl;

        if(timeout == std::chrono::seconds::zero())
        {
            cv_.wait(lock, [&](){return matched_ == num_publishers_;});
        }
        else
        {
            cv_.wait_for(lock, timeout, [&](){return matched_ == num_publishers_;});
        }

        std::cout << "Publisher discovery finished: " << matched_ << std::endl;
    }

    PublishingParticipant& topic_name(std::string topicName)
    {
        publisher_attr_.topic.topicDataType = type_.getName();
        publisher_attr_.topic.topicName=topicName;
        return *this;
    }

    PublishingParticipant& reliability(const ReliabilityQosPolicyKind kind)
    {
        publisher_attr_.qos.m_reliability.kind = kind;
        return *this;
    }

    PublishingParticipant& liveliness_kind(const LivelinessQosPolicyKind kind)
    {
        publisher_attr_.qos.m_liveliness.kind = kind;
        return *this;
    }

    PublishingParticipant& liveliness_lease_duration(const Duration_t lease_duration)
    {
        publisher_attr_.qos.m_liveliness.lease_duration = lease_duration;
        return *this;
    }

    PublishingParticipant& liveliness_announcement_period(const Duration_t announcement_period)
    {
        publisher_attr_.qos.m_liveliness.announcement_period = announcement_period;
        return *this;
    }

    unsigned int times_liveliness_lost()
    {
        return listener_.times_liveliness_lost;
    }

private:

    PublishingParticipant& operator=(const PublishingParticipant&)= delete;

    void matched()
    {
        std::unique_lock<std::mutex> lock(mutexDiscovery_);
        ++matched_;
        if (matched_ == num_expected_subscribers_)
        {
            cv_.notify_one();
        }
    }

    void unmatched()
    {
        std::unique_lock<std::mutex> lock(mutexDiscovery_);
        --matched_;
        cv_.notify_one();
    }

    Participant* participant_;
    ParticipantAttributes participant_attr_;

    //! Number of publishers in this participant
    unsigned int num_publishers_;
    //! Number of expected subscribers to match
    unsigned int num_expected_subscribers_;

    std::vector<Publisher*> publishers_;
    PublisherAttributes publisher_attr_;

    Listener listener_;

    std::mutex mutexDiscovery_;
    std::condition_variable cv_;
    std::atomic<unsigned int> matched_;

    type_support type_;
};

}
}

#endif // _TEST_BLACKBOX_PUBLISHINGPARTICIPANT_HPP_
