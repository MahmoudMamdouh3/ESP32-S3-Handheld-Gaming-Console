#include <string.h>

#include "src/cores/doom_wad_path.h"
#include "test_check.h"

int main() {
  char buf[DoomWadPath::MAX_WAD_PATH_BYTES] = {};

  // Standard long nested path (>72 chars) and up to 255-char paths must fit inside MAX_WAD_PATH_BYTES (264).
  const char* longName = "subdirectories/action_classics_collection/the_ultimate_doom_episode_iv_edition/DOOM1.WAD";
  BMO_CHECK(strlen(longName) > 72);
  BMO_CHECK(DoomWadPath::build(longName, buf, sizeof(buf)));
  BMO_CHECK(strncmp(buf, "/sd/", 4) == 0);
  BMO_CHECK(strcmp(buf + 4, longName) == 0);

  char midLong[140];
  memset(midLong, 'A', 135);
  midLong[135] = '\0';
  BMO_CHECK(DoomWadPath::build(midLong, buf, sizeof(buf)));

  // Reject null, empty, and overflow filenames safely (>259 relative bytes overflows "/sd/" + 255 + NUL = 264).
  BMO_CHECK(!DoomWadPath::build(nullptr, buf, sizeof(buf)));
  BMO_CHECK(!DoomWadPath::build("", buf, sizeof(buf)));
  char tooLong[280];
  memset(tooLong, 'A', 270);
  tooLong[270] = '\0';
  BMO_CHECK(!DoomWadPath::build(tooLong, buf, sizeof(buf)));
  return 0;
}
