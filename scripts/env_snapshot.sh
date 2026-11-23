#!/bin/bash
# Uso (a partir da raiz do repo): bash scripts/env_snapshot.sh > docs/env-v2.txt
C=${1:-ndn-ifa}; IMG=${IMAGE:-emrevoid/ndnsim:2.8}
echo "data_utc: $(date -u +%FT%TZ)"
echo "host: $(uname -srm) | cpus=$(nproc) | mem=$(free -h | awk '/Mem/{print $2}')"
echo "docker: $(docker --version)"
echo "imagem: $(docker image inspect $IMG --format '{{index .RepoDigests 0}}')"
echo "repo_commit: $(git rev-parse HEAD)$(git status --porcelain | grep -q . && echo ' (DIRTY)')"
docker exec $C bash -c '
NS=/home/ndn/ndnSIM/ns-3; SRC=$NS/src/ndnSIM
grep PRETTY /etc/os-release; g++ --version | head -1; python --version 2>&1; $NS/waf --version
grep -o "BUILD_PROFILE = .*" $NS/build/c4che/_cache.py | head -1
for d in $NS $SRC $SRC/NFD $SRC/ndn-cxx; do echo "commit $d: $(git -C $d log -1 --format=%H)"; done'
