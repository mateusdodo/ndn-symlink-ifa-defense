// Bracos: --arm=0 (A0 sem ataque) 1 (A1 sem defesa) 2 (A2 rate-limit) 4 (A4 oraculo + desvio)
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ndnSIM/apps/ndn-producer.hpp"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/forwarder.hpp"

#include "quarantine-strategy.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <vector>
#include <map>
#include <set>

namespace ns3 {
namespace ndn {

class BlackholeProducer : public Producer {
public:
  static TypeId GetTypeId() {
    static TypeId tid = TypeId("ns3::ndn::BlackholeProducer")
                          .SetGroupName("Ndn")
                          .SetParent<Producer>()
                          .AddConstructor<BlackholeProducer>();
    return tid;
  }
  virtual void OnInterest(std::shared_ptr<const Interest> interest) override {
    App::OnInterest(interest);
    ++s_received[GetNode()->GetId()];
  }
  static std::map<uint32_t, uint64_t> s_received;
};
std::map<uint32_t, uint64_t> BlackholeProducer::s_received;
NS_OBJECT_ENSURE_REGISTERED(BlackholeProducer);

}

static std::map<std::string, double> g_sentTime;
static std::set<std::string> g_received;
static std::map<int, uint64_t> g_sent, g_recvDl, g_recvAny;
static std::map<int, double> g_delaySum;
static double g_deadline = 1.0;
static std::ofstream g_faces;
static Ptr<Node> g_r1;

// M1: resumo da FIB (numero de entradas : hash de prefixo|face:custo)
static std::string FibSnapshot(Ptr<Node> n) {
  auto fw = n->GetObject<ndn::L3Protocol>()->getForwarder();
  std::vector<std::string> v;
  for (const auto& e : fw->getFib()) {
    std::string s = e.getPrefix().toUri();
    for (const auto& nh : e.getNextHops())
      s += "|" + std::to_string(nh.getFace().getId()) + ":" + std::to_string(nh.getCost());
    v.push_back(s);
  }
  std::sort(v.begin(), v.end());
  std::string all;
  for (const auto& s : v) all += s + "\n";
  return std::to_string(v.size()) + ":" + std::to_string(std::hash<std::string>()(all));
}

static void MarkSuspect(nfd::FaceId id) {
  nfd::fw::QuarantineStrategy::s_suspects.insert(id);
}

static void OnUserTx(std::shared_ptr<const ndn::Interest> i, Ptr<ndn::App>, std::shared_ptr<ndn::Face>) {
  std::string n = i->getName().toUri();
  if (g_sentTime.count(n)) return;
  double t = Simulator::Now().GetSeconds();
  g_sentTime[n] = t;
  g_sent[(int)t]++;
}

static void OnUserRx(std::shared_ptr<const ndn::Data> d, Ptr<ndn::App>, std::shared_ptr<ndn::Face>) {
  std::string n = d->getName().toUri();
  auto it = g_sentTime.find(n);
  if (it == g_sentTime.end() || g_received.count(n)) return;
  g_received.insert(n);
  double delay = Simulator::Now().GetSeconds() - it->second;
  int bin = (int)it->second;
  g_recvAny[bin]++;
  g_delaySum[bin] += delay;
  if (delay <= g_deadline) g_recvDl[bin]++;
}

static void SampleFaces(double period) {
  auto l3 = g_r1->GetObject<ndn::L3Protocol>();
  auto fw = l3->getForwarder();
  double t = Simulator::Now().GetSeconds();
  for (const auto& f : l3->getFaceTable()) {
    const auto& c = f.getCounters();
    g_faces << t << "," << f.getId() << ","
            << static_cast<uint64_t>(c.nInInterests) << ","
            << static_cast<uint64_t>(c.nOutInterests) << ","
            << static_cast<uint64_t>(c.nInData) << ","
            << static_cast<uint64_t>(c.nOutData) << ","
            << static_cast<uint64_t>(c.nInNacks) << ","
            << static_cast<uint64_t>(c.nOutNacks) << ","
            << fw->getPit().size() << "\n";
  }
  Simulator::Schedule(Seconds(period), &SampleFaces, period);
}

int run(int argc, char* argv[]) {
  int arm = 1;
  double rho = 2.0, duration = 30.0, atkStart = 10.0, atkStop = 25.0, userHz = 20.0, rlFrac = 0.5;
  double fcPeak = 0.0, fcStart = 10.0, fcStop = 25.0, markAt = 10.0, qFrac = 0.0;
  uint32_t interestBytes = 32, payload = 32;
  uint32_t fibN = 0, suspN = 0;
  std::string bw = "50Kbps", qbw = "50Kbps", outDir = ".", commit = "unknown";

  CommandLine cmd;
  cmd.AddValue("arm", "0=A0 1=A1 2=A2 4=A4 6=A6", arm);
  cmd.AddValue("fcPeak", "taxa total do usuario no flash crowd (Hz); 0 = sem flash crowd", fcPeak);
  cmd.AddValue("fcStart", "inicio do flash crowd (s)", fcStart);
  cmd.AddValue("fcStop", "fim do flash crowd (s)", fcStop);
  cmd.AddValue("qFrac", "Exp.2: limite por face sobre trafego desviado = qFrac x capacidade (0 = off)", qFrac);
  cmd.AddValue("markAt", "A6: instante em que a face do usuario e marcada (falso positivo)", markAt);
  cmd.AddValue("rho", "carga do ataque / capacidade do gargalo", rho);
  cmd.AddValue("rlFrac", "A2: limite por face = rlFrac x capacidade (Interests/s)", rlFrac);
  cmd.AddValue("bw", "capacidade do enlace R1-produtor", bw);
  cmd.AddValue("qbw", "capacidade do enlace R1-Q", qbw);
  cmd.AddValue("interestBytes", "tamanho medio do Interest", interestBytes);
  cmd.AddValue("payload", "tamanho do Data (bytes)", payload);
  cmd.AddValue("userHz", "taxa do usuario legitimo (Poisson)", userHz);
  cmd.AddValue("duration", "duracao (s)", duration);
  cmd.AddValue("atkStart", "inicio do ataque (s)", atkStart);
  cmd.AddValue("atkStop", "fim do ataque (s)", atkStop);
  cmd.AddValue("deadline", "prazo (s) do ISR", g_deadline);
  cmd.AddValue("outDir", "diretorio de saida (precisa existir)", outDir);
  cmd.AddValue("commit", "commit git", commit);
  cmd.AddValue("fibN", "Exp.4: entradas falsas na FIB de R1", fibN);
  cmd.AddValue("suspN", "Exp.4: faces suspeitas falsas no conjunto da estrategia", suspN);
  cmd.Parse(argc, argv);

  if (arm == 0) rho = 0;
  double capacityHz = DataRate(bw).GetBitRate() / 8.0 / interestBytes;
  double attackHz = rho * capacityHz;

  NodeContainer nodes;
  nodes.Create(5);
  Ptr<Node> user = nodes.Get(0), att = nodes.Get(1), r1 = nodes.Get(2), prod = nodes.Get(3), q = nodes.Get(4);
  g_r1 = r1;

  PointToPointHelper fast, bottleneck, qlink;
  fast.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  fast.SetChannelAttribute("Delay", StringValue("1ms"));
  bottleneck.SetDeviceAttribute("DataRate", StringValue(bw));
  bottleneck.SetChannelAttribute("Delay", StringValue("1ms"));
  qlink.SetDeviceAttribute("DataRate", StringValue(qbw));
  qlink.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer dUser = fast.Install(user, r1);   // Get(1) esta em R1
  NetDeviceContainer dAtt = fast.Install(att, r1);   // Get(1) esta em R1
  bottleneck.Install(r1, prod);
  NetDeviceContainer dQ = qlink.Install(r1, q);      // Get(0) esta em R1

  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(false);
  ndnHelper.setCsSize(0);
  ndnHelper.InstallAll();

  ndn::StrategyChoiceHelper::Install<nfd::fw::QuarantineStrategy>(r1, "/data");
  ndn::StrategyChoiceHelper::Install<nfd::fw::QuarantineStrategy>(r1, "/fake");

  ndn::FibHelper::AddRoute(user, "/data", r1, 1);
  ndn::FibHelper::AddRoute(att, "/fake", r1, 1);
  ndn::FibHelper::AddRoute(r1, "/data", prod, 1);
  ndn::FibHelper::AddRoute(r1, "/fake", prod, 1);

  NodeContainer qc;
  qc.Add(q);
  ndn::NetworkRegionTableHelper::AddRegionName(qc, ndn::Name("/quarentena"));

  auto l3 = r1->GetObject<ndn::L3Protocol>();
  auto faceAtt = l3->getFaceByNetDevice(dAtt.Get(1));
  auto faceQ = l3->getFaceByNetDevice(dQ.Get(0));
  auto faceUser = l3->getFaceByNetDevice(dUser.Get(1));
  using nfd::fw::QuarantineStrategy;
  QuarantineStrategy::s_quarantineFace = faceQ.get();
  if (arm == 4) QuarantineStrategy::s_suspects.insert(faceAtt->getId());
  if (arm == 6) {   // A6: face do usuario marcada por engano; atacante (se houver) tambem desviado
    if (rho > 0) QuarantineStrategy::s_suspects.insert(faceAtt->getId());
    Simulator::Schedule(Seconds(markAt), &MarkSuspect, faceUser->getId());
  }
  if (arm == 2) QuarantineStrategy::s_rlLimit = rlFrac * capacityHz;
  QuarantineStrategy::s_qLimit = qFrac * capacityHz;
  for (uint32_t i = 0; i < fibN; ++i)
    ndn::FibHelper::AddRoute(r1, ndn::Name("/dummy/" + std::to_string(i)), prod, 1);
  for (uint32_t i = 0; i < suspN; ++i)
    QuarantineStrategy::s_suspects.insert(100000 + i);

  ndn::AppHelper pApp("ns3::ndn::Producer");
  pApp.SetPrefix("/data");
  pApp.SetAttribute("PayloadSize", UintegerValue(payload));
  pApp.Install(prod);
  pApp.Install(q);                                   // replica do conteudo em Q
  ndn::AppHelper bApp("ns3::ndn::BlackholeProducer");
  bApp.SetPrefix("/fake");
  bApp.Install(prod);
  bApp.Install(q);                                   // Q tambem nao responde /fake

  ndn::AppHelper uApp("ns3::ndn::ConsumerCbr");
  uApp.SetPrefix("/data");
  uApp.SetAttribute("Frequency", DoubleValue(userHz));
  uApp.SetAttribute("Randomize", StringValue("exponential"));
  ApplicationContainer uc = uApp.Install(user);
  uc.Start(Seconds(0.5));
  uc.Stop(Seconds(duration));
  uc.Get(0)->TraceConnectWithoutContext("TransmittedInterests", MakeCallback(&OnUserTx));
  uc.Get(0)->TraceConnectWithoutContext("ReceivedDatas", MakeCallback(&OnUserRx));

  if (rho > 0) {
    ndn::AppHelper aApp("ns3::ndn::ConsumerCbr");
    aApp.SetPrefix("/fake");
    aApp.SetAttribute("Frequency", DoubleValue(attackHz));
    aApp.SetAttribute("Randomize", StringValue("exponential"));
    aApp.SetAttribute("RetxTimer", StringValue("1000s"));
    ApplicationContainer ac = aApp.Install(att);
    ac.Start(Seconds(atkStart));
    ac.Stop(Seconds(atkStop));
  }

  if (fcPeak > userHz) {   // flash crowd: trafego legitimo extra, prefixo proprio
    ndn::AppHelper fApp("ns3::ndn::ConsumerCbr");
    fApp.SetPrefix("/data/fc");
    fApp.SetAttribute("Frequency", DoubleValue(fcPeak - userHz));
    fApp.SetAttribute("Randomize", StringValue("exponential"));
    ApplicationContainer fc = fApp.Install(user);
    fc.Start(Seconds(fcStart));
    fc.Stop(Seconds(fcStop));
    fc.Get(0)->TraceConnectWithoutContext("TransmittedInterests", MakeCallback(&OnUserTx));
    fc.Get(0)->TraceConnectWithoutContext("ReceivedDatas", MakeCallback(&OnUserRx));
  }

  std::string fibBefore = FibSnapshot(r1);
  g_faces.open(outDir + "/faces.csv");
  g_faces << std::unitbuf;
  g_faces << "t,face,inI,outI,inD,outD,inN,outN,pit\n";
  Simulator::Schedule(Seconds(0.1), &SampleFaces, 0.1);

  Simulator::Stop(Seconds(duration));
  Simulator::Run();
  std::string fibAfter = FibSnapshot(r1);
  Simulator::Destroy();
  g_faces.close();

  std::ofstream isr(outDir + "/isr.csv");
  isr << "sec,sent,recvDl,recvAny,delaySum\n";
  for (int s = 0; s <= (int)duration; ++s) {
    isr << s << "," << g_sent[s] << "," << g_recvDl[s] << "," << g_recvAny[s] << "," << g_delaySum[s] << "\n";
  }

  std::ofstream meta(outDir + "/meta.txt");
  meta << "arm=" << arm << "\nrho=" << rho << "\nattackHz=" << attackHz
       << "\ncapacityHz=" << capacityHz << "\nrlFrac=" << rlFrac
       << "\nrlLimit=" << QuarantineStrategy::s_rlLimit
       << "\nrlDrops=" << QuarantineStrategy::s_rlDrops
       << "\nhintSent=" << QuarantineStrategy::s_hintSent
       << "\nnormalSent=" << QuarantineStrategy::s_normalSent
       << "\nblackhole_prod=" << ndn::BlackholeProducer::s_received[prod->GetId()]
       << "\nblackhole_q=" << ndn::BlackholeProducer::s_received[q->GetId()]
       << "\nfibN=" << fibN << "\nsuspN=" << suspN<< "\nfibBefore=" << fibBefore << "\nfibAfter=" << fibAfter<< "\nnsHint=" << QuarantineStrategy::s_nsHint << "\nnHint=" << QuarantineStrategy::s_nHint<< "\nnsNormal=" << QuarantineStrategy::s_nsNormal << "\nnNormal=" << QuarantineStrategy::s_nNormal<< "\nbytesHint=" << QuarantineStrategy::s_bytesHint << "\nbytesNormal=" << QuarantineStrategy::s_bytesNormal<< "\nqFrac=" << qFrac << "\nqDrops=" << QuarantineStrategy::s_qDrops << "\nfcPeak=" << fcPeak << "\nmarkAt=" << markAt << "\nbw=" << bw << "\nqbw=" << qbw << "\nuserHz=" << userHz
       << "\nduration=" << duration << "\natkStart=" << atkStart << "\natkStop=" << atkStop
       << "\ndeadline=" << g_deadline << "\ncommit=" << commit << "\n";
  return 0;
}

}

int main(int argc, char* argv[]) {
  return ns3::run(argc, argv);
}
