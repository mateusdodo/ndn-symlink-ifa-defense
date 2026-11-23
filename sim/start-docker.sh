#!/bin/bash
# Ambiente fixo dos experimentos. Rode a partir de sim/.
IMAGE="${IMAGE:-emrevoid/ndnsim:2.8}"   # otimizado; use :2.8-debug só para NS_LOG
docker rm -f ndn-ifa 2>/dev/null
docker run -d --name ndn-ifa -v "$(pwd)":/simulation -w /simulation \
  -u "$(id -u)":"$(id -g)" --ipc=host "$IMAGE" tail -f /dev/null
