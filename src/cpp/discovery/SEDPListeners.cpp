/*************************************************************************
 * Copyright (c) 2014 eProsima. All rights reserved.
 *
 * This copy of eProsima RTPS is licensed to you under the terms described in the
 * EPROSIMARTPS_LIBRARY_LICENSE file included in this distribution.
 *
 *************************************************************************/

/**
 * @file SEDPListeners.cpp
 *
 *  Created on: May 19, 2014
 *      Author: Gonzalo Rodriguez Canosa
 *      email:  gonzalorodriguez@eprosima.com
 *              grcanosa@gmail.com  	
 */

#include "eprosimartps/discovery/SEDPListeners.h"

#include "eprosimartps/Participant.h"

#include "eprosimartps/discovery/data/DiscoveredWriterData.h"
#include "eprosimartps/discovery/data/DiscoveredReaderData.h"

#include "eprosimartps/reader/StatefulReader.h"
#include "eprosimartps/discovery/ParticipantDiscoveryProtocol.h"
#include "eprosimartps/discovery/SimpleEDP.h"
#include "eprosimartps/discovery/data/DiscoveredData.h"

#include "eprosimartps/utils/RTPSLog.h"



namespace eprosima {
namespace rtps {

void SEDPPubListener::onNewDataMessage()
{
	pInfo(CYAN<<"SEDP PUB Listener:onNewDataMessage"<<DEF<<endl);
	CacheChange_t* change;
	if(this->mp_SEDP->mp_PubReader->get_last_added_cache(&change))
	{
		ParameterList_t param;
		CDRMessage_t msg;
		msg.msg_endian = change->serializedPayload.encapsulation == PL_CDR_BE ? BIGEND:LITTLEEND;
		msg.length = change->serializedPayload.length;
		memcpy(msg.buffer,change->serializedPayload.data,msg.length);
		if(ParameterList::readParameterListfromCDRMsg(&msg,&param,NULL,NULL)>0)
		{
			DiscoveredWriterData wdata;
			if(DiscoveredData::ParameterList2DiscoveredWriterData(param,&wdata))
			{
				change->instanceHandle = wdata.m_key;
				if(wdata.m_writerProxy.remoteWriterGuid.guidPrefix == mp_SEDP->mp_PDP->mp_localDPData->m_guidPrefix)
				{
					//cout << "SMAE"<<endl;
					pInfo(CYAN<<"SEDP Pub Listener: Message from own participant, removing"<<DEF<<endl)
							this->mp_SEDP->mp_PubReader->remove_change(change->sequenceNumber,change->writerGUID);
					return;
				}
				DiscoveredParticipantData* pdata = NULL;
				for(std::vector<DiscoveredParticipantData>::iterator pit = this->mp_SEDP->mp_PDP->m_discoveredParticipants.begin();
						pit!=this->mp_SEDP->mp_PDP->m_discoveredParticipants.begin();++pit)
				{
					if(pit->m_guidPrefix == wdata.m_writerProxy.remoteWriterGuid.guidPrefix)
					{
						pdata = &(*pit);
						break;
					}
				}
				if(pdata == NULL)
				{
					pWarning("Publications Listener: received message from UNKNOWN participant, removing"<<endl);
					this->mp_SEDP->mp_PubReader->remove_change(change->sequenceNumber,change->writerGUID);
					return;
				}
				bool already_in_history = false;
				//Check if CacheChange_t with same Key is already in History:
				for(std::vector<CacheChange_t*>::iterator it = this->mp_SEDP->mp_PubReader->readerHistoryCacheBegin();
						it!=this->mp_SEDP->mp_PubReader->readerHistoryCacheEnd();++it)
				{
					if((*it)->instanceHandle == change->instanceHandle)
					{
						this->mp_SEDP->mp_PubReader->remove_change((*it)->sequenceNumber,(*it)->writerGUID);
						already_in_history = true;
						break;
					}
				}
				DiscoveredWriterData* wdataptr = NULL;
				if(already_in_history)
				{
					for(std::vector<DiscoveredWriterData>::iterator it = pdata->m_writers.begin();
							it!=pdata->m_writers.end();++it)
					{
						if(it->m_key == change->instanceHandle)
						{
							wdataptr = &(*it);
							*wdataptr = wdata;
							break;
						}
					}
				}
				else
				{
					wdataptr = &wdata;
					pdata->m_writers.push_back(*wdataptr);
				}
				for(std::vector<RTPSReader*>::iterator rit = this->mp_SEDP->mp_PDP->mp_participant->userReadersListBegin();
						rit!=this->mp_SEDP->mp_PDP->mp_participant->userReadersListEnd();++rit)
				{
					if(already_in_history)
					{
						this->mp_SEDP->updateReaderMatching(*rit,wdataptr);
					}
					else
					{
						this->mp_SEDP->localReaderMatching(*rit,wdataptr);
					}
				}
			}
			param.deleteParams();
		}
		else
		{
			pError("SEDP Pub Listener: error reading Parameters from CDRMessage"<<endl);
			param.deleteParams();
			return;
		}
	}
	return;
}


void SEDPSubListener::onNewDataMessage()
{
	pInfo(CYAN<<"SEDP SUB Listener:onNewDataMessage"<<DEF<<endl);
	CacheChange_t* change;
	if(this->mp_SEDP->mp_SubReader->get_last_added_cache(&change))
	{
		ParameterList_t param;
		CDRMessage_t msg;
		msg.msg_endian = change->serializedPayload.encapsulation == PL_CDR_BE ? BIGEND:LITTLEEND;
		msg.length = change->serializedPayload.length;
		memcpy(msg.buffer,change->serializedPayload.data,msg.length);
		if(ParameterList::readParameterListfromCDRMsg(&msg,&param,NULL,NULL)>0)
		{
			DiscoveredReaderData rdata;
			if(DiscoveredData::ParameterList2DiscoveredReaderData(param,&rdata))
			{
				change->instanceHandle = rdata.m_key;
				if(rdata.m_readerProxy.remoteReaderGuid.guidPrefix == mp_SEDP->mp_PDP->mp_localDPData->m_guidPrefix)
				{
					//cout << "SMAE"<<endl;
					pInfo(CYAN<<"SEDP Sub Listener: Message from own participant, removing"<<DEF<<endl)
							this->mp_SEDP->mp_SubReader->remove_change(change->sequenceNumber,change->writerGUID);
					return;
				}
				DiscoveredParticipantData* pdata = NULL;
				for(std::vector<DiscoveredParticipantData>::iterator pit = this->mp_SEDP->mp_PDP->m_discoveredParticipants.begin();
						pit!=this->mp_SEDP->mp_PDP->m_discoveredParticipants.begin();++pit)
				{
					cout << "loop:" << pit->m_guidPrefix << endl;
					cout << "read:" << rdata.m_readerProxy.remoteReaderGuid.guidPrefix << endl;
					if(pit->m_guidPrefix == rdata.m_readerProxy.remoteReaderGuid.guidPrefix)
					{
						pdata = &(*pit);
						break;
					}
				}
				if(pdata == NULL)
				{
					pWarning("Subscriptions Listener: received message from UNKNOWN participant, removing"<<endl);
					this->mp_SEDP->mp_SubReader->remove_change(change->sequenceNumber,change->writerGUID);
					return;
				}
				bool already_in_history = false;
				//Check if CacheChange_t with same Key is already in History:
				for(std::vector<CacheChange_t*>::iterator it = this->mp_SEDP->mp_SubReader->readerHistoryCacheBegin();
						it!=this->mp_SEDP->mp_SubReader->readerHistoryCacheEnd();++it)
				{
					if((*it)->instanceHandle == change->instanceHandle)
					{
						this->mp_SEDP->mp_SubReader->remove_change((*it)->sequenceNumber,(*it)->writerGUID);
						already_in_history = true;
						break;
					}
				}
				DiscoveredReaderData* rdataptr = NULL;
				if(already_in_history)
				{
					for(std::vector<DiscoveredReaderData>::iterator it = pdata->m_readers.begin();
							it!=pdata->m_readers.end();++it)
					{
						if(it->m_key == change->instanceHandle)
						{
							rdataptr = &(*it);
							*rdataptr = rdata;
							break;
						}
					}
				}
				else
				{
					rdataptr = &rdata;
					pdata->m_readers.push_back(*rdataptr);
				}
				for(std::vector<RTPSWriter*>::iterator wit = this->mp_SEDP->mp_PDP->mp_participant->userWritersListBegin();
						wit!=this->mp_SEDP->mp_PDP->mp_participant->userWritersListEnd();++wit)
				{
					if(already_in_history)
					{
						this->mp_SEDP->updateWriterMatching(*wit,rdataptr);
					}
					else
					{
						this->mp_SEDP->localWriterMatching(*wit,rdataptr);
					}
				}
			}
			param.deleteParams();
		}
		else
		{
			pError("SEDPSubListener: error reading Parameters from CDRMessage"<<endl);
			param.deleteParams();
			return;
		}
	}
	return;
}



} /* namespace rtps */
} /* namespace eprosima */