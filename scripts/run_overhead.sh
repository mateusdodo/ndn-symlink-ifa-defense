#!/bin/bash
# Exp. 4. Uso, a partir da raiz do repo, com o container ndn-ifa ativo: bash scripts/run_overhead.sh
# Roda SEQUENCIAL de proposito (tempo de parede). Nao use a VM para mais nada durante a execucao.
cd "$(dirname "$0")/../sim" || exit 1
COMMIT=$(git rev-parse --short HEAD)
configs="0 0 4
1000 0 4
10000 0 4
100000 0 4
0 100 4
0 10000 4
0 1000000 4
0 0 0
0 0 1"
while read -r fibN suspN arm; do
  for run in 1 2 3 4 5; do
    d=results/dev/ovh/arm${arm}_fib${fibN}_susp${suspN}_run${run}; mkdir -p "$d"
    docker exec -w /simulation/$d -e LD_LIBRARY_PATH=/usr/local/lib ndn-ifa \
      /simulation/build/ifa-arms --arm=$arm --rho=2 --fibN=$fibN --suspN=$suspN \
      --RngRun=$run --commit=$COMMIT || echo "FALHOU arm=$arm fib=$fibN susp=$suspN run=$run"
  done
done <<< "$configs"
docker exec ndn-ifa chown -R "$(id -u):$(id -g)" /simulation
