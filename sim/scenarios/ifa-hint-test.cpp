#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ndnSIM/apps/ndn-producer.hpp"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"

#include "quarantine-strategy.hpp"

#include <cstdio>

namespace ns3 {
namespace ndn {

class ProdCounter : public Producer {
public:
  static TypeId GetTypeId() {
    static TypeId tid = TypeId("ns3::ndn::ProdCounter")
                          .SetGroupName("Ndn")
                          .SetParent<Producer>()
                          .AddConstructor<ProdCounter>();
    return tid;
  }
  virtual void OnInterest(std::shared_ptr<const Interest> interest) override {
    Producer::OnInterest(interest);
    ++s_n;
  }
  static uint64_t s_n;
};
uint64_t ProdCounter::s_n = 0;
NS_OBJECT_ENSURE_REGISTERED(ProdCounter);

class ReplicaCounter : public Producer {
public:
  static TypeId GetTypeId() {
    static TypeId tid = TypeId("ns3::ndn::ReplicaCounter")
                          .SetGroupName("Ndn")
                          .SetParent<Producer>()
                          .AddConstructor<ReplicaCounter>();
    return tid;
  }
  virtual void OnInterest(std::shared_ptr<const Interest> interest) override {
    if (!interest->getForwardingHint().empty()) ++s_withHint;
    Producer::OnInterest(interest);
    ++s_n;
  }
  static uint64_t s_n, s_withHint;
};
uint64_t ReplicaCounter::s_n = 0;
uint64_t ReplicaCounter::s_withHint = 0;
NS_OBJECT_ENSURE_REGISTERED(ReplicaCounter);

}

static uint64_t g_tx[2] = {0, 0}, g_rx[2] = {0, 0};

static void OnTx(int i, std::shared_ptr<const ndn::Interest>, Ptr<ndn::App>, std::shared_ptr<ndn::Face>) {
  ++g_tx[i];
}
static void OnRx(int i, std::shared_ptr<const ndn::Data>, Ptr<ndn::App>, std::shared_ptr<ndn::Face>) {
  ++g_rx[i];
}

int run(int argc, char* argv[]) {
  int suspect = 1;
  double duration = 10.0, userHz = 10.0;
  CommandLine cmd;
  cmd.AddValue("suspect", "1 = face de A marcada como suspeita; 0 = controle", suspect);
  cmd.AddValue("duration", "duracao (s)", duration);
  cmd.AddValue("userHz", "taxa de cada usuario", userHz);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);
  Ptr<Node> A = nodes.Get(0), B = nodes.Get(1), r1 = nodes.Get(2), prod = nodes.Get(3), q = nodes.Get(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer dA = p2p.Install(A, r1);
  NetDeviceContainer dB = p2p.Install(B, r1);
  NetDeviceContainer dP = p2p.Install(r1, prod);
  NetDeviceContainer dQ = p2p.Install(r1, q);

  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(false);
  ndnHelper.setCsSize(0);
  ndnHelper.InstallAll();

  ndn::StrategyChoiceHelper::Install<nfd::fw::QuarantineStrategy>(r1, "/data");

  ndn::FibHelper::AddRoute(A, "/data", r1, 1);
  ndn::FibHelper::AddRoute(B, "/data", r1, 1);
  ndn::FibHelper::AddRoute(r1, "/data", prod, 1);

  NodeContainer qc;
  qc.Add(q);
  ndn::NetworkRegionTableHelper::AddRegionName(qc, ndn::Name("/quarentena"));

  auto l3 = r1->GetObject<ndn::L3Protocol>();
  auto faceA = l3->getFaceByNetDevice(dA.Get(1));
  auto faceQ = l3->getFaceByNetDevice(dQ.Get(0));
  if (suspect) nfd::fw::QuarantineStrategy::s_suspects.insert(faceA->getId());
  nfd::fw::QuarantineStrategy::s_quarantineFace = faceQ.get();

  ndn::AppHelper pApp("ns3::ndn::ProdCounter");
  pApp.SetPrefix("/data");
  pApp.Install(prod);
  ndn::AppHelper rApp("ns3::ndn::ReplicaCounter");
  rApp.SetPrefix("/data");
  rApp.Install(q);

  Ptr<Node> users[2] = {A, B};
  const char* prefix[2] = {"/data/a", "/data/b"};
  for (int i = 0; i < 2; ++i) {
    ndn::AppHelper c("ns3::ndn::ConsumerCbr");
    c.SetPrefix(prefix[i]);
    c.SetAttribute("Frequency", DoubleValue(userHz));
    c.SetAttribute("Randomize", StringValue("exponential"));
    ApplicationContainer ac = c.Install(users[i]);
    ac.Start(Seconds(0.5));
    ac.Stop(Seconds(duration - 1));
    ac.Get(0)->TraceConnectWithoutContext("TransmittedInterests", MakeBoundCallback(&OnTx, i));
    ac.Get(0)->TraceConnectWithoutContext("ReceivedDatas", MakeBoundCallback(&OnRx, i));
  }

  Simulator::Stop(Seconds(duration));
  Simulator::Run();
  Simulator::Destroy();

  std::printf("suspect=%d faceA=%llu faceQ=%llu\n", suspect,
              (unsigned long long)faceA->getId(), (unsigned long long)faceQ->getId());
  std::printf("A: tx=%llu rx=%llu | B: tx=%llu rx=%llu\n",
              (unsigned long long)g_tx[0], (unsigned long long)g_rx[0],
              (unsigned long long)g_tx[1], (unsigned long long)g_rx[1]);
  std::printf("estrategia: enviados_com_hint=%llu enviados_normais=%llu\n",
              (unsigned long long)nfd::fw::QuarantineStrategy::s_hintSent,
              (unsigned long long)nfd::fw::QuarantineStrategy::s_normalSent);
  std::printf("produtor_recebeu=%llu replica_recebeu=%llu replica_com_hint_residual=%llu\n",
              (unsigned long long)ndn::ProdCounter::s_n,
              (unsigned long long)ndn::ReplicaCounter::s_n,
              (unsigned long long)ndn::ReplicaCounter::s_withHint);
  return 0;
}

}

int main(int argc, char* argv[]) {
  return ns3::run(argc, argv);
}
