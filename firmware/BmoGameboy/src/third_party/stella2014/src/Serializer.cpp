#include "Serializer.h"
#include "../../../hal/psram_alloc.h"
#include <cstring>

Serializer::Serializer(const string& filename, bool readonly)
  : myBuf(0), mySize(0), myCap(0), myPos(0), myValid(false), myReadonly(readonly), myError(false)
{
  (void)filename;
  // File streams are not opened directly from Stella emucore; callers use loadFromBuffer/data.
}

Serializer::Serializer(void)
  : myBuf(0), mySize(0), myCap(0), myPos(0), myValid(true), myReadonly(false), myError(false)
{
  ensureSpace(4096u);
}

Serializer::~Serializer(void)
{
  if (myBuf) {
    bmo_psram_free(myBuf);
    myBuf = 0;
  }
}

bool Serializer::ensureSpace(size_t extra)
{
  if (myReadonly) {
    myError = true;
    return false;
  }
  size_t needed = myPos + extra;
  if (needed <= myCap) return true;

  size_t newCap = myCap ? myCap : 4096u;
  while (newCap < needed) {
    newCap *= 2u;
  }
  uint8_t* next = (uint8_t*)bmo_psram_realloc(myBuf, newCap);
  if (!next) {
    myValid = false;
    myError = true;
    return false;
  }
  myBuf = next;
  myCap = newCap;
  return true;
}

bool Serializer::loadFromBuffer(const uint8_t* src, size_t len)
{
  mySize = 0;
  myPos = 0;
  myError = false;
  myReadonly = false;
  myValid = true;
  if (len == 0 || !src) return true;
  if (!ensureSpace(len)) return false;
  memcpy(myBuf, src, len);
  mySize = len;
  return true;
}

bool Serializer::readBytes(void* dst, size_t n)
{
  if (n == 0) return true;
  if (!myBuf || myPos + n > mySize) {
    memset(dst, 0, n);
    myError = true;
    return false;
  }
  memcpy(dst, myBuf + myPos, n);
  myPos += n;
  return true;
}

void Serializer::writeBytes(const void* src, size_t n)
{
  if (n == 0 || !ensureSpace(n)) return;
  memcpy(myBuf + myPos, src, n);
  myPos += n;
  if (myPos > mySize) mySize = myPos;
}

uint8_t Serializer::getByte(void)
{
  uint8_t v = 0;
  readBytes(&v, 1u);
  return v;
}

void Serializer::getByteArray(uint8_t* array, uint32_t size)
{
  if (size > 0 && array) readBytes(array, size);
}

uint16_t Serializer::getShort(void)
{
  uint8_t b[2] = {0, 0};
  readBytes(b, 2u);
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

void Serializer::getShortArray(uint16_t* array, uint32_t size)
{
  for (uint32_t i = 0; i < size; ++i) array[i] = getShort();
}

uint32_t Serializer::getInt(void)
{
  uint8_t b[4] = {0, 0, 0, 0};
  readBytes(b, 4u);
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

void Serializer::getIntArray(uint32_t* array, uint32_t size)
{
  for (uint32_t i = 0; i < size; ++i) array[i] = getInt();
}

string Serializer::getString(void)
{
  uint32_t len = getInt();
  if (myError || len > 1024u * 1024u || myPos + len > mySize) {
    myError = true;
    return string();
  }
  string s;
  s.resize(len);
  if (len > 0) readBytes(&s[0], len);
  return s;
}

bool Serializer::getBool(void)
{
  uint8_t val = getByte();
  if (val == TruePattern) return true;
  if (val == FalsePattern) return false;
  return val != 0;
}

void Serializer::putByte(uint8_t value)
{
  writeBytes(&value, 1u);
}

void Serializer::putByteArray(const uint8_t* array, uint32_t size)
{
  if (size > 0 && array) writeBytes(array, size);
}

void Serializer::putShort(uint16_t value)
{
  uint8_t b[2] = { (uint8_t)(value & 0xffu), (uint8_t)((value >> 8) & 0xffu) };
  writeBytes(b, 2u);
}

void Serializer::putShortArray(const uint16_t* array, uint32_t size)
{
  for (uint32_t i = 0; i < size; ++i) putShort(array[i]);
}

void Serializer::putInt(uint32_t value)
{
  uint8_t b[4] = {
    (uint8_t)(value & 0xffu),
    (uint8_t)((value >> 8) & 0xffu),
    (uint8_t)((value >> 16) & 0xffu),
    (uint8_t)((value >> 24) & 0xffu)
  };
  writeBytes(b, 4u);
}

void Serializer::putIntArray(const uint32_t* array, uint32_t size)
{
  for (uint32_t i = 0; i < size; ++i) putInt(array[i]);
}

void Serializer::putString(const string& str)
{
  uint32_t len = (uint32_t)str.length();
  putInt(len);
  if (len > 0) writeBytes(str.data(), len);
}

void Serializer::putBool(bool b)
{
  putByte(b ? (uint8_t)TruePattern : (uint8_t)FalsePattern);
}
