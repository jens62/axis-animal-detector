#!/bin/sh
# Runs the host-side unit tests inside the ACAP SDK image (needs gcc from apt).
set -e
cd "$(dirname "$0")/.."
docker run --rm --platform=linux/amd64 -v "$PWD":/src:ro axisecp/acap-native-sdk:12.11.0-aarch64-ubuntu24.04 sh -c '
  apt-get update -qq >/dev/null && apt-get install -y -qq gcc libjansson-dev >/dev/null &&
  gcc -Wall -Wextra -Werror -fsanitize=address,undefined -g /src/tests/test_animal_events.c /src/app/animal_events.c -o /tmp/test_animal_events &&
  gcc -Wall -Wextra -Werror -fsanitize=address,undefined -g /src/tests/test_regions_crop.c /src/app/regions.c /src/app/crop.c -ljansson -lpthread -lm -o /tmp/test_regions_crop &&
  /tmp/test_animal_events && /tmp/test_regions_crop'
