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
 * @file SubscribingParticipant.hpp
 *
 */

#ifndef _TEST_BLACKBOX_SUBSCRIBINGPARTICIPANT_HPP_
#define _TEST_BLACKBOX_SUBSCRIBINGPARTICIPANT_HPP_

#include <fastrtps/fastrtps_fwd.h>
#include <fastrtps/Domain.h>
#include <fastrtps/participant/Participant.h>
#include <fastrtps/participant/ParticipantListener.h>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/subscriber/Subscriber.h>
#include <fastrtps/subscriber/SubscriberListener.h>
#include <fastrtps/attributes/SubscriberAttributes.h>
#include <fastrtps/subscriber/SampleInfo.h>

#include <asio.hpp>
#include <condition_variable>
#include <gtest/gtest.h>
#include <vector>

namespace eprosima {
namespace fastrtps {

template<class TypeSupport>
class SubscribingParticipant
{
public:

    typedef TypeSupport type_support;
    typedef typename type_support::type type;

private:

    class Listener : public SubscriberListener
    {
        friend class SubscribingParticipant;

    public:

        Listener(SubscribingParticipant* participant)
            : participant_(participant)
            , times_liveliness_lost_()
            , times_liveliness_recovered_()
        {}

        ~Listener()
        {}

        void onNewDataMessage(Subscriber *sub) override
        {
            (void)sub;
        }

        void onSubscriptionMatched(
                Subscriber* sub,
                rtps::MatchingInfo& info) override
        {
            (void)sub;
            (info.status == rtps::MATCHED_MATCHING) ? participant_->matched() : participant_->unmatched();
        }

        virtual void on_liveliness_changed(
                Subscriber* sub,
                const LivelinessChangedStatus& status)
        {
            (void)sub;
            (status.alive_count_change == 1) ? times_liveliness_recovered_++ : times_liveliness_lost_++;

            if (status.alive_count_change == 1)
            {
                std::cout << "++++ Publisher " << status.last_publication_handle << " recovered liveliness: " << sub->getGuid() << std::endl;
            }
            else
            {
                std::cout << "++++ Publisher " << status.last_publication_handle << " lost liveliness: " << sub->getGuid() << std::endl;
            }
        }

    private:

        Listener& operator=(const Listener&) = delete;
        //! A pointer to the participant
        SubscribingParticipant* participant_;
        //! The number of times liveliness was lost
        unsigned int times_liveliness_lost_;
        //! The number of times liveliness was recovered
        unsigned int times_liveliness_recovered_;

    } listener_;

public:

    SubscribingParticipant(
            unsigned int num_subscribers,
            unsigned int num_expected_publishers)
        : participant_(nullptr)
        , participant_attr_()
        , listener_(this)
        , num_subscribers_(num_subscribers)
        , num_expected_publishers_(num_expected_publishers)
        , subscribers_(num_subscribers)
        , matched_(0)
        , participant_matched_(0)
        {
#if defined(PREALLOCATED_WITH_REALLOC_MEMORY_MODE_TEST)
            subscriber_attr_.historyMemoryPolicy = rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
#elif defined(DYNAMIC_RESERVE_MEMORY_MODE_TEST)
            subscriber_attr_.historyMemoryPolicy = rtps::DYNAMIC_RESERVE_MEMORY_MODE;
#else
            subscriber_attr_.historyMemoryPolicy = rtps::PREALLOCATED_MEMORY_MODE;
#endif
            // By default, heartbeat period delay is 100 milliseconds.
            subscriber_attr_.times.heartbeatResponseDelay = 0.1;
        }

    ~SubscribingParticipant()
    {
        if(participant_ != nullptr)
        {
            Domain::removeParticipant(participant_);
            participant_ = nullptr;
        }
    }

    bool init_participant()
    {
        participant_attr_.rtps.builtin.domainId = (uint32_t)GET_PID() % 230;

        participant_ = Domain::createParticipant(participant_attr_);
        if (participant_!= nullptr)
        {
            Domain::registerType(participant_, &type_);
            return true;
        }
        return false;
    }

    bool init_subscriber(unsigned int index)
    {
        if (index >= num_subscribers_)
        {
            return false;
        }
        auto subscriber = Domain::createSubscriber(participant_, subscriber_attr_, &listener_);
        if (subscriber != nullptr)
        {
            subscribers_[index] = subscriber;
            return true;
        }
        return false;
    }

    void wait_discovery(std::chrono::seconds timeout = std::chrono::seconds::zero())
    {
        std::unique_lock<std::mutex> lock(mutexDiscovery_);

        std::cout << "Subscriber is waiting discovery..." << std::endl;

        if(timeout == std::chrono::seconds::zero())
        {
            cv_.wait(lock, [&](){return matched_ == num_subscribers_;});
        }
        else
        {
            cv_.wait_for(lock, timeout, [&](){return matched_ == num_subscribers_;});
        }

        std::cout << "Subscriber discovery finished: " << matched_ << std::endl;
    }

    SubscribingParticipant& reliability(const ReliabilityQosPolicyKind kind)
    {
        subscriber_attr_.qos.m_reliability.kind = kind;
        return *this;
    }

    SubscribingParticipant& liveliness_kind(const LivelinessQosPolicyKind& kind)
    {
        subscriber_attr_.qos.m_liveliness.kind = kind;
        return *this;
    }

    SubscribingParticipant& liveliness_lease_duration(const Duration_t lease_duration)
    {
        subscriber_attr_.qos.m_liveliness.lease_duration = lease_duration;
        return *this;
    }

    SubscribingParticipant& topic_name(std::string topicName)
    {
        subscriber_attr_.topic.topicDataType = type_.getName();
        subscriber_attr_.topic.topicName = topicName;
        return *this;
    }

    unsigned int times_liveliness_lost()
    {
        return listener_.times_liveliness_lost_;
    }

    unsigned int times_liveliness_recovered()
    {
        return listener_.times_liveliness_recovered_;
    }

private:

    void matched()
    {
        std::unique_lock<std::mutex> lock(mutexDiscovery_);
        ++matched_;
        if (matched_ == num_expected_publishers_)
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

    SubscribingParticipant& operator=(const SubscribingParticipant&)= delete;

    Participant* participant_;
    ParticipantAttributes participant_attr_;

    //! Number of subscribers in this participant
    unsigned int num_subscribers_;
    //! Number of expected subscribers to match
    unsigned int num_expected_publishers_;

    std::vector<Subscriber*> subscribers_;
    SubscriberAttributes subscriber_attr_;

    std::mutex mutexDiscovery_;
    std::condition_variable cv_;
    std::atomic<unsigned int> matched_;
    unsigned int participant_matched_;

    type_support type_;
};

}
}
#endif // _TEST_BLACKBOX_SUBSCRIBINGPARTICIPANT_HPP_
