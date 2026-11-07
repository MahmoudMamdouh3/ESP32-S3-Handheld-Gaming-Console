#include "path_key.h"

#include <stdio.h>

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    char key[33];
    if (RomKey::make(argv[i], key, sizeof(key))) {
      puts(key);
    } else {
      puts("INVALID");
    }
  }
  return 0;
}
