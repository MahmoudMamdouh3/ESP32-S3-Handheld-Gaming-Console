#ifndef SERIALIZER_HXX
#define SERIALIZER_HXX

#include "bspf.h"

class Serializer
{
  public:
    Serializer(const string& filename, bool readonly = false);
    Serializer(void);
    virtual ~Serializer(void);

  public:
    bool isValid(void) const { return myValid; }
    bool ok(void) const { return myValid && !myError; }
    void reset(void) { myPos = 0; myError = false; }
    uint32_t size(void) const { return (uint32_t)mySize; }
    void setPosition(uint32_t pos) { myPos = (pos <= mySize) ? pos : mySize; }

    const uint8_t* data(void) const { return myBuf; }
    bool loadFromBuffer(const uint8_t* src, size_t len);

    uint8_t getByte(void);
    void getByteArray(uint8_t* array, uint32_t size);
    uint16_t getShort(void);
    void getShortArray(uint16_t* array, uint32_t size);
    uint32_t getInt(void);
    void getIntArray(uint32_t* array, uint32_t size);
    string getString(void);
    bool getBool(void);

    void putByte(uint8_t value);
    void putByteArray(const uint8_t* array, uint32_t size);
    void putShort(uint16_t value);
    void putShortArray(const uint16_t* array, uint32_t size);
    void putInt(uint32_t value);
    void putIntArray(const uint32_t* array, uint32_t size);
    void putString(const string& str);
    void putBool(bool b);

    string get(void) const {
      if (!myBuf || mySize == 0) return string();
      return string(reinterpret_cast<const char*>(myBuf), mySize);
    }
    void set(const string& s) {
      loadFromBuffer(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

  private:
    bool ensureSpace(size_t extra);
    bool readBytes(void* dst, size_t n);
    void writeBytes(const void* src, size_t n);

    uint8_t* myBuf;
    size_t mySize;
    size_t myCap;
    size_t myPos;
    bool myValid;
    bool myReadonly;
    bool myError;

    enum {
      TruePattern  = 0xfe,
      FalsePattern = 0x01
    };
};

#endif
