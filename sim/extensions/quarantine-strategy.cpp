#include "quarantine-strategy.hpp"
#include "ns3/simulator.h"

namespace nfd {
namespace fw {

std::set<FaceId> QuarantineStrategy::s_suspects;
Face* QuarantineStrategy::s_quarantineFace = nullptr;
double QuarantineStrategy::s_rlLimit = 0;
std::map<FaceId, QuarantineStrategy::RateWin> QuarantineStrategy::s_win;
uint64_t QuarantineStrategy::s_hintSent = 0;
uint64_t QuarantineStrategy::s_normalSent = 0;
uint64_t QuarantineStrategy::s_rlDrops = 0;

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

  // A4: face suspeita -> copia com ForwardingHint -> face de Q
  if (s_quarantineFace != nullptr && s_suspects.count(ingress.face.getId()) > 0) {
    auto copy = std::make_shared<Interest>(interest);
    ::ndn::DelegationList hint;
    hint.insert(1, Name("/quarentena"));
    copy->setForwardingHint(hint);
    ++s_hintSent;
    this->sendInterest(pitEntry, FaceEndpoint(*s_quarantineFace, 0), *copy);
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  for (const auto& nh : fibEntry.getNextHops()) {
    if (!canForwardTo(ingress.face, pitEntry, nh)) {
      continue;
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
