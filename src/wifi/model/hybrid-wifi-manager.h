/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
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
#ifndef HYBRID_WIFI_MANAGER_H
#define HYBRID_WIFI_MANAGER_H

#include "wifi-remote-station-manager.h"
#include <queue>

namespace ns3 {

/**
 * \brief implement the CARA rate control algorithm
 * \ingroup wifi
 *
 * Implement the CARA algorithm from:
 * J. Kim, S. Kim, S. Choi, and D. Qiao.
 * "CARA: Collision-Aware Rate Adaptation for IEEE 802.11 WLANs."
 *
 * Originally implemented by Federico Maguolo for a very early
 * prototype version of ns-3.
 */


class HybridWifiManager : public WifiRemoteStationManager
{
public:
  static TypeId GetTypeId (void);
  HybridWifiManager ();
  virtual ~HybridWifiManager ();

private:
  // overriden from base class
  virtual WifiRemoteStation * DoCreateStation (void) const;
  virtual void DoReportRxOk (WifiRemoteStation *station,
                             double rxSnr, WifiMode txMode);
  virtual void DoReportRtsFailed (WifiRemoteStation *station);
  virtual void DoReportDataFailed (WifiRemoteStation *station);
  virtual void DoReportRtsOk (WifiRemoteStation *station,
                              double ctsSnr, WifiMode ctsMode, double rtsSnr);
  virtual void DoReportDataOk (WifiRemoteStation *station,
                               double ackSnr, WifiMode ackMode, double dataSnr);
  virtual void DoReportFinalRtsFailed (WifiRemoteStation *station);
  virtual void DoReportFinalDataFailed (WifiRemoteStation *station);
  virtual WifiTxVector DoGetDataTxVector (WifiRemoteStation *station, uint32_t size);
  virtual WifiTxVector DoGetRtsTxVector (WifiRemoteStation *station);
  virtual bool DoNeedRts (WifiRemoteStation *station,
                          Ptr<const Packet> packet, bool normally);
  virtual bool IsLowLatency (void) const;

  bool CalcSuccessRatio();
  bool DoesSuccessRatioJump();

  uint32_t m_packetsToSwtich;
  uint32_t m_timerTimeout;
  uint32_t m_successThreshold;
  uint32_t m_failureThreshold;
  uint32_t m_probeThreshold;

  //aarf
  uint32_t m_minTimerThreshold;
  uint32_t m_minSuccessThreshold;
  double m_successK;
  uint32_t m_maxSuccessThreshold;
  double m_timerK;

  bool doProbe;
  int probe_caraRate;
  int probe_aarfRate;

  bool isCcaBusy;

  int mode;
  // 0 -- cara
  // 1 -- aarf

  unsigned int totalPackets;
  bool onlyStarted;

  // change sensitivity
  unsigned int successfulPacketsCount;
  unsigned int failedPacketsCount;
  std::vector<double> ratioHistory;
  int rateDecreased;

  void TryDoProbe();

  virtual void DoReportSifsAfterTx()
  {
    isCcaBusy = IsCcaBusy();
  }
};

} // namespace ns3

#endif /* HYBRID_WIFI_MANAGER_H */
