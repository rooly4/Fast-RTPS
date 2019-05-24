#include <iostream>
#include <string>

#include <fastrtps/participant/Participant.h>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/subscriber/Subscriber.h>
#include <fastrtps/attributes/SubscriberAttributes.h>
#include <fastrtps/subscriber/SubscriberListener.h>
#include <fastrtps/publisher/Publisher.h>
#include <fastrtps/publisher/PublisherListener.h>
#include <fastrtps/attributes/PublisherAttributes.h>
#include <fastrtps/Domain.h>
#include <fastrtps/subscriber/SampleInfo.h>

#include <fastrtps/utils/eClock.h>

#include "samplePubSubTypes.h"

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

void latejoiners();

int main(){
    latejoiners();
    return 0;
}

Participant* g_part = nullptr;
samplePubSubType sampleType;
SampleInfo_t sample_info;
Subscriber* mySub2 = nullptr;

std::thread* create_sub2 = nullptr;

void create_thread()
{
    //Volatile Sub
    SubscriberAttributes Rparam2;
    Rparam2.topic.topicDataType = sampleType.getName();
    Rparam2.topic.topicName = "samplePubSubTopic";

    Rparam2.topic.historyQos.kind = KEEP_ALL_HISTORY_QOS;
    Rparam2.qos.m_reliability.kind = RELIABLE_RELIABILITY_QOS;
    Rparam2.qos.m_durability.kind = VOLATILE_DURABILITY_QOS;

    std::cout << "Creating second volatile Subscriber..." << std::endl;
    mySub2= Domain::createSubscriber(g_part, Rparam2, nullptr);
    if(mySub2 == nullptr){
        std::cout << "something went wrong while creating the Volatile Subscriber..." << std::endl;
    }
}

class PubListener : public eprosima::fastrtps::PublisherListener
{
public:
    PubListener(){}
    ~PubListener(){}
    void onPublicationMatched(eprosima::fastrtps::Publisher*, eprosima::fastrtps::rtps::MatchingInfo& info) override
    {
        if(info.status == MATCHED_MATCHING)
        {
            std::cout << "Publisher matched"<<std::endl;
            //if (create_sub2 == nullptr)
            //{
            //    create_sub2 = new std::thread(&create_thread);
            //}
        }
        else
        {
            std::cout << "Publisher unmatched"<<std::endl;
        }
    }
};

class SubListener : public eprosima::fastrtps::SubscriberListener
{
public:
    SubListener(){}
    ~SubListener(){}
    void onSubscriptionMatched(eprosima::fastrtps::Subscriber*, eprosima::fastrtps::rtps::MatchingInfo& info)
    {
        if(info.status == MATCHED_MATCHING)
        {
            std::cout << "Subscriber matched"<<std::endl;
        }
        else
        {
            std::cout << "Subscriber unmatched"<<std::endl;
        }
    }

    void onNewDataMessage(eprosima::fastrtps::Subscriber*)
    {
        //if (create_sub2 == nullptr)
        //{
        //    create_sub2 = new std::thread(&create_thread);
       // }
    }
};

void latejoiners(){

    sample my_sample;
    SubListener m_listener;
    PubListener m_pub_listener;

    ParticipantAttributes ParticipantParam;
    ParticipantParam.rtps.builtin.domainId = 0;
    ParticipantParam.rtps.builtin.leaseDuration = c_TimeInfinite;
    ParticipantParam.rtps.setName("Participant");

    Participant *participant = Domain::createParticipant(ParticipantParam);
    if(participant == nullptr){
        std::cout << " Something went wrong while creating the Publisher Participant..." << std::endl;
        return;
    }
    Domain::registerType(participant,(TopicDataType*) &sampleType);
    g_part = participant;


    //Publisher config
    PublisherAttributes Pparam;
    Pparam.topic.topicDataType = sampleType.getName();
    Pparam.topic.topicName = "samplePubSubTopic";

    Pparam.topic.historyQos.kind = KEEP_ALL_HISTORY_QOS;
    Pparam.qos.m_reliability.kind = RELIABLE_RELIABILITY_QOS;
    Pparam.qos.m_durability.kind = VOLATILE_DURABILITY_QOS;
    Pparam.qos.m_publishMode.kind = ASYNCHRONOUS_PUBLISH_MODE;

    std::cout << "Creating Publisher..." << std::endl;
    Publisher *myPub= Domain::createPublisher(participant, Pparam, &m_pub_listener);
    if(myPub == nullptr){
        std::cout << "Something went wrong while creating the Publisher..." << std::endl;
        return;
    }

    //std::cout << "Publishing 1 sample on the topic" << std::endl;
    //my_sample.index(0);
    //my_sample.key_value(1);
    //myPub->write(&my_sample);

    //Transient Local Sub
    SubscriberAttributes Rparam;
    Rparam.topic.topicDataType = sampleType.getName();
    Rparam.topic.topicName = "samplePubSubTopic";

    Rparam.topic.historyQos.kind = KEEP_ALL_HISTORY_QOS;
    Rparam.qos.m_reliability.kind = RELIABLE_RELIABILITY_QOS;
    Rparam.qos.m_durability.kind = VOLATILE_DURABILITY_QOS;

    std::cout << "Creating first volatile Subscriber..." << std::endl;
    Subscriber *mySub1= Domain::createSubscriber(participant, Rparam, &m_listener);
    if(mySub1 == nullptr){
        std::cout << "something went wrong while creating the Transient Local Subscriber..." << std::endl;
        return;
    }

    //Send 20 samples
    //std::cout << "Publishing another sample on the topic" << std::endl;
    //my_sample.index(1);
    //my_sample.key_value(1);
   // myPub->write(&my_sample);

    //Volatile Sub
    //SubscriberAttributes Rparam2;
    //Rparam2.topic.topicDataType = sampleType.getName();
    //Rparam2.topic.topicName = "samplePubSubTopic";

    //Rparam2.topic.historyQos.kind = KEEP_ALL_HISTORY_QOS;
    //Rparam2.qos.m_reliability.kind = RELIABLE_RELIABILITY_QOS;
    //Rparam2.qos.m_durability.kind = VOLATILE_DURABILITY_QOS;

    //std::cout << "Creating second volatile Subscriber..." << std::endl;
    //Subscriber *mySub2= Domain::createSubscriber(participant, Rparam2, nullptr);
    //if(mySub2 == nullptr){
    //    std::cout << "something went wrong while creating the Volatile Subscriber..." << std::endl;
    //    return;
    //}

    create_thread();

    //Send 20 samples
    //Subscriber* mySub1 = nullptr;
    std::cout << "Publishing 20 samples on the topic..." << std::endl;
    for(uint8_t j=0; j < 40 /*&& mySub2 == nullptr*/; j++){
        my_sample.index(j+2);
        my_sample.key_value(1);
        myPub->write(&my_sample);

        /*
        if (j == 1)
        {
            std::cout << "Creating first volatile Subscriber..." << std::endl;
            mySub1 = Domain::createSubscriber(participant, Rparam, &m_listener);
            if(myPub == nullptr){
                std::cout << "something went wrong while creating the Transient Local Subscriber..." << std::endl;
                return;
            }
        }
        */
    }

    eClock::my_sleep(1500);

    //create_sub2->join();

    //Read the contents of both histories:
    std::cout << "The first Subscriber holds: " << std::endl;
    while(mySub1->readNextData(&my_sample, &sample_info)){
        std::cout << std::to_string(my_sample.index()) << " ";
    }
    std::cout << std::endl;
    std::cout << "The second Subscriber holds: " << std::endl;
    while(mySub2->readNextData(&my_sample, &sample_info)){
        std::cout << std::to_string(my_sample.index()) << " ";
    }
    std::cout << std::endl;
}
