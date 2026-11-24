#ifndef IFA_QUARANTINE_STRATEGY_HPP
#define IFA_QUARANTINE_STRATEGY_HPP

#include "ns3/ndnSIM/NFD/daemon/fw/strategy.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/algorithm.hpp"

#include <cstdint>
#include <map>
#include <set>

namespace nfd {
namespace fw {

// Configuravel pelo cenario (todos os bracos usam esta mesma estrategia):
//  - s_rlLimit > 0: rate-limit por face de entrada (Interests/s, janela fixa de 1 s) -> A2
//  - s_suspects: faces cujos Interests recebem ForwardingHint e vao a s_quarantineFace -> A4
class QuarantineStrategy : public Strategy {
public:
  QuarantineStrategy(Forwarder& forwarder, const Name& name = getStrategyName());
  virtual ~QuarantineStrategy() override;

  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  static const Name&
  getStrategyName();

  struct RateWin { double start = 0; uint64_t n = 0; };

  static std::set<FaceId> s_suspects;
  static Face* s_quarantineFace;
  static double s_rlLimit;
  static double s_qLimit;
  static std::map<FaceId, RateWin> s_qWin;
  static std::map<FaceId, RateWin> s_win;
  static uint64_t s_hintSent, s_normalSent, s_rlDrops, s_qDrops;
  static uint64_t s_nsHint, s_nHint, s_nsNormal, s_nNormal, s_bytesHint, s_bytesNormal;
};

} // namespace fw
} // namespace nfd

#endif
