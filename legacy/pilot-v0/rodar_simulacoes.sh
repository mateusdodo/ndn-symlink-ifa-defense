#!/bin/bash

# Cores para deixar o terminal organizado e bonito
VERDE='\033[0;32m'
VERMELHO='\033[0;31m'
AZUL='\033[0;34m'
AMARELO='\033[1;33m'
NC='\033[0m' # Sem cor (Reset)

# Matriz de parâmetros
bandwidths=("50Kbps" "250Kbps" "1Mbps" "5Mbps" "10Mbps")
attack_freqs=("0.1" "10.0" "50.0" "200.0" "1000.0" "3000.0" "5000.0")

# Cálculos iniciais de progresso
total_runs=$(( ${#bandwidths[@]} * ${#attack_freqs[@]} ))
current_run=1

echo -e "${AZUL}============================================================${NC}"
echo -e "${AZUL}🧠 INICIANDO ENGINE DE SIMULAÇÃO NDN - IFA DATASET${NC}"
echo -e "${AZUL}   -> Total de cenários mapeados: $total_runs${NC}"
echo -e "${AZUL}============================================================${NC}"

# Marca o tempo total de início do lote
tempo_total_inicio=$(date +%s)

for bw in "${bandwidths[@]}"; do
    for freq in "${attack_freqs[@]}"; do
        # Calcula a porcentagem atual
        pct=$(( current_run * 100 / total_runs ))
        
        echo -e "\n${AMARELO}[$current_run/$total_runs] (${pct}%) ⏳ Executando: Banda=$bw | Freq. Ataque=$freq...${NC}"
        
        # Marca o início desta simulação específica
        tempo_inicio=$(date +%s)
        
        # Executa no Docker redirecionando a saída padrão e erros para o limbo (deixa a tela limpa)
        docker exec ndn-tutorial-e2 ./waf --run "cenario-cifa --bandwidth=$bw --attackFreq=$freq" > /dev/null 2>&1
        status_docker=$?
        
        tempo_fim=$(date +%s)
        duracao=$(( tempo_fim - tempo_inicio ))
        
        # Validação do sucesso da execução
        if [ $status_docker -eq 0 ] && [ -f rate-trace-cifa.txt ]; then
            nome_final="rate-trace-bw_${bw}-freq_${freq}.txt"
            mv rate-trace-cifa.txt "$nome_final"
            echo -e "    ${VERDE}[SUCESSO]${NC} Finalizado em ${duracao}s. Salvo como: $nome_final"
        else
            echo -e "    ${VERMELHO}[FALHA]${NC} A simulação falhou ou o arquivo não foi gerado! (Status do Docker: $status_docker)"
            echo -e "    Se desejar interromper o processo, pressione ${VERMELHO}Ctrl+C${NC}."
        fi
        
        ((current_run++))
    done
done

# Calcula o tempo total de execução
tempo_total_fim=$(date +%s)
tempo_total=$(( tempo_total_fim - tempo_total_inicio ))
minutos=$(( tempo_total / 60 ))
segundos=$(( tempo_total % 60 ))

echo -e "\n${VERDE}============================================================${NC}"
echo -e "${VERDE}🎉 CONCLUÍDO COM SUCESSO!${NC}"
echo -e "${VERDE}⏱️  Tempo total de processamento: ${minutos}m ${segundos}s${NC}"
echo -e "${VERDE}============================================================${NC}"