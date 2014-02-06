/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2004,2005,2006 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Federico Maguolo <maguolof@dei.unipd.it>
 */

#include "cara-cca-wifi-manager.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"

#define Min(a,b) ((a < b) ? a : b)

NS_LOG_COMPONENT_DEFINE ("CaraCca");


namespace ns3 {

struct CaraCcaWifiRemoteStation : public WifiRemoteStation
{
  uint32_t m_timer;
  uint32_t m_success;
  uint32_t m_failed;
  uint32_t m_rate;
};

NS_OBJECT_ENSURE_REGISTERED (CaraCcaWifiManager);

TypeId
CaraCcaWifiManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CaraCcaWifiManager")
    .SetParent<WifiRemoteStationManager> ()
    .AddConstructor<CaraCcaWifiManager> ()
    .AddAttribute ("ProbeThreshold",
                   "The number of consecutive transmissions failure to activate the RTS probe.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&CaraCcaWifiManager::m_probeThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("FailureThreshold",
                   "The number of consecutive transmissions failure to decrease the rate.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&CaraCcaWifiManager::m_failureThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SuccessThreshold",
                   "The minimum number of sucessfull transmissions to try a new rate.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&CaraCcaWifiManager::m_successThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Timeout",
                   "The 'timer' in the CARA algorithm",
                   UintegerValue (15),
                   MakeUintegerAccessor (&CaraCcaWifiManager::m_timerTimeout),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

CaraCcaWifiManager::CaraCcaWifiManager ()
  : WifiRemoteStationManager ()
{
  NS_LOG_FUNCTION (this);


  //std::cout << "cara cca wifi manager has been created!" << std::endl;
}
CaraCcaWifiManager::~CaraCcaWifiManager ()
{
  NS_LOG_FUNCTION (this);
}

WifiRemoteStation *
CaraCcaWifiManager::DoCreateStation (void) const
{
  NS_LOG_FUNCTION (this);
  CaraCcaWifiRemoteStation *station = new CaraCcaWifiRemoteStation ();
  station->m_rate = 0;
  station->m_success = 0;
  station->m_failed = 0;
  station->m_timer = 0;
  return station;
}

void
CaraCcaWifiManager::DoReportRtsFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
}

void
CaraCcaWifiManager::DoReportDataFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  CaraCcaWifiRemoteStation *station = (CaraCcaWifiRemoteStation *) st;
  station->m_timer++;
  station->m_failed++;
  station->m_success = 0;

  if (isCcaBusy)
  {
    //std::cout << "CaraCcaWifiManager::DoReportDataFailed isCcaBusy = " << isCcaBusy << std::endl;
  }

  if (station->m_failed >= m_failureThreshold && !isCcaBusy)
    {
      NS_LOG_DEBUG ("self=" << station << " dec rate");
      if (station->m_rate != 0)
        {
          station->m_rate--;
        }
      station->m_failed = 0;
      station->m_timer = 0;
      //
    }

  isCcaBusy = false;
}
void
CaraCcaWifiManager::DoReportRxOk (WifiRemoteStation *st,
                               double rxSnr, WifiMode txMode)
{
  NS_LOG_FUNCTION (this << st << rxSnr << txMode);
}
void
CaraCcaWifiManager::DoReportRtsOk (WifiRemoteStation *st,
                                double ctsSnr, WifiMode ctsMode, double rtsSnr)
{
  NS_LOG_FUNCTION (this << st << ctsSnr << ctsMode << rtsSnr);
  NS_LOG_DEBUG ("self=" << st << " rts ok");
}
void
CaraCcaWifiManager::DoReportDataOk (WifiRemoteStation *st,
                                 double ackSnr, WifiMode ackMode, double dataSnr)
{
  NS_LOG_FUNCTION (this << st << ackSnr << ackMode << dataSnr);
  CaraCcaWifiRemoteStation *station = (CaraCcaWifiRemoteStation *) st;
  station->m_timer++;
  station->m_success++;
  station->m_failed = 0;

  //std::cout << "CaraCcaWifiManager::DoReportDataOk" << std::endl;

  NS_LOG_DEBUG ("self=" << station << " data ok success=" << station->m_success << ", timer=" << station->m_timer);
  if ((station->m_success == m_successThreshold
       || station->m_timer >= m_timerTimeout))
    {
      if (station->m_rate < GetNSupported (station) - 1)
        {
          station->m_rate++;
        }
      NS_LOG_DEBUG ("self=" << station << " inc rate=" << station->m_rate);
      station->m_timer = 0;
      station->m_success = 0;
    }

  isCcaBusy = false;
}
void
CaraCcaWifiManager::DoReportFinalRtsFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
}
void
CaraCcaWifiManager::DoReportFinalDataFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
}

WifiTxVector
CaraCcaWifiManager::DoGetDataTxVector (WifiRemoteStation *st,
                                    uint32_t size)
{
  NS_LOG_FUNCTION (this << st << size);
  CaraCcaWifiRemoteStation *station = (CaraCcaWifiRemoteStation *) st;
  return WifiTxVector (GetSupported (station, station->m_rate), GetDefaultTxPowerLevel (), GetLongRetryCount (station), GetShortGuardInterval (station), Min (GetNumberOfReceiveAntennas (station),GetNumberOfTransmitAntennas()), GetNumberOfTransmitAntennas (station), GetStbc (station));
}
WifiTxVector
CaraCcaWifiManager::DoGetRtsTxVector (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  /// \todo we could/should implement the Arf algorithm for
  /// RTS only by picking a single rate within the BasicRateSet.
  return WifiTxVector (GetSupported (st, 0), GetDefaultTxPowerLevel (), GetLongRetryCount (st), GetShortGuardInterval (st), Min (GetNumberOfReceiveAntennas (st),GetNumberOfTransmitAntennas()), GetNumberOfTransmitAntennas (st), GetStbc (st));
}

bool
CaraCcaWifiManager::DoNeedRts (WifiRemoteStation *st,
                            Ptr<const Packet> packet, bool normally)
{
  NS_LOG_FUNCTION (this << st << normally);
  CaraCcaWifiRemoteStation *station = (CaraCcaWifiRemoteStation *) st;
  return normally || station->m_failed >= m_probeThreshold;
}

bool
CaraCcaWifiManager::IsLowLatency (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

} // namespace ns3
