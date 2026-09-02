# ndn-symlink-ifa-defense
A non-punitive IFA mitigation framework for NDN combining Random Forest telemetries and dynamic Link Objects

```markdown
# ndn-symlink-ifa-defense

Um framework não punitivo de mitigação de IFA para NDN combinando telemetria
Random Forest e Link Objects dinâmicos.

## Visão Geral

Este repositório apresenta um framework inteligente e não punitivo para detecção
e mitigação de Ataques de Inundação de Interest (IFA - *Interest Flooding Attacks*)
em Redes Centradas em Dados (NDN). Desenvolvida sobre o `ndnSIM 2.8`, a solução
integra telemetria de rede de alta precisão, um classificador baseado em Random Forest
e **Link Objects (Symlinks)** dinâmicos. 

Em vez de recorrer a limitação agressiva de taxa (*rate-limiting*) ou descarte de pacotes
— que correm o risco de penalizar usuários legítimos em casos de falsos positivos —, este
framework redireciona de forma transparente o tráfego de usuários legítimos para rotas
de backup isoladas com complexidade O(1) quando padrões de ataque são identificados.

## Principais Recursos

- **Redirecionamento Não Punitivo:** Utiliza NDN Link Objects para redirecionar consumidores
  legítimos para produtores de quarentena/backup (`/ufba/dados_backup`) sem descartar pacotes
  ou reindexar a FIB.
- **Telemetria Consciente do Estado:** Introduz a métrica `PIT_Leak = max(0, InInterests - OutData)`
  para rastrear a retenção de recursos da PIT e desequilíbrios de fluxo junto com `InNacks` nativos.
- **Classificação de Alta Precisão:** Utiliza um modelo Random Forest treinado em 35 condições
  de rede para detectar a saturação de IFA em intervalos de amostragem de 0,1 segundo.
- **Avaliação Reprodutível:** Fornece cenários completos de simulação em C++, conjuntos de
  dados de telemetria em múltiplas larguras de banda (50Kbps a 10Mbps) e frequências de
  ataque (0,1Hz a 5000Hz), além de scripts de execução automatizados.

```

## Metodologia e Arquitetura

O pipeline de mitigação opera em um ciclo contínuo de retroalimentação em duas etapas:

| Etapa | Componente | Função |
| --- | --- | --- |
| **1. Detecção** | `L3RateTracer` + Random Forest | Extrai `InInterests`, `OutData` e `InNacks` a cada 0,1s para calcular o `PIT_Leak`. O classificador identifica anomalias de fluxo com baixo overhead de CPU. |
| **2. Mitigação** | Link Objects (Symlinks) | Após a confirmação do ataque, o roteador altera os ponteiros de prefixo, redirecionando o tráfego legítimo do namespace alvo (`/ufba/dados`) para canais limpos (`/ufba/dados_backup`). |
