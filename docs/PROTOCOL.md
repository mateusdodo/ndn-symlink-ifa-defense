# Protocolo experimental v1 (congelado em protocol-v1)

Status: rascunho até a tag protocol-v1. Depois da tag, qualquer mudança vira
"Desvio do protocolo" no fim deste arquivo, com data e motivo.

## 1. Perguntas de pesquisa
RQ1. Um detector leve por face identifica IFA com TPR e FPR aceitáveis em
     tráfego legítimo difícil (flash crowd, falha de enlace)?
RQ2. O desvio para um nó de quarentena preserva o ISR dos usuários legítimos
     durante o ataque, frente a nenhuma defesa e a rate-limit por face?
RQ3. Qual o custo de um falso positivo (ISR do tráfego legítimo desviado)?
RQ4. O detector generaliza para taxas de ataque e topologias não vistas no treino?

## 2. Hipóteses falsificáveis (limiares a preencher na decisão 1)
H1. ISR(A5) - ISR(A1) >= [X] pontos percentuais, IC95% exclui zero.
H2. ISR(A5) >= ISR(A2), Wilcoxon pareado por semente, alfa = 0,05 com Holm.
H3. FPR do detector <= [Y]% em tráfego legítimo (validação leave-scenario-out).
H4. ISR(A6) - ISR(A0) >= -[Z] p.p. (degradação graduada limitada).
Refutação: se qualquer IC95% incluir o limiar contrário, a hipótese é refutada
e reportada como tal.

## 3. Métricas
ISR (Interest Satisfaction Ratio) do usuário legítimo; ocupação da PIT;
TPR, FPR, F1 e tempo até detecção; custo de falso positivo.

## 4. Desenho
Topologia: usuário e atacante -> R1 -> produtor (gargalo); Q ligado a R1.
Carga do ataque em rho = carga/capacidade, rho em {0 (A0), 1, 2, 5}.
Duração: 30 s simulados; ataque de 10 s a 25 s.
Produtor ignora /fake para a PIT permanecer pendente.
Tráfego legítimo: chegadas exponenciais a 20 Hz; cenários: estável,
flash crowd, falha de enlace.

## 5. Sementes
Desenvolvimento: 1-10 (depuração e escolha de limiares do detector).
Confirmação: 101-130, executadas uma única vez, após a tag protocol-v1.

## 6. Estatística
IC95% por bootstrap; Wilcoxon pareado por semente; Holm para comparações
múltiplas; Cliff's delta como tamanho de efeito.

## 7. Detector
Random Forest; baselines: limiar simples em InInterests e regressão logística.
Rótulo pelo instante real do ataque. Validação leave-scenario-out e
GroupKFold por simulação. Ablação com e sem InNacks.

## 8. Ameaças à validade
Simulação (ndnSIM 2.8) vs rede real; topologia única; produtor sintético;
imagem de terceiros (mitigação: digest fixado).

## 9. Desvios do protocolo
(nenhum)

## Verificação do mecanismo (dev, 2026-11-23)
Teste de 3 nós (ifa-hint-test, userHz=10, 10 s): com a face de A suspeita, 91/91 Interests
de A foram a Q com ForwardingHint, 0 chegaram com hint residual, 91/91 Data retornaram;
B (83/83) foi ao produtor. Controle (suspect=0): 0 desvios. Estratégia: best-route
simplificado derivado de Strategy; Q ligado direto ao R1. Evidência de consumo do hint é
indireta (sem NS_LOG na imagem otimizada).

## Calibração de A2 (dev, sementes 1-3, ataque rho=2, userHz=20, deadline 1 s)
rlFrac 0.25 / 0.5: ISR(1s)=1.00; rlFrac 1.0: 0.92-0.95; rlFrac 2.0: ~0.49 (igual a A1).
Goodput no prazo: A0/A2(0.5)/A4 ~18-19.5 Data/s; A1 ~3.5-4.6 Data/s.
Contra atacante puro, A2 (rlFrac<=0.5) e A4 empatam no ISR; A4 tem menor atraso (0.022 s vs 0.12 s).

## Calibração do flash crowd (dev, A0 sem atacante, sementes 1-3, 2026-11-24)
Pico do usuário (Hz): 40 -> ISR 1.00 | 60 -> 1.00 | 80 -> 0.99 | 100 -> 0.60 | 150 -> 0.13-0.24.
O gargalo (50 Kbps) comporta ~85-95 Data/s: o A0 satura entre 80 e 100 Hz (Data maior que Interest).
GRADE CONGELADA (definida só com o A0, antes de qualquer resultado de A2/A6 sob flash crowd):
  picos {40, 60, 80} Hz; A2 com rlFrac {0.25, 0.5, 1.0}; A6 com markAt=10 s.
Picos de 100 e 150 Hz saturam o A0 e ficam fora da grade (sobrecarga, nao flash crowd).
Limitacao: uma face legitima nao passa de ~85 Interests/s, entao A2 com rlFrac>=0.5 (limite>=98/s)
nao deve cortar trafego legitimo nesta topologia; o custo de A2 so aparece com limite < pico.

## Correção (2026-11-24)
Os resultados de A2 e A6 em results/dev/fc/ foram obtidos com atacante (rho=2 e o padrao), nao
"sem atacante". Sao o cenario combinado (ataque + flash crowd). Achado: A6 com Q compartilhado
com o atacante tem ISR(1s)~0.28 e atraso ~5 s (falso positivo muito custoso); A2 so funciona em
rlFrac~0.5 (0.25 corta o legitimo, 1.0 deixa o ataque passar).

## EXPERIMENTO 2 (declarado em 2026-11-24, ANTES de qualquer execucao; exploratorio)
Motivacao: no Experimento 1, A6 com atacante dividindo Q (50 Kbps) teve ISR(1s)~0.28 e atraso ~5 s.
Justificativa independente do resultado: uma quarentena compartilhada sem isolamento nao e um
desenho realista de tratamento de falso positivo.
Mudanca: limite por face de entrada sobre Interests desviados para Q (--qFrac x capacidade, Interests/s).
Vale so para faces suspeitas. Nada muda para faces normais. qFrac=0 reproduz o Experimento 1.
Grade: qFrac {0.1, 0.2, 0.5}; picos {40, 60, 80} Hz; rho=2; sementes de desenvolvimento 1-3.
Hipoteses (inalteradas): custo do falso positivo limitado (ISR(A6)-ISR(A0) >= -Z p.p.), e A4/A5
mais robustos a calibracao que A2.
Previsoes: A6 com qFrac=0.2 recupera o ISR do usuario (>=0.9 nos tres picos); qFrac=0.5 ainda
satura Q (Interest com hint ~50 B -> Q comporta ~120/s); qFrac=0.1 protege Q mas pode cortar o
usuario no pico de 80 Hz se ele ficar acima do limite. Se isso nao ocorrer, reporta-se como achado.
O Experimento 1 e seus resultados permanecem no artigo.
