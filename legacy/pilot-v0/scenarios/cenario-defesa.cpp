/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

using namespace ns3;

/**
 * Cenário: DEFESA (Rate Limiting)
 * Condições idênticas ao Ataque Nuclear (Link 50Kbps),
 * mas simulamos que o filtro de taxa entrou em ação.
 */

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 25);
  
  // 1. Criando os Nós
  NodeContainer nodes;
  nodes.Create(4); 
  Names::Add("User", nodes.Get(0));
  Names::Add("Attacker", nodes.Get(1));
  Names::Add("Router", nodes.Get(2));
  Names::Add("Producer", nodes.Get(3));

  // 2. Configurando os Canais (O MESMO GARGALO DE 50Kbps)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("50Kbps")); 
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("10p"));

  p2p.Install(nodes.Get(0), nodes.Get(2)); 
  p2p.Install(nodes.Get(1), nodes.Get(2)); 
  p2p.Install(nodes.Get(2), nodes.Get(3)); 

  // 3. Pilha NDN
  ns3::ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.setCsSize(10); // Mesma CS pequena
  ndnHelper.Install(nodes);

  // 4. Estratégia
  ns3::ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/best-route");

  // 5. Roteamento Global
  ns3::ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);
  ndnGlobalRoutingHelper.AddOrigins("/ufba", nodes.Get(3));
  ns3::ndn::GlobalRoutingHelper::CalculateRoutes();

  // 6. Aplicações

  // --- PRODUTOR ---
  ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix("/ufba/dados");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(3));

  // --- USUÁRIO LEGÍTIMO ---
  // Tenta acessar normalmente
  ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix("/ufba/dados");
  consumerHelper.SetAttribute("Frequency", StringValue("2.0")); 
  consumerHelper.Install(nodes.Get(0));

  // --- ATACANTE (SOB EFEITO DA DEFESA) ---
  // O filtro Rate Limiting cortou o tráfego dele.
  // De 3000/s caiu para 10/s.
  ns3::ndn::AppHelper attackerHelper("ns3::ndn::ConsumerCbr");
  attackerHelper.SetPrefix("/ufba/lixo"); 
  attackerHelper.SetAttribute("Frequency", StringValue("10.0")); // SIMULANDO O LIMITE
  attackerHelper.SetAttribute("Randomize", StringValue("uniform"));
  attackerHelper.Install(nodes.Get(1));

  // 7. Trace (Nome do arquivo diferente!)
  ns3::ndn::L3RateTracer::InstallAll("rate-trace-defesa.txt", Seconds(1.0));

  Simulator::Stop(Seconds(10.0));

  std::cout << "=== Simulação DEFESA ATIVA ===" << std::endl;
  std::cout << "Link: 50Kbps | User: 2/s | Attacker: Limitado a 10/s" << std::endl;
  
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
