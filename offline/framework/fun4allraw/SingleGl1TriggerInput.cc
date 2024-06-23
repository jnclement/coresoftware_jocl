#include "SingleGl1TriggerInput.h"

#include "Fun4AllPrdfInputTriggerManager.h"
#include "InputManagerType.h"

#include <ffarawobjects/Gl1Packetv2.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>    // for PHIODataNode
#include <phool/PHNode.h>          // for PHNode
#include <phool/PHNodeIterator.h>  // for PHNodeIterator
#include <phool/PHObject.h>        // for PHObject
#include <phool/getClass.h>
#include <phool/phool.h>

#include <Event/Event.h>
#include <Event/EventTypes.h>
#include <Event/Eventiterator.h>
#include <Event/packet.h>  // for Packet

#include <cstdint>   // for uint64_t
#include <iostream>  // for operator<<, basic_ostream<...
#include <iterator>  // for reverse_iterator
#include <limits>    // for numeric_limits
#include <memory>
#include <set>
#include <utility>  // for pair

SingleGl1TriggerInput::SingleGl1TriggerInput(const std::string &name)
  : SingleTriggerInput(name)
{
  SubsystemEnum(InputManagerType::GL1);
}

SingleGl1TriggerInput::~SingleGl1TriggerInput()
{
  CleanupUsedPackets(std::numeric_limits<int>::max());
// some events are already in the m_EventStack but they haven't been put
// into the m_PacketMap
   while(m_EventStack.begin() != m_EventStack.end())
   {
     m_EventStack.erase(m_EventStack.begin());
   }
}

void SingleGl1TriggerInput::FillPool(const unsigned int keep)
{
  if (AllDone())  // no more files and all events read
  {
    return;
  }
  while (GetEventiterator() == nullptr)  // at startup this is a null pointer
  {
    if (!OpenNextFile())
    {
      AllDone(1);
      return;
    }
  }
  while (GetSomeMoreEvents(keep))
  {
    std::unique_ptr<Event> evt(GetEventiterator()->getNextEvent());
    while (!evt)
    {
      fileclose();
      if (!OpenNextFile())
      {
        AllDone(1);
        return;
      }
      evt.reset(GetEventiterator()->getNextEvent());
    }
    if (Verbosity() > 2)
    {
      std::cout << PHWHERE << "Fetching next Event" << evt->getEvtSequence() << std::endl;
    }
    RunNumber(evt->getRunNumber());
    if (GetVerbosity() > 1)
    {
      evt->identify();
    }
    if (evt->getEvtType() != DATAEVENT)
    {
      m_NumSpecialEvents++;
      continue;
    }
    // static bool firstevent = true;
    // if (firstevent)
    // {
    //   firstevent = false;
    //   continue;
    // }
    int EventSequence = evt->getEvtSequence();
    Packet *packet = evt->getPacket(14001);

    if (Verbosity() > 1)
    {
      packet->identify();
    }

    // by default use previous bco clock for gtm bco
    Gl1Packet *newhit = new Gl1Packetv2();
    uint64_t gtm_bco = packet->lValue(0, "BCO");
    newhit->setBCO(packet->lValue(0, "BCO"));
    newhit->setHitFormat(packet->getHitFormat());
    newhit->setIdentifier(packet->getIdentifier());
    newhit->setEvtSequence(EventSequence);
    newhit->setPacketNumber(packet->iValue(0));
    newhit->setBunchNumber(packet->lValue(0, "BunchNumber"));
    newhit->setTriggerInput(packet->lValue(0, "TriggerInput"));
    newhit->setLiveVector(packet->lValue(0, "LiveVector"));
    newhit->setScaledVector(packet->lValue(0, "ScaledVector"));
    newhit->setGTMBusyVector(packet->lValue(0, "GTMBusyVector"));
    for (int i = 0; i < 64; i++)
    {
      for (int j = 0; j < 3; j++)
      {
        newhit->setScaler(i, j, packet->lValue(i, j));
      }
    }
    for (int i = 0; i < 12; i++)
    {
      newhit->setGl1pScaler(i, 0, packet->lValue(i, "GL1PRAW"));
      newhit->setGl1pScaler(i, 1, packet->lValue(i, "GL1PLIVE"));
      newhit->setGl1pScaler(i, 2, packet->lValue(i, "GL1PSCALED"));
    }
    if (Verbosity() > 2)
    {
      std::cout << PHWHERE << " Packet: " << packet->getIdentifier()
                << " evtno: " << EventSequence
                << ", bco: 0x" << std::hex << gtm_bco << std::dec
                << ", bunch no: " << packet->lValue(0, "BunchNumber")
                << std::endl;
      std::cout << PHWHERE << " RB Packet: " << newhit->getIdentifier()
                << " evtno: " << newhit->getEvtSequence()
                << ", bco: 0x" << std::hex << newhit->getBCO() << std::dec
                << ", bunch no: " << +newhit->getBunchNumber()
                << std::endl;
    }
    if (TriggerInputManager())
    {
      TriggerInputManager()->AddGl1Packet(EventSequence, newhit);
    }
    m_PacketMap[EventSequence].push_back(newhit);
    m_EventStack.insert(EventSequence);
    if (ddump_enabled())
    {
      ddumppacket(packet);
    }

    delete packet;
  }
}

void SingleGl1TriggerInput::Print(const std::string &what) const
{
  if (what == "ALL" || what == "STORAGE")
  {
    for (const auto &bcliter : m_PacketMap)
    {
      std::cout << PHWHERE << "Event: " << bcliter.first << std::endl;
    }
  }
  if (what == "ALL" || what == "STACK")
  {
    for (auto iter : m_EventStack)
    {
      std::cout << PHWHERE << "stacked event: " << iter << std::endl;
    }
  }
}

void SingleGl1TriggerInput::CleanupUsedPackets(const int eventno)
{
  std::vector<int> toclearevents;
  for (const auto &iter : m_PacketMap)
  {
    if (iter.first <= eventno)
    {
      for (auto pktiter : iter.second)
      {
        delete pktiter;
      }
      toclearevents.push_back(iter.first);
    }
    else
    {
      break;
    }
  }

  for (auto iter : toclearevents)
  {
    m_EventStack.erase(iter);
    m_PacketMap.erase(iter);
  }
}

void SingleGl1TriggerInput::ClearCurrentEvent()
{
  // called interactively, to get rid of the current event
  int currentevent = *m_EventStack.begin();
  //  std::cout << PHWHERE << "clearing bclk 0x" << std::hex << currentbclk << std::dec << std::endl;
  CleanupUsedPackets(currentevent);
  return;
}

bool SingleGl1TriggerInput::GetSomeMoreEvents(const unsigned int keep)
{
  if (AllDone())
  {
    return false;
  }
  if (m_PacketMap.empty())
  {
    return true;
  }

  int first_event = m_PacketMap.begin()->first;
  int last_event = m_PacketMap.rbegin()->first;
  if (Verbosity() > 1)
  {
    std::cout << PHWHERE << "first event: " << first_event
              << " last event: " << last_event
              << std::endl;
  }
  if (keep > 2 && m_PacketMap.size() < keep)
  {
    return true;
  }
  if (first_event >= last_event)
  {
    return true;
  }
  return false;
}

void SingleGl1TriggerInput::CreateDSTNode(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
  {
    dstNode = new PHCompositeNode("DST");
    topNode->addNode(dstNode);
  }
  PHNodeIterator iterDst(dstNode);
  PHCompositeNode *detNode = dynamic_cast<PHCompositeNode *>(iterDst.findFirst("PHCompositeNode", "GL1"));
  if (!detNode)
  {
    detNode = new PHCompositeNode("GL1");
    dstNode->addNode(detNode);
  }
  OfflinePacket *gl1hitcont = findNode::getClass<OfflinePacket>(detNode, "GL1Packet");
  if (!gl1hitcont)
  {
    gl1hitcont = new Gl1Packetv2();
    PHIODataNode<PHObject> *newNode = new PHIODataNode<PHObject>(gl1hitcont, "GL1Packet", "PHObject");
    detNode->addNode(newNode);
  }
}

// void SingleGl1TriggerInput::ConfigureStreamingInputManager()
// {
//   if (StreamingInputManager())
//   {
//     StreamingInputManager()->SetGl1BcoRange(m_BcoRange);
//   }
//   return;
// }
