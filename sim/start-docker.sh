#!/bin/bash
# Ambiente fixo dos experimentos. Rode a partir de sim/.
# Roda como root (-u 0): na imagem os headers do ndn-cxx são 0400 do uid 1000, ilegíveis
# para o uid do host. Após build/lotes: docker exec ndn-ifa chown -R "$(id -u):$(id -g)" /simulation
IMAGE="${IMAGE:-emrevoid/ndnsim:2.8}"
docker rm -f ndn-ifa 2>/dev/null
docker run -d --name ndn-ifa -v "$(pwd)":/simulation -w /simulation \
  -u 0 --ipc=host "$IMAGE" tail -f /dev/null
