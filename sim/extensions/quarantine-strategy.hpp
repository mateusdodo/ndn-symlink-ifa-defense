#ifndef IFA_QUARANTINE_STRATEGY_HPP
#define IFA_QUARANTINE_STRATEGY_HPP

#include "ns3/ndnSIM/NFD/daemon/fw/strategy.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/algorithm.hpp"

#include <cstdint>
#include <set>

namespace nfd {
namespace fw {

class QuarantineStrategy : public Strategy {
public:
  QuarantineStrategy(Forwarder& forwarder, const Name& name = getStrategyName());

  virtual ~QuarantineStrategy() override;

  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  static const Name&
  getStrategyName();

  static std::set<FaceId> s_suspects;
  static Face* s_quarantineFace;
  static uint64_t s_hintSent;
  static uint64_t s_normalSent;
};

}
}

#endif
