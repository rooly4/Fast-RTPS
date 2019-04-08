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

/**
 * @file Liveliness_main.cpp
 *
 */

#include "LivelinessPublisher.h"
#include "LivelinessSubscriber.h"

#include <fastrtps/Domain.h>
#include <fastrtps/utils/eClock.h>
#include <fastrtps/log/Log.h>
#include <fastrtps/qos/QosPolicies.h>

using namespace eprosima;
using namespace fastrtps;
using namespace rtps;

int main(int argc, char** argv)
{
    int type = 1;
    int count = 10;
    long sleep = 1000;

    eprosima::fastrtps::LivelinessQosPolicyKind liveliness = eprosima::fastrtps::AUTOMATIC_LIVELINESS_QOS;

    if(argc > 1)
    {
        if(strcmp(argv[1], "publisher")==0)
        {
            type = 1;
            if (argc > 2)
            {
                if (strcmp(argv[2], "AUTOMATIC") == 0)
                {
                    liveliness = eprosima::fastrtps::AUTOMATIC_LIVELINESS_QOS;
                }
                else if (strcmp(argv[2], "MANUAL_BY_PARTICIPANT") == 0)
                {
                    liveliness = eprosima::fastrtps::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS;
                }
                else if (strcmp(argv[2], "MANUAL_BY_TOPIC") == 0)
                {
                    liveliness = eprosima::fastrtps::MANUAL_BY_TOPIC_LIVELINESS_QOS;
                }
                else
                {
                    std::cout << "Unknown liveliness kind" << std::endl;
                    return 0;
                }

                if (argc > 3)
                {
                    count = atoi(argv[2]);
                    if (argc > 4)
                    {
                        sleep = atoi(argv[3]);
                    }
                }

            }
        }
        else if(strcmp(argv[1],"subscriber")==0)
        {
            type = 2;
            if (argc > 2)
            {
                if (strcmp(argv[2], "AUTOMATIC") == 0)
                {
                    liveliness = eprosima::fastrtps::AUTOMATIC_LIVELINESS_QOS;
                }
                else if (strcmp(argv[2], "MANUAL_BY_PARTICIPANT") == 0)
                {
                    liveliness = eprosima::fastrtps::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS;
                }
                else if (strcmp(argv[2], "MANUAL_BY_TOPIC") == 0)
                {
                    liveliness = eprosima::fastrtps::MANUAL_BY_TOPIC_LIVELINESS_QOS;
                }
                else
                {
                    std::cout << "Unknown liveliness kind" << std::endl;
                    return 0;
                }
            }
        }
    }
    else
    {
        std::cout << "publisher OR subscriber argument needed" << std::endl;
        Log::Reset();
        return 0;
    }

    switch(type)
    {
        case 1:
            {
                LivelinessPublisher mypub;
                if(mypub.init(liveliness))
                {
                    mypub.run(count, sleep);
                }
                break;
            }
        case 2:
            {
                LivelinessSubscriber mysub;
                if(mysub.init(liveliness))
                {
                    mysub.run();
                }
                break;
            }
    }
    Domain::stopAll();
    Log::Reset();
    return 0;
}
