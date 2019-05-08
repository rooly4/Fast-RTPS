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
 * @file PDPClient.cpp
 *
 */

#include <fastrtps/rtps/builtin/discovery/participant/PDPClient.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDPListener.h>
#include <fastrtps/rtps/builtin/discovery/participant/timedevent/DSClientEvent.h>
#include <fastrtps/rtps/builtin/discovery/participant/timedevent/RemoteParticipantLeaseDuration.h>

#include <fastrtps/rtps/builtin/discovery/endpoint/EDPClient.h>

#include <fastrtps/rtps/builtin/BuiltinProtocols.h>
#include <fastrtps/rtps/builtin/liveliness/WLP.h>

#include <fastrtps/rtps/participant/RTPSParticipantListener.h>
#include <fastrtps/rtps/reader/StatefulReader.h>

#include <fastrtps/rtps/writer/ReaderProxy.h>

#include <fastrtps/rtps/history/WriterHistory.h>
#include <fastrtps/rtps/history/ReaderHistory.h>

#include <fastrtps/utils/TimeConversion.h>

#include "../../../participant/RTPSParticipantImpl.h"

#include <fastrtps/log/Log.h>


using namespace eprosima::fastrtps;

namespace eprosima {
namespace fastrtps{
namespace rtps {

GUID_t RemoteServerAttributes::GetParticipant() const
{
    return GUID_t(guidPrefix, c_EntityId_RTPSParticipant);
}

GUID_t RemoteServerAttributes::GetPDPReader() const
{
    return GUID_t(guidPrefix, c_EntityId_SPDPReader);
}

GUID_t RemoteServerAttributes::GetPDPWriter() const
{
    return GUID_t(guidPrefix, c_EntityId_SPDPWriter);
}

GUID_t RemoteServerAttributes::GetEDPPublicationsReader() const
{
    return GUID_t(guidPrefix, c_EntityId_SEDPPubReader);
}

GUID_t RemoteServerAttributes::GetEDPSubscriptionsWriter() const
{
    return GUID_t(guidPrefix, c_EntityId_SEDPSubWriter);
}

GUID_t RemoteServerAttributes::GetEDPPublicationsWriter() const
{
    return GUID_t(guidPrefix, c_EntityId_SEDPPubWriter);
}

GUID_t RemoteServerAttributes::GetEDPSubscriptionsReader() const
{
    return GUID_t(guidPrefix, c_EntityId_SEDPSubReader);
}

PDPClient::PDPClient(BuiltinProtocols* built)
    : PDP(built)
    , _msgbuffer(DISCOVERY_PARTICIPANT_DATA_MAX_SIZE, built->mp_participantImpl->getGuid().guidPrefix)
    , mp_sync(nullptr)
    ,_serverPing(false)
{

}

PDPClient::~PDPClient()
{
    if (mp_sync != nullptr)
        delete(mp_sync);
}

void PDPClient::initializeParticipantProxyData(ParticipantProxyData* participant_data)
{
    PDP::initializeParticipantProxyData(participant_data); // TODO: Remember that the PDP version USES security

    if(getRTPSParticipant()->getAttributes().builtin.discoveryProtocol != PDPType_t::CLIENT)
    {
        logError(RTPS_PDP, "Using a PDP client object with another user's settings");
    }

    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.use_PublicationWriterANDSubscriptionReader)
    {
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;
    }

    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.use_PublicationReaderANDSubscriptionWriter)
    {
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;
    }

//#if HAVE_SECURITY
//    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.enable_builtin_secure_publications_writer_and_subscriptions_reader)
//    {
//        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_SECURE_ANNOUNCER;
//        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_SECURE_DETECTOR;
//    }
//
//    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.enable_builtin_secure_subscriptions_writer_and_publications_reader)
//    {
//        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_SECURE_ANNOUNCER;
//        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_SECURE_DETECTOR;
//    }
//#endif

}

bool PDPClient::initPDP(RTPSParticipantImpl* part)
{
    if (!PDP::initPDP(part))
    {
        return false;
    }

    /* We keep using EPDSimple notwithstanding its method EDPSimple::assignRemoteEndpoints regards all server EDPs as TRANSIENT_LOCAL.
       Server builtin Writers are actually TRANSIENT. Currently this mistake is not an issue but must be kept in mind if further development
       justifies the creation of an EDPClient class.
    */
    mp_EDP = (EDP*)(new EDPClient(this,mp_RTPSParticipant));
    if(!mp_EDP->initEDP(m_discovery))
    {
        logError(RTPS_PDP,"Endpoint discovery configuration failed");
        return false;
    }

    mp_sync = new DSClientEvent(this, TimeConv::Time_t2MilliSecondsDouble(m_discovery.discoveryServer_client_syncperiod));
    mp_sync->restart_timer();

    return true;
}

ParticipantProxyData * PDPClient::createParticipantProxyData(const ParticipantProxyData & participant_data, const CacheChange_t &)
{
    std::unique_lock<std::recursive_mutex> lock(*getMutex());

    // Verify if this participant is a server
    bool is_server = false;
    for (auto & svr : mp_builtin->m_DiscoveryServers)
    {
        if (svr.guidPrefix == participant_data.m_guid.guidPrefix)
        {
            is_server = true;
        }
    }

    ParticipantProxyData * pdata = new ParticipantProxyData(participant_data);
    pdata->isAlive = true;

    // Clients only assert its server lifeliness, other clients liveliness is provided
    // through server's PDP discovery data
    if (is_server)
    {
        pdata->mp_leaseDurationTimer = new RemoteParticipantLeaseDuration(this,
            pdata,
            TimeConv::Time_t2MilliSecondsDouble(pdata->m_leaseDuration));
        pdata->mp_leaseDurationTimer->restart_timer();
    }
    else
    {
        pdata->mp_leaseDurationTimer = nullptr;
    }

    m_participantProxies.push_back(pdata);

    return pdata;
}


bool PDPClient::createPDPEndpoints()
{
    logInfo(RTPS_PDP,"Beginning PDPClient Endpoints creation");

    HistoryAttributes hatt;
    hatt.payloadMaxSize = DISCOVERY_PARTICIPANT_DATA_MAX_SIZE;
    hatt.initialReservedCaches = pdp_initial_reserved_caches;
    hatt.memoryPolicy = mp_builtin->m_att.readerHistoryMemoryPolicy;
    mp_PDPReaderHistory = new ReaderHistory(hatt);

    ReaderAttributes ratt;
    ratt.expectsInlineQos = false;
    ratt.endpoint.endpointKind = READER;
    ratt.endpoint.multicastLocatorList = mp_builtin->m_metatrafficMulticastLocatorList;
    ratt.endpoint.unicastLocatorList = mp_builtin->m_metatrafficUnicastLocatorList;
    ratt.endpoint.topicKind = WITH_KEY;
    ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
    ratt.endpoint.reliabilityKind = RELIABLE;
    ratt.times.heartbeatResponseDelay = pdp_heartbeat_response_delay;

    mp_listener = new PDPListener(this);

    if (mp_RTPSParticipant->createReader(&mp_PDPReader, ratt, mp_PDPReaderHistory, mp_listener, c_EntityId_SPDPReader, true, false))
    {
//#if HAVE_SECURITY
//        mp_RTPSParticipant->set_endpoint_rtps_protection_supports(rout, false);
//#endif
        // Initial peer list doesn't make sense in server scenario. Client should match its server list
        for (auto it = mp_builtin->m_DiscoveryServers.begin(); it != mp_builtin->m_DiscoveryServers.end(); ++it)
        {
            RemoteWriterAttributes rwatt;

            rwatt.guid = it->GetPDPWriter();
            rwatt.endpoint.multicastLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rwatt.endpoint.unicastLocatorList.push_back(it->metatrafficUnicastLocatorList);
            rwatt.endpoint.topicKind = WITH_KEY;
            rwatt.endpoint.durabilityKind = TRANSIENT; // Server Information must be persistent
            rwatt.endpoint.reliabilityKind = RELIABLE;

            // TODO: remove the join when Reader and Writer match functions are updated
            rwatt.endpoint.remoteLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rwatt.endpoint.remoteLocatorList.push_back(it->metatrafficUnicastLocatorList);

            mp_PDPReader->matched_writer_add(rwatt);
        }

    }
    else
    {
        logError(RTPS_PDP, "PDPClient Reader creation failed");
        delete(mp_PDPReaderHistory);
        mp_PDPReaderHistory = nullptr;
        delete(mp_listener);
        mp_listener = nullptr;
        return false;
    }

    hatt.payloadMaxSize = DISCOVERY_PARTICIPANT_DATA_MAX_SIZE;
    hatt.initialReservedCaches = pdp_initial_reserved_caches;
    hatt.memoryPolicy = mp_builtin->m_att.writerHistoryMemoryPolicy;
    mp_PDPWriterHistory = new WriterHistory(hatt);

    WriterAttributes watt;
    watt.endpoint.endpointKind = WRITER;
    watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
    watt.endpoint.reliabilityKind = RELIABLE;
    watt.endpoint.topicKind = WITH_KEY;
    watt.endpoint.multicastLocatorList = mp_builtin->m_metatrafficMulticastLocatorList;
    watt.endpoint.unicastLocatorList = mp_builtin->m_metatrafficUnicastLocatorList;
    watt.times.heartbeatPeriod = pdp_heartbeat_period;
    watt.times.nackResponseDelay = pdp_nack_response_delay;
    watt.times.nackSupressionDuration = pdp_nack_supression_duration;

    if (mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.bytesPerPeriod != UINT32_MAX &&
        mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.periodMillisecs != 0)
    {
        watt.mode = ASYNCHRONOUS_WRITER;
    }

    if (mp_RTPSParticipant->createWriter(&mp_PDPWriter, watt, mp_PDPWriterHistory, nullptr, c_EntityId_SPDPWriter, true))
    {
//#if HAVE_SECURITY
//        mp_RTPSParticipant->set_endpoint_rtps_protection_supports(wout, false);
//#endif
        for (auto it = mp_builtin->m_DiscoveryServers.begin(); it != mp_builtin->m_DiscoveryServers.end(); ++it)
        {
            RemoteReaderAttributes rratt;

            rratt.guid = it->GetPDPReader();
            rratt.endpoint.multicastLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rratt.endpoint.unicastLocatorList.push_back(it->metatrafficUnicastLocatorList);
            rratt.endpoint.topicKind = WITH_KEY;
            rratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
            rratt.endpoint.reliabilityKind = RELIABLE;

            // TODO: remove the join when Reader and Writer match functions are updated
            rratt.endpoint.remoteLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rratt.endpoint.remoteLocatorList.push_back(it->metatrafficUnicastLocatorList);

            mp_PDPWriter->matched_reader_add(rratt);
        }

    }
    else
    {
        logError(RTPS_PDP, "PDPClient Writer creation failed");
        delete(mp_PDPWriterHistory);
        mp_PDPWriterHistory = nullptr;
        return false;
    }
    logInfo(RTPS_PDP,"PDPClient Endpoints creation finished");
    return true;
}

// the ParticipantProxyData* pdata must be the one kept in PDP database
void PDPClient::assignRemoteEndpoints(ParticipantProxyData* pdata)
{
    std::unique_lock<std::recursive_mutex> lock(*getMutex());

    // Verify if this participant is a server
    for (auto & svr : mp_builtin->m_DiscoveryServers)
    {
        if (svr.guidPrefix == pdata->m_guid.guidPrefix)
        {
            svr.proxy = pdata;
        }
    }

    notifyAboveRemoteEndpoints(*pdata);
}

void PDPClient::notifyAboveRemoteEndpoints(const ParticipantProxyData& pdata)
{
    // No EDP notification needed. EDP endpoints would be match when PDP synchronization is granted
    if (mp_builtin->mp_WLP != nullptr)
        mp_builtin->mp_WLP->assignRemoteEndpoints(pdata);
}


void PDPClient::removeRemoteEndpoints(ParticipantProxyData* pdata)
{
    // EDP endpoints have been already unmatch by the associated listener
    assert(!mp_EDP->areRemoteEndpointsMatched(pdata));

    bool is_server = false;
    { 
        std::unique_lock<std::recursive_mutex> lock(*getMutex());

        // Verify if this participant is a server
        for (auto & svr : mp_builtin->m_DiscoveryServers)
        {
            if (svr.guidPrefix == pdata->m_guid.guidPrefix)
            {
                svr.proxy = nullptr; // reasign when we receive again server DATA(p)
                is_server = true;
                mp_sync->restart_timer(); // enable announcement and sync mechanism till this server reappears
            }
        }
    }

    if (is_server)
    {
        // We should unmatch and match the PDP endpoints to renew the PDP reader and writer associated proxies
        logInfo(RTPS_PDP, "For unmatching for server: " << pdata->m_guid);
        uint32_t endp = pdata->m_availableBuiltinEndpoints;
        uint32_t auxendp = endp;
        auxendp &= DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;

        if (auxendp != 0)
        {
            RemoteWriterAttributes watt;

            watt.guid.guidPrefix = pdata->m_guid.guidPrefix;
            watt.guid.entityId = c_EntityId_SPDPWriter;
            watt.endpoint.persistence_guid = watt.guid;
            watt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
            watt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
            watt.endpoint.reliabilityKind = RELIABLE;
            watt.endpoint.durabilityKind = TRANSIENT;
            watt.endpoint.topicKind = WITH_KEY;

            // TODO remove the join when Reader and Writer match functions are updated
            watt.endpoint.remoteLocatorList.push_back(pdata->m_metatrafficUnicastLocatorList);
            watt.endpoint.remoteLocatorList.push_back(pdata->m_metatrafficMulticastLocatorList);

            mp_PDPReader->matched_writer_remove(watt);
            // rematch but discarding any previous state of the server
            // because we know the server shutdown intencionally (sent a DATA(p[UD]))
            mp_PDPReader->matched_writer_add(watt,false);
        }

        auxendp = endp;
        auxendp &= DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;

        if (auxendp != 0)
        {
            RemoteReaderAttributes ratt;

            ratt.expectsInlineQos = false;
            ratt.guid.guidPrefix = pdata->m_guid.guidPrefix;
            ratt.guid.entityId = c_EntityId_SPDPReader;
            ratt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
            ratt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
            ratt.endpoint.reliabilityKind = RELIABLE;
            ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
            ratt.endpoint.topicKind = WITH_KEY;

            mp_PDPWriter->matched_reader_remove(ratt);
            mp_PDPWriter->matched_reader_add(ratt);
        }
    }
}

bool PDPClient::all_servers_acknowledge_PDP()
{
    // check if already initialized
    assert(mp_PDPWriterHistory && mp_PDPWriter);

    // get a reference to client proxy data
    CacheChange_t * pPD;
    if (mp_PDPWriterHistory->get_min_change(&pPD))
    {
       return mp_PDPWriter->is_acked_by_all(pPD);
    }
    else
    {
        logError(RTPS_PDP, "ParticipantProxy data should have been added to client PDP history cache by a previous call to announceParticipantState()");
    }

    return false;
}

bool PDPClient::is_all_servers_PDPdata_updated()
{
    // Assess all server DATA has been received
    StatefulReader * pR = dynamic_cast<StatefulReader *>(mp_PDPReader);
    assert(pR);
    return pR->isInCleanState();
}

void PDPClient::announceParticipantState(bool new_change, bool dispose, WriteParams& )
{
    WriteParams wp;
    SampleIdentity local;
    local.writer_guid(mp_PDPWriter->getGuid());
    local.sequence_number(mp_PDPWriterHistory->next_sequence_number());
    wp.sample_identity(local);
    wp.related_sample_identity(local);

    // Add the write params to the sample
    if (dispose)
    {
        // we must assure when the server is dying that all client are send at least a DATA(p)
            // note here we can no longer receive and DATA or ACKNACK from clients.
            // In order to avoid that we send the message directly as in the standard stateless PDP

        StatefulWriter * pW = dynamic_cast<StatefulWriter*>(mp_PDPWriter);
        assert(pW);

        CacheChange_t* change = nullptr;

        if ((change = pW->new_change([]() -> uint32_t {return DISCOVERY_PARTICIPANT_DATA_MAX_SIZE; },
            NOT_ALIVE_DISPOSED_UNREGISTERED, getLocalParticipantProxyData()->m_key)))
        {
            // update the sequence number
            change->sequenceNumber = mp_PDPWriterHistory->next_sequence_number();
            change->write_params = wp;

            std::lock_guard<std::recursive_mutex> wlock(*pW->getMutex());

            RTPSMessageGroup group(getRTPSParticipant(), mp_PDPWriter, RTPSMessageGroup::WRITER, _msgbuffer);

            std::vector<GUID_t> remote_readers;
            LocatorList_t locators;

            for (auto it = pW->matchedReadersBegin(); it != pW->matchedReadersEnd(); ++it)
            {
                RemoteReaderAttributes & att = (*it)->m_att;
                remote_readers.push_back(att.guid);

                EndpointAttributes & ep = att.endpoint;
                locators.push_back(ep.unicastLocatorList);
                //locators.push_back(ep.multicastLocatorList);
            }

            if (!group.add_data(*change, remote_readers, locators, false))
            {
                logError(RTPS_PDP, "Error sending announcement from client to servers");
            }
        }

        // free change
        mp_PDPWriterHistory->release_Cache(change);
    }
    else
    {
        PDP::announceParticipantState(new_change, dispose, wp);

        if (!new_change)
        {
            // retrieve the participant discovery data
            CacheChange_t * pPD;
            if (mp_PDPWriterHistory->get_min_change(&pPD))
            {
                std::lock_guard<std::recursive_mutex> lock(*getMutex());

                RTPSMessageGroup group(getRTPSParticipant(), mp_PDPWriter, RTPSMessageGroup::WRITER, _msgbuffer);

                std::vector<GUID_t> remote_readers;
                LocatorList_t locators;

                for (auto & svr : mp_builtin->m_DiscoveryServers)
                {
                    // non-pinging announcements like lease duration ones must be
                    // broadcast to all servers
                    if (svr.proxy == nullptr || !_serverPing)
                    {
                        remote_readers.push_back(svr.GetPDPReader());
                        locators.push_back(svr.metatrafficMulticastLocatorList);
                        locators.push_back(svr.metatrafficUnicastLocatorList);
                    }
                }

                if (!group.add_data(*pPD, remote_readers, locators, false))
                {
                    logError(RTPS_PDP, "Error sending announcement from client to servers");
                }

                // ping done independtly of which triggered the announcement
                // note all event callbacks are currently serialized
                _serverPing = false;
            }
            else
            {
                logError(RTPS_PDP, "ParticipantProxy data should have been added to client PDP history cache by a previous call to announceParticipantState()");
            }
        }
    }
}

bool PDPClient::match_servers_EDP_endpoints()
{
    // PDP must have been initialize
    assert(mp_EDP);

    std::lock_guard<std::recursive_mutex> lock(*getMutex());
    bool all = true; // have all servers been discovered?

    for (auto & svr : mp_builtin->m_DiscoveryServers)
    {
        all &= (svr.proxy != nullptr);

        if (svr.proxy && !mp_EDP->areRemoteEndpointsMatched(svr.proxy))
        {
            mp_EDP->assignRemoteEndpoints(*svr.proxy);
        }
    }

    return all;
}

} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
