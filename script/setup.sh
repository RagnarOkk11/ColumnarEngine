#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    g++ libgoogle-glog-dev
