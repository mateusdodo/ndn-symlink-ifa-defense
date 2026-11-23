/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

using namespace ns3;

/**
 * Cenário: PROPOSTA (Redirecionamento Symlink Emulado)
 * Objetivo: Demonstrar que o redirecionamento do usuário legítimo para
 * um prefixo seguro/isolado restabelece o desempenho da rede.
 */

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

  // 3. Pilha NDN
  ns3::ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.setCsSize(10);
  ndnHelper.Install(nodes);

  // 4. Estratégia
  ns3::ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/best-route");

  // 5. Roteamento Global
  ns3::ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);
  
  // Rotas distintas para simular o redirecionamento
  ndnGlobalRoutingHelper.AddOrigins("/ufba/dados", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigins("/ufba/lixo", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigins("/ufba/dados_backup", nodes.Get(4)); // Rota do Symlink
  
  ns3::ndn::GlobalRoutingHelper::CalculateRoutes();

  // 6. Aplicações

  // --- PRODUTOR PRINCIPAL ---
  ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix("/ufc/dados");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(3));

  // --- PRODUTOR BACKUP (Servidor de Quarentena / Symlink) ---
  ns3::ndn::AppHelper producerBackupHelper("ns3::ndn::Producer");
  producerBackupHelper.SetPrefix("/ufc/dados_backup");
  producerBackupHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerBackupHelper.Install(nodes.Get(4));

  // --- USUÁRIO LEGÍTIMO (Fase 1: Sofrendo o Ataque) ---
  // Requisita o prefixo principal até o segundo 3.0
  ns3::ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerCbr");
  consumerHelper1.SetPrefix("/ufba/dados");
  consumerHelper1.SetAttribute("Frequency", StringValue("2.0")); 
  ApplicationContainer appLegitima1 = consumerHelper1.Install(nodes.Get(0));
  appLegitima1.Start(Seconds(0.0));
  appLegitima1.Stop(Seconds(3.0));

  // --- USUÁRIO LEGÍTIMO (Fase 2: Redirecionamento por Symlink) ---
  // No segundo 3.0, muda para o prefixo seguro até o fim da simulação
  ns3::ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerCbr");
  consumerHelper2.SetPrefix("/ufba/dados_backup");
  consumerHelper2.SetAttribute("Frequency", StringValue("2.0")); 
  ApplicationContainer appLegitima2 = consumerHelper2.Install(nodes.Get(0));
  appLegitima2.Start(Seconds(3.0));
  appLegitima2.Stop(Seconds(10.0));

  // --- ATACANTE (Ininterrupto) ---
  // Continua atacando o canal principal a 200.0/s durante todo o tempo
  ns3::ndn::AppHelper attackerHelper("ns3::ndn::ConsumerCbr");
  attackerHelper.SetPrefix("/ufba/lixo"); 
  attackerHelper.SetAttribute("Frequency", StringValue("200.0")); 
  attackerHelper.SetAttribute("Randomize", StringValue("uniform"));
  attackerHelper.Install(nodes.Get(1));

  // 7. Trace (Nome do arquivo exclusivo!)
  ns3::ndn::L3RateTracer::InstallAll("rate-trace-symlink.txt", Seconds(1.0));

  Simulator::Stop(Seconds(10.0)); 

  std::cout << "=== Simulação PROPOSTA SYMLINK ===" << std::endl;
  std::cout << "Legítimo: 0s-3s /ufba/dados | 3s-10s /ufba/dados_backup" << std::endl;
  std::cout << "Atacante persistente a 200/s no canal principal" << std::endl;
  
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
