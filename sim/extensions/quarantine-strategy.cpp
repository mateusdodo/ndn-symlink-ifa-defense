#include "quarantine-strategy.hpp"
#include "ns3/simulator.h"
#include <chrono>

namespace nfd {
namespace fw {

std::set<FaceId> QuarantineStrategy::s_suspects;
Face* QuarantineStrategy::s_quarantineFace = nullptr;
double QuarantineStrategy::s_rlLimit = 0;
double QuarantineStrategy::s_qLimit = 0;
std::map<FaceId, QuarantineStrategy::RateWin> QuarantineStrategy::s_qWin;
std::map<FaceId, QuarantineStrategy::RateWin> QuarantineStrategy::s_win;
uint64_t QuarantineStrategy::s_hintSent = 0;
uint64_t QuarantineStrategy::s_normalSent = 0;
uint64_t QuarantineStrategy::s_rlDrops = 0;
uint64_t QuarantineStrategy::s_qDrops = 0;
uint64_t QuarantineStrategy::s_nsHint = 0;
uint64_t QuarantineStrategy::s_nHint = 0;
uint64_t QuarantineStrategy::s_nsNormal = 0;
uint64_t QuarantineStrategy::s_nNormal = 0;
uint64_t QuarantineStrategy::s_bytesHint = 0;
uint64_t QuarantineStrategy::s_bytesNormal = 0;

QuarantineStrategy::QuarantineStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
{
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

QuarantineStrategy::~QuarantineStrategy()
{
}

static bool
canForwardTo(const Face& inFace, const shared_ptr<pit::Entry>& pitEntry, const fib::NextHop& nh)
{
  return !wouldViolateScope(inFace, pitEntry->getInterest(), nh.getFace()) &&
         canForwardToLegacy(*pitEntry, nh.getFace());
}

void
QuarantineStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                         const shared_ptr<pit::Entry>& pitEntry)
{
  // A2: rate-limit por face de entrada (todas as faces, igualmente)
  if (s_rlLimit > 0) {
    double now = ns3::Simulator::Now().GetSeconds();
    RateWin& w = s_win[ingress.face.getId()];
    if (now - w.start >= 1.0) {
      w.start = now;
      w.n = 0;
    }
    if (++w.n > s_rlLimit) {
      ++s_rlDrops;
      this->rejectPendingInterest(pitEntry);  // descarta antes de ocupar a PIT
      return;
    }
  }

  if (hasPendingOutRecords(*pitEntry)) {
    return;
  }

  auto t0 = std::chrono::steady_clock::now();   // M3: inicio do trecho medido
  // A4: face suspeita -> copia com ForwardingHint -> face de Q
  if (s_quarantineFace != nullptr && s_suspects.count(ingress.face.getId()) > 0) {
    if (s_qLimit > 0) {   // Exp. 2: limite por face sobre o trafego desviado
      double now = ns3::Simulator::Now().GetSeconds();
      RateWin& w = s_qWin[ingress.face.getId()];
      if (now - w.start >= 1.0) { w.start = now; w.n = 0; }
      if (++w.n > s_qLimit) {
        ++s_qDrops;
        this->rejectPendingInterest(pitEntry);
        return;
      }
    }
    auto copy = std::make_shared<Interest>(interest);
    ::ndn::DelegationList hint;
    hint.insert(1, Name("/quarentena"));
    copy->setForwardingHint(hint);
    {
      auto t1 = std::chrono::steady_clock::now();
      s_nsHint += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
      ++s_nHint;
      s_bytesHint += copy->wireEncode().size();   // M2 (fora do trecho medido)
    }
    ++s_hintSent;
    this->sendInterest(pitEntry, FaceEndpoint(*s_quarantineFace, 0), *copy);
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  for (const auto& nh : fibEntry.getNextHops()) {
    if (!canForwardTo(ingress.face, pitEntry, nh)) {
      continue;
    }
    {
      auto t1 = std::chrono::steady_clock::now();
      s_nsNormal += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
      ++s_nNormal;
      s_bytesNormal += interest.wireEncode().size();
    }
    ++s_normalSent;
    this->sendInterest(pitEntry, FaceEndpoint(nh.getFace(), 0), interest);
    return;
  }
  this->rejectPendingInterest(pitEntry);
}

const Name&
QuarantineStrategy::getStrategyName()
{
  static Name strategyName("ndn:/localhost/nfd/strategy/quarantine/%FD%01");
  return strategyName;
}

} // namespace fw
} // namespace nfd
