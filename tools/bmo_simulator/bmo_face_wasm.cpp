#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../firmware/BmoGameboy/src/core/bmo_face.h"

namespace {

constexpr uint32_t SCREEN_WIDTH = 320;
constexpr uint32_t SCREEN_HEIGHT = 240;
constexpr size_t FRAMEBUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;

constexpr size_t CUSTOM_SPEECH_CAPACITY = 64;

uint16_t s_framebuffer[FRAMEBUFFER_SIZE];
char s_customSpeechBuf[CUSTOM_SPEECH_CAPACITY] = {};
uint32_t s_clockUs = 0;
uint32_t s_randomState = 1;
bool s_initialized = false;

uint32_t nextRandom() {
  s_randomState ^= s_randomState << 13;
  s_randomState ^= s_randomState >> 17;
  s_randomState ^= s_randomState << 5;
  return s_randomState;
}

const char* const kQuotes[] = {
  "Who wants to play video games?!",
  "Yay! BMO is so happy to see you!",
  "Sometimes life is scary, but we have games!",
  "Yes, Finn. It goes in my butt.",
  "BMO chop! If this were a real attack, you'd be dead!",
  "When bad things happen, I know you want to believe they're a joke.",
  "I am a little living boy!",
  "BMO is camera ready! *wink*",
  "Oh, you are making BMO blush!",
  "Whew! What a fun dance party!"
};
constexpr uint32_t kQuoteCount = sizeof(kQuotes) / sizeof(kQuotes[0]);

const char* quoteForIndex(uint32_t index) {
  return index < kQuoteCount ? kQuotes[index] : nullptr;
}

}  // namespace

extern "C" unsigned long millis() {
  return s_clockUs / 1000U;
}

extern "C" unsigned long micros() {
  return s_clockUs;
}

void randomSeed(unsigned long seed) {
  s_randomState = static_cast<uint32_t>(seed);
  if (s_randomState == 0) s_randomState = 1;
}

long random(long upperBound) {
  return upperBound > 0 ? static_cast<long>(nextRandom() % static_cast<uint32_t>(upperBound)) : 0;
}

long random(long lowerBound, long upperBound) {
  if (upperBound <= lowerBound) return lowerBound;
  return lowerBound + random(upperBound - lowerBound);
}

extern "C" void* heap_caps_malloc(size_t size, uint32_t) {
  return malloc(size);
}

extern "C" void heap_caps_free(void* memory) {
  free(memory);
}

namespace DisplayEmu {

void pushPixelsAt(int, int, int, int, const uint16_t*) {}

}  // namespace DisplayEmu

extern "C" {

__attribute__((visibility("default")))
int32_t bmo_face_init(uint32_t nowUs) {
  s_clockUs = nowUs;
  if (!s_initialized) {
    BmoFace::begin();
    s_initialized = true;
  }
  BmoFace::renderFullScreen(s_framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
  return 0;
}

__attribute__((visibility("default")))
void bmo_face_set_clock(uint32_t nowUs) {
  s_clockUs = nowUs;
}

__attribute__((visibility("default")))
uint16_t* bmo_face_step(uint32_t nowUs) {
  if (!s_initialized) return nullptr;
  s_clockUs = nowUs;
  BmoFace::update();
  BmoFace::renderFullScreen(s_framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint16_t* bmo_face_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
void bmo_face_set_expression(int32_t expression) {
  if (expression >= BmoFace::IDLE && expression <= BmoFace::CELEBRATING) {
    BmoFace::setExpression(static_cast<BmoFace::BmoExpression>(expression));
  }
}

__attribute__((visibility("default")))
int32_t bmo_face_get_expression() {
  return BmoFace::getExpression();
}

__attribute__((visibility("default")))
void bmo_face_set_gaze(float x, float y) {
  BmoFace::setGaze(x, y);
}

__attribute__((visibility("default")))
void bmo_face_trigger_wink() {
  BmoFace::triggerWink();
}

__attribute__((visibility("default")))
void bmo_face_trigger_blush() {
  BmoFace::triggerBlush();
}

__attribute__((visibility("default")))
void bmo_face_trigger_joy() {
  BmoFace::triggerJoy();
}

__attribute__((visibility("default")))
void bmo_face_tickle(float intensity) {
  BmoFace::tickle(intensity);
}

__attribute__((visibility("default")))
void bmo_face_pet_companion() {
  BmoFace::petCompanion();
}

__attribute__((visibility("default")))
int32_t bmo_face_say_quote(uint32_t index, uint32_t durationMs) {
  const char* quote = quoteForIndex(index);
  if (!quote) return -1;
  BmoFace::say(quote, durationMs);
  return 0;
}

__attribute__((visibility("default")))
void bmo_face_clear_speech() {
  BmoFace::clearSpeech();
}

__attribute__((visibility("default")))
void bmo_face_toggle_dance() {
  if (BmoFace::isDancing()) {
    BmoFace::stopDance();
    BmoFace::say("Whew! What a fun dance party!", 2500);
  } else {
    BmoFace::triggerDance(125.0f);
  }
}

__attribute__((visibility("default")))
int32_t bmo_face_is_dancing() {
  return BmoFace::isDancing() ? 1 : 0;
}

__attribute__((visibility("default")))
int32_t bmo_face_has_speech() {
  return BmoFace::hasSpeech() ? 1 : 0;
}

__attribute__((visibility("default")))
int32_t bmo_face_visible_chars() {
  return BmoFace::getVisibleChars();
}

__attribute__((visibility("default")))
const char* bmo_face_current_quote() {
  return BmoFace::getCurrentQuote();
}

__attribute__((visibility("default")))
int32_t bmo_face_buddy_level() {
  return BmoFace::getBuddyLevel();
}

__attribute__((visibility("default")))
int32_t bmo_face_happiness() {
  return BmoFace::getHappiness();
}

__attribute__((visibility("default")))
char* bmo_face_speech_buffer() {
  return s_customSpeechBuf;
}

__attribute__((visibility("default")))
uint32_t bmo_face_speech_capacity() {
  return static_cast<uint32_t>(CUSTOM_SPEECH_CAPACITY);
}

__attribute__((visibility("default")))
int32_t bmo_face_say_buffer(uint32_t length, uint32_t durationMs) {
  if (length == 0) return -1;
  uint32_t safeLen = length < CUSTOM_SPEECH_CAPACITY ? length : static_cast<uint32_t>(CUSTOM_SPEECH_CAPACITY - 1);
  s_customSpeechBuf[safeLen] = '\0';
  BmoFace::say(s_customSpeechBuf, durationMs);
  return 0;
}

__attribute__((visibility("default")))
void bmo_face_start_dance(float bpm) {
  BmoFace::triggerDance(bpm);
}

__attribute__((visibility("default")))
void bmo_face_stop_dance() {
  BmoFace::stopDance();
}

__attribute__((visibility("default")))
uint32_t bmo_face_quote_count() {
  return kQuoteCount;
}

__attribute__((visibility("default")))
const char* bmo_face_quote_text(uint32_t index) {
  return quoteForIndex(index);
}

}
