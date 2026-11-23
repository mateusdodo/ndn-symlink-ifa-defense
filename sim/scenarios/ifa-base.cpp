
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ndnSIM/apps/ndn-producer.hpp"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/forwarder.hpp"

#include <fstream>
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
    ++s_received;
  }

  static uint64_t s_received;
};

uint64_t BlackholeProducer::s_received = 0;

NS_OBJECT_ENSURE_REGISTERED(BlackholeProducer);

}

static std::map<std::string, double> g_sentTime;
static std::set<std::string> g_received;
static std::map<int, uint64_t> g_sent, g_recvDl, g_recvAny;
static std::map<int, double> g_delaySum;
static double g_deadline = 1.0;

static std::ofstream g_faces;
static Ptr<Node> g_r1;

static void OnUserTx(std::shared_ptr<const ndn::Interest> i,
                     Ptr<ndn::App>,
                     std::shared_ptr<ndn::Face>) {
  std::string n = i->getName().toUri();

  if (g_sentTime.count(n)) return;

  double t = Simulator::Now().GetSeconds();

  g_sentTime[n] = t;
  g_sent[(int)t]++;
}

static void OnUserRx(std::shared_ptr<const ndn::Data> d,
                     Ptr<ndn::App>,
                     std::shared_ptr<ndn::Face>) {
  std::string n = d->getName().toUri();

  auto it = g_sentTime.find(n);

  if (it == g_sentTime.end() || g_received.count(n)) return;

  g_received.insert(n);

  double delay = Simulator::Now().GetSeconds() - it->second;
  int bin = (int)it->second;

  g_recvAny[bin]++;
  g_delaySum[bin] += delay;

  if (delay <= g_deadline) {
    g_recvDl[bin]++;
  }
}

static void SampleFaces(double period) {
  auto l3 = g_r1->GetObject<ndn::L3Protocol>();
  auto fw = l3->getForwarder();

  double t = Simulator::Now().GetSeconds();

  for (auto& f : l3->getFaceTable()) {
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
  double rho = 0.0;
  double duration = 30.0;
  double atkStart = 10.0;
  double atkStop = 25.0;
  double userHz = 10.0;

  uint32_t interestBytes = 32;
  uint32_t payload = 32;

  std::string bw = "50Kbps";
  std::string outDir = ".";
  std::string commit = "unknown";

  CommandLine cmd;

  cmd.AddValue("rho", "carga do ataque / capacidade do gargalo (0 = A0)", rho);
  cmd.AddValue("bw", "capacidade do enlace R1-produtor", bw);
  cmd.AddValue("interestBytes", "tamanho medio do Interest (para calcular a taxa)", interestBytes);
  cmd.AddValue("payload", "tamanho do Data do produtor (bytes)", payload);
  cmd.AddValue("userHz", "taxa media do usuario legitimo (Poisson)", userHz);
  cmd.AddValue("duration", "duracao (s)", duration);
  cmd.AddValue("atkStart", "inicio do ataque (s)", atkStart);
  cmd.AddValue("atkStop", "fim do ataque (s)", atkStop);
  cmd.AddValue("outDir", "diretorio de saida (precisa existir)", outDir);
  cmd.AddValue("commit", "commit git do codigo (gravado no meta)", commit);
  cmd.AddValue("deadline", "prazo (s) para contar o Interest como atendido", g_deadline);

  cmd.Parse(argc, argv);

  double capacityHz = DataRate(bw).GetBitRate() / 8.0 / interestBytes;
  double attackHz = rho * capacityHz;

  NodeContainer nodes;
  nodes.Create(5);

  Ptr<Node> user = nodes.Get(0);
  Ptr<Node> att = nodes.Get(1);
  Ptr<Node> r1 = nodes.Get(2);
  Ptr<Node> prod = nodes.Get(3);
  Ptr<Node> q = nodes.Get(4);

  g_r1 = r1;

  PointToPointHelper fast, bottleneck;

  fast.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  fast.SetChannelAttribute("Delay", StringValue("1ms"));

  bottleneck.SetDeviceAttribute("DataRate", StringValue(bw));
  bottleneck.SetChannelAttribute("Delay", StringValue("1ms"));

  fast.Install(user, r1);
  fast.Install(att, r1);
  bottleneck.Install(r1, prod);
  fast.Install(r1, q);

  ndn::StackHelper ndnHelper;

  ndnHelper.SetDefaultRoutes(false);
  ndnHelper.setCsSize(0);
  ndnHelper.InstallAll();

  ndn::GlobalRoutingHelper grh;

  grh.InstallAll();

  ndn::AppHelper pApp("ns3::ndn::Producer");

  pApp.SetPrefix("/data");
  pApp.SetAttribute("PayloadSize", UintegerValue(payload));
  pApp.Install(prod);

  ndn::AppHelper bApp("ns3::ndn::BlackholeProducer");

  bApp.SetPrefix("/fake");
  bApp.Install(prod);

  grh.AddOrigins("/data", prod);
  grh.AddOrigins("/fake", prod);
  grh.CalculateRoutes();

  ndn::AppHelper uApp("ns3::ndn::ConsumerCbr");

  uApp.SetPrefix("/data");
  uApp.SetAttribute("Frequency", DoubleValue(userHz));
  uApp.SetAttribute("Randomize", StringValue("exponential"));

  ApplicationContainer uc = uApp.Install(user);

  uc.Start(Seconds(0.5));
  uc.Stop(Seconds(duration));

  uc.Get(0)->TraceConnectWithoutContext(
      "TransmittedInterests",
      MakeCallback(&OnUserTx));

  uc.Get(0)->TraceConnectWithoutContext(
      "ReceivedDatas",
      MakeCallback(&OnUserRx));

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

  g_faces.open(outDir + "/faces.csv");
  g_faces << std::unitbuf;
  g_faces << "t,face,inI,outI,inD,outD,inN,outN,pit\n";

  Simulator::Schedule(Seconds(0.1), &SampleFaces, 0.1);

  Simulator::Stop(Seconds(duration));

  Simulator::Run();
  Simulator::Destroy();

  g_faces.close();

  std::ofstream isr(outDir + "/isr.csv");

  isr << "sec,sent,recvDl,recvAny,delaySum\n";

  for (int s = 0; s <= (int)duration; ++s) {
    isr << s << "," << g_sent[s] << "," << g_recvDl[s] << ","
        << g_recvAny[s] << "," << g_delaySum[s] << "\n";
  }

  std::ofstream meta(outDir + "/meta.txt");

  meta << "rho=" << rho << "\n"
       << "attackHz=" << attackHz << "\n"
       << "capacityHz=" << capacityHz
       << "\nbw=" << bw
       << "\nuserHz=" << userHz
       << "\nduration=" << duration
       << "\natkStart=" << atkStart
       << "\natkStop=" << atkStop
       << "\ndeadline=" << g_deadline
       << "\ncommit=" << commit
       << "\nblackhole_received="
       << ndn::BlackholeProducer::s_received << "\n";

  return 0;
}

}

int main(int argc, char* argv[]) {
  return ns3::run(argc, argv);
}
