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

#include "hybrid-wifi-manager.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"
#include <iostream>
// #include <numeric>
#include <cmath>
// #include <ctgmath>

#define Min(a,b) ((a < b) ? a : b)
#define Max(a,b) ((a > b) ? a : b)

NS_LOG_COMPONENT_DEFINE ("Hybrid");


namespace ns3 {

/**
 * \brief hold per-remote-station state for CARA Wifi manager.
 *
 * This struct extends from WifiRemoteStation struct to hold additional
 * information required by the CARA Wifi manager
 */
struct HybridWifiRemoteStation : public WifiRemoteStation
{
  uint32_t m_timer;
  uint32_t m_success;
  uint32_t m_failed;
  uint32_t m_rate;

  //aarf
  bool m_recovery;
  uint32_t m_retry;

  uint32_t m_timerTimeout;
  uint32_t m_successThreshold;
};

NS_OBJECT_ENSURE_REGISTERED (HybridWifiManager)
  ;

TypeId
HybridWifiManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HybridWifiManager")
    .SetParent<WifiRemoteStationManager> ()
    .AddConstructor<HybridWifiManager> ()
    .AddAttribute ("ProbeThreshold",
                   "The number of consecutive transmissions failure to activate the RTS probe.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&HybridWifiManager::m_probeThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("FailureThreshold",
                   "The number of consecutive transmissions failure to decrease the rate.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&HybridWifiManager::m_failureThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SuccessThreshold",
                   "The minimum number of sucessfull transmissions to try a new rate.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&HybridWifiManager::m_successThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Timeout",
                   "The 'timer' in the CARA algorithm",
                   UintegerValue (15),
                   MakeUintegerAccessor (&HybridWifiManager::m_timerTimeout),
                   MakeUintegerChecker<uint32_t> ())
    // aarf 
    .AddAttribute ("SuccessK", "Multiplication factor for the success threshold in the AARF algorithm.",
                   DoubleValue (2.0),
                   MakeDoubleAccessor (&HybridWifiManager::m_successK),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TimerK",
                   "Multiplication factor for the timer threshold in the AARF algorithm.",
                   DoubleValue (2.0),
                   MakeDoubleAccessor (&HybridWifiManager::m_timerK),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxSuccessThreshold",
                   "Maximum value of the success threshold in the AARF algorithm.",
                   UintegerValue (60),
                   MakeUintegerAccessor (&HybridWifiManager::m_maxSuccessThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MinTimerThreshold",
                   "The minimum value for the 'timer' threshold in the AARF algorithm.",
                   UintegerValue (15),
                   MakeUintegerAccessor (&HybridWifiManager::m_minTimerThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MinSuccessThreshold",
                   "The minimum value for the success threshold in the AARF algorithm.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&HybridWifiManager::m_minSuccessThreshold),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

HybridWifiManager::HybridWifiManager ()
  : WifiRemoteStationManager ()
{
  mode = 0;
  doProbe = false;
  totalPackets = 0;
  rateDecreased = 0;
  onlyStarted = false;
  NS_LOG_FUNCTION (this);
}
HybridWifiManager::~HybridWifiManager ()
{
  NS_LOG_FUNCTION (this);
}

WifiRemoteStation *
HybridWifiManager::DoCreateStation (void) const
{
  NS_LOG_FUNCTION (this);
  HybridWifiRemoteStation *station = new HybridWifiRemoteStation ();
  station->m_rate = 0;
  station->m_success = 0;
  station->m_failed = 0;
  station->m_timer = 0;

  station->m_successThreshold = m_minSuccessThreshold;
  station->m_timerTimeout = m_minTimerThreshold;
  station->m_recovery = false;
  station->m_retry = 0;

  return station;
}

bool 
HybridWifiManager::CalcSuccessRatio()
{
  unsigned int threshold = 1000;

  if (successfulPacketsCount < threshold && failedPacketsCount < threshold)
  {
    return false;
  }

  if (ratioHistory.size() >= 10)
  {
    ratioHistory.erase(ratioHistory.begin());
  }

  double ratio = (double)successfulPacketsCount / (double)(successfulPacketsCount + failedPacketsCount);
  successfulPacketsCount = 0;
  failedPacketsCount = 0;

  ratioHistory.push_back(ratio);
  return true;

  // NS_LOG_UNCOND(Simulator::Now().GetSeconds());
  // NS_LOG_UNCOND(ratio);
}

double dAbs(double val)
{
  return val > 0 ? val : ((-1.0) * val);
}

bool 
HybridWifiManager::DoesSuccessRatioJump()
{
  if (ratioHistory.size() < 5)
  {
    return false;
  }

  std::vector<double> normlizedRatioHistory;

  double mean = 0.0;
  double lastVal = ratioHistory[ratioHistory.size() - 1];

  for(unsigned int i = 0; i < ratioHistory.size() - 1; ++i)
  {
    double currentVal = ratioHistory[i];
    normlizedRatioHistory.push_back(currentVal);
    mean += currentVal;
  }

  mean = mean / normlizedRatioHistory.size();

  double std = 0.0;

  for(std::vector<double>::iterator iter = normlizedRatioHistory.begin(); iter != normlizedRatioHistory.end(); iter++)
  {
    double sub = *iter - mean;
    std += sub * sub;
  }

  std = sqrt(std / normlizedRatioHistory.size());
  bool doesJump = dAbs(lastVal - mean) > 3.0 * std;

  // if (doesJump)
  // {
    // NS_LOG_UNCOND(Simulator::Now().GetSeconds());
    // NS_LOG_UNCOND(3.0 * std);
    // NS_LOG_UNCOND(dAbs(lastVal - mean));
    // NS_LOG_UNCOND(doesJump);
    // NS_LOG_UNCOND("------------------------");
  // }

  return doesJump;
}

void 
HybridWifiManager::TryDoProbe()
{
  bool newItemInserted = CalcSuccessRatio();

  if (!doProbe)
  {
    bool doesJump = false;

    if (newItemInserted)
    {
      doesJump = /*totalPackets > 10000 && */DoesSuccessRatioJump();
    }

    // condition to do probe
    if (onlyStarted ||
      doesJump)
    {
      NS_LOG_UNCOND(Simulator::Now().GetSeconds());
      NS_LOG_UNCOND("Probe started");
      //std::cout << "Probe started..." << std::endl;
      mode = 0;
      probe_caraRate = 0;
      probe_aarfRate = 0;
      m_packetsToSwtich = 1000;
      totalPackets = 0;
      onlyStarted = false;
      doProbe = true;
    }

    return;
  }

  if (m_packetsToSwtich == 500)
  {
    mode = 1;
  }

  m_packetsToSwtich--;

  if (m_packetsToSwtich == 0)
  {
    //std::cout << "Probe ended, aarfRate = " << probe_aarfRate << ", caraRate = " << probe_caraRate << std::endl;

    if (probe_aarfRate < probe_caraRate)
    {
      mode = 0;
    }
    else
    {
      mode = 1;
    }

    doProbe = false;
    rateDecreased = 0;
    // std::vector<double> empty;
    // std::swap(ratioHistory, empty);
  }
}

void
HybridWifiManager::DoReportRtsFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
}

void
HybridWifiManager::DoReportDataFailed (WifiRemoteStation *st)
{
  failedPacketsCount++;

  TryDoProbe();
  //totalPackets++;

  NS_LOG_FUNCTION (this << st);
  HybridWifiRemoteStation *station = (HybridWifiRemoteStation *) st;

  if (mode == 0) 
  {
    station->m_timer++;
    station->m_failed++;
    station->m_success = 0;

    if (station->m_failed >= m_failureThreshold /*&& !isCcaBusy*/)
      {
        NS_LOG_DEBUG ("self=" << station << " dec rate");
        if (station->m_rate != 0)
          {
            station->m_rate--;
            rateDecreased++;
  // NS_LOG_UNCOND(rateDecreased);
          }
        station->m_failed = 0;
        station->m_timer = 0;
      }
  }

  if (mode == 1)
  {
    station->m_timer++;
    station->m_failed++;
    station->m_retry++;
    station->m_success = 0;

    if (station->m_recovery)
      {
        NS_ASSERT (station->m_retry >= 1);
        if (station->m_retry == 1)
          {
            // need recovery fallback
            station->m_successThreshold = (int)(Min (station->m_successThreshold * m_successK,
                                                     m_maxSuccessThreshold));
            station->m_timerTimeout = (int)(Max (station->m_timerTimeout * m_timerK,
                                                 m_minSuccessThreshold));
            if (station->m_rate != 0)
              {
                station->m_rate--;
                rateDecreased++;
  // NS_LOG_UNCOND(rateDecreased);
              }
          }
        station->m_timer = 0;
      }
    else
      {
        NS_ASSERT (station->m_retry >= 1);
        if (((station->m_retry - 1) % 2) == 1)
          {
            // need normal fallback
            station->m_timerTimeout = m_minTimerThreshold;
            station->m_successThreshold = m_minSuccessThreshold;
            if (station->m_rate != 0)
              {
                station->m_rate--;
                rateDecreased++;
  // NS_LOG_UNCOND(rateDecreased);
              }
          }
        if (station->m_retry >= 2)
          {
            station->m_timer = 0;
          }
      }
  }
  
}
void
HybridWifiManager::DoReportRxOk (WifiRemoteStation *st,
                               double rxSnr, WifiMode txMode)
{
  NS_LOG_FUNCTION (this << st << rxSnr << txMode);
}
void
HybridWifiManager::DoReportRtsOk (WifiRemoteStation *st,
                                double ctsSnr, WifiMode ctsMode, double rtsSnr)
{
  NS_LOG_FUNCTION (this << st << ctsSnr << ctsMode << rtsSnr);
  NS_LOG_DEBUG ("self=" << st << " rts ok");
}
void
HybridWifiManager::DoReportDataOk (WifiRemoteStation *st,
                                 double ackSnr, WifiMode ackMode, double dataSnr)
{
  successfulPacketsCount++;

  TryDoProbe();
  totalPackets++;

  if (doProbe && mode == 0)
  {
    probe_caraRate++;
  }

  if (doProbe && mode == 1)
  {
    probe_aarfRate++;
  }

  NS_LOG_FUNCTION (this << st << ackSnr << ackMode << dataSnr);
  HybridWifiRemoteStation *station = (HybridWifiRemoteStation *) st;

  if (mode == 0)
  {
    station->m_timer++;
    station->m_success++;
    station->m_failed = 0;
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
  }
  
  if (mode == 1)
  {
    station->m_timer++;
    station->m_success++;
    station->m_failed = 0;
    station->m_recovery = false;
    station->m_retry = 0;
    NS_LOG_DEBUG ("station=" << station << " data ok success=" << station->m_success << ", timer=" << station->m_timer);
    if ((station->m_success == station->m_successThreshold
         || station->m_timer == station->m_timerTimeout)
        && (station->m_rate < (GetNSupported (station) - 1)))
      {
        NS_LOG_DEBUG ("station=" << station << " inc rate");
        station->m_rate++;
        station->m_timer = 0;
        station->m_success = 0;
        station->m_recovery = true;
      }
    }
}
void
HybridWifiManager::DoReportFinalRtsFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
}
void
HybridWifiManager::DoReportFinalDataFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
}

WifiTxVector
HybridWifiManager::DoGetDataTxVector (WifiRemoteStation *st,
                                    uint32_t size)
{
  NS_LOG_FUNCTION (this << st << size);
  HybridWifiRemoteStation *station = (HybridWifiRemoteStation *) st;
  return WifiTxVector (GetSupported (station, station->m_rate), GetDefaultTxPowerLevel (), GetLongRetryCount (station), GetShortGuardInterval (station), Min (GetNumberOfReceiveAntennas (station),GetNumberOfTransmitAntennas()), GetNumberOfTransmitAntennas (station), GetStbc (station));
}
WifiTxVector
HybridWifiManager::DoGetRtsTxVector (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  /// \todo we could/should implement the Arf algorithm for
  /// RTS only by picking a single rate within the BasicRateSet.
  return WifiTxVector (GetSupported (st, 0), GetDefaultTxPowerLevel (), GetLongRetryCount (st), GetShortGuardInterval (st), Min (GetNumberOfReceiveAntennas (st),GetNumberOfTransmitAntennas()), GetNumberOfTransmitAntennas (st), GetStbc (st));
}

bool
HybridWifiManager::DoNeedRts (WifiRemoteStation *st,
                            Ptr<const Packet> packet, bool normally)
{
  NS_LOG_FUNCTION (this << st << normally);
  if (mode == 0)
  {
    HybridWifiRemoteStation *station = (HybridWifiRemoteStation *) st;
    return normally || station->m_failed >= m_probeThreshold;  
  }

  return normally;
}

bool
HybridWifiManager::IsLowLatency (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

} // namespace ns3
