/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

using namespace ns3;

/**
 * Cenário: Interest Flooding Attack (IFA) - VERSÃO NUCLEAR
 * Objetivo: Forçar o descarte de pacotes legítimos.
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

  // 2. Configurando os Canais (GARGALO EXTREMO)
  PointToPointHelper p2p;
  // 50Kbps é muito pouco. Qualquer sopro derruba a rede.
  p2p.SetDeviceAttribute("DataRate", StringValue("50Kbps")); 
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  // Limitamos a fila do roteador para estourar rápido
  p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("10p"));

  p2p.Install(nodes.Get(0), nodes.Get(2)); // User -> Router
  p2p.Install(nodes.Get(1), nodes.Get(2)); // Attacker -> Router
  p2p.Install(nodes.Get(2), nodes.Get(3)); // Router -> Producer

  // 3. Pilha NDN
  ns3::ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  // CS minúscula
  ndnHelper.setCsSize(10); 
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
  ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix("/ufba/dados");
  consumerHelper.SetAttribute("Frequency", StringValue("2.0")); 
  consumerHelper.Install(nodes.Get(0));

  // --- ATACANTE ---
  // 3000 pacotes por segundo num link de 50Kbps. 
  // Isso deve matar a comunicação.
  ns3::ndn::AppHelper attackerHelper("ns3::ndn::ConsumerCbr");
  attackerHelper.SetPrefix("/ufba/lixo"); 
  attackerHelper.SetAttribute("Frequency", StringValue("200.0")); 
  attackerHelper.SetAttribute("Randomize", StringValue("uniform"));
  attackerHelper.Install(nodes.Get(1));

  // 7. Trace
  ns3::ndn::L3RateTracer::InstallAll("rate-trace-ataque.txt", Seconds(1.0));

  Simulator::Stop(Seconds(10.0)); 

  std::cout << "=== Simulação IFA NUCLEAR ===" << std::endl;
  std::cout << "Link: 50Kbps | User: 2/s | Attacker: 200/s" << std::endl;
  
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
