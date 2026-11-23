/* -*- Mode:C++; c-file-sty==le:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Valores padrão (Default)
  std::string bandwidth = "50Kbps";
  std::string attackFrequency = "200.0";

  // Permite alterar via terminal
  CommandLine cmd;
  cmd.AddValue("bandwidth", "Largura de banda dos canais", bandwidth);
  cmd.AddValue("attackFreq", "Frequencia de envio do atacante", attackFrequency);
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 25);
  NodeContainer nodes;
  nodes.Create(5);
  Names::Add("User", nodes.Get(0));
  Names::Add("Attacker", nodes.Get(1));
  Names::Add("Router", nodes.Get(2));
  Names::Add("Producer", nodes.Get(3));
  Names::Add("ProducerBackup", nodes.Get(4));

  // Configurando os canais de forma dinâmica
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth)); 
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("10p"));

  // Super Link para o atacante não se estrangular
  PointToPointHelper p2pAttacker;
  p2pAttacker.SetDeviceAttribute("DataRate", StringValue("100Mbps")); 
  p2pAttacker.SetChannelAttribute("Delay", StringValue("10ms"));

  p2p.Install(nodes.Get(0), nodes.Get(2)); // User -> Router
  p2pAttacker.Install(nodes.Get(1), nodes.Get(2)); // Attacker -> Router (SUPER LINK)
  p2p.Install(nodes.Get(2), nodes.Get(3)); // Router -> Produtor 1
  p2p.Install(nodes.Get(2), nodes.Get(4)); // Router -> Produtor Backup
  
  ns3::ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.setCsSize(10); 
  ndnHelper.Install(nodes);

  ns3::ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/best-route");

  ns3::ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);
  ndnGlobalRoutingHelper.AddOrigins("/ufc/dados", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigins("/ufc/lixo", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigins("/ufc/dados_backup", nodes.Get(4)); 
  ns3::ndn::GlobalRoutingHelper::CalculateRoutes();

  // --- PRODUTORES ---
  ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix("/ufc/dados");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(3));

  ns3::ndn::AppHelper producerBackupHelper("ns3::ndn::Producer");
  producerBackupHelper.SetPrefix("/ufc/dados_backup");
  producerBackupHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerBackupHelper.Install(nodes.Get(4));

  // --- USUÁRIO LEGÍTIMO ---
  // Fase 1: Normal (0 a 3s)
  ns3::ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerCbr");
  consumerHelper1.SetPrefix("/ufc/dados");
  consumerHelper1.SetAttribute("Frequency", StringValue("2.0")); 
  ApplicationContainer appLegitima1 = consumerHelper1.Install(nodes.Get(0));
  appLegitima1.Start(Seconds(0.0));
  appLegitima1.Stop(Seconds(6.0));

// Fase 2: Foge para o Symlink seguro (6s a 20s)
  ns3::ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerCbr");
  consumerHelper2.SetPrefix("/ufc/dados_backup");
  consumerHelper2.SetAttribute("Frequency", StringValue("2.0")); 
  ApplicationContainer appLegitima2 = consumerHelper2.Install(nodes.Get(0));
  appLegitima2.Start(Seconds(6.0));
  appLegitima2.Stop(Seconds(20.0));

// --- ATACANTE INTELIGENTE ---
  // Fase 1: Ataca a rota principal desde o início
  ns3::ndn::AppHelper attackerHelper1("ns3::ndn::ConsumerCbr");
  attackerHelper1.SetPrefix("/ufc/lixo"); // CORRIGIDO PARA UFC
  attackerHelper1.SetAttribute("Frequency", StringValue(attackFrequency));
  ApplicationContainer appAtacante1 = attackerHelper1.Install(nodes.Get(1));
  appAtacante1.Start(Seconds(0.0));
  appAtacante1.Stop(Seconds(20.0));

  // Fase 3: Atacante descobre o túnel e ataca o Symlink (12s a 20s)
  ns3::ndn::AppHelper attackerHelper2("ns3::ndn::ConsumerCbr");
  attackerHelper2.SetPrefix("/ufc/dados_backup"); // CORRIGIDO PARA UFC
  attackerHelper2.SetAttribute("Frequency", StringValue(attackFrequency));
  ApplicationContainer appAtacante2 = attackerHelper2.Install(nodes.Get(1));
  appAtacante2.Start(Seconds(12.0));
  appAtacante2.Stop(Seconds(20.0));

  // ALTERAÇÃO CRUCIAL: Tracer a cada 0.1s para gerar 10x mais dados!
  ns3::ndn::L3RateTracer::InstallAll("rate-trace-cifa.txt", Seconds(0.1));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();
}
