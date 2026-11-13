#include "Paddles.h"
#include "TIA.h"
#include "Sound.h"
#include <ctime>
#include "OSystem.h"
#include "SerialPort.h"
#include "PropsSet.h"
#include "Console.h"

OSystem::OSystem()
{
    myConsole      = 0;
    myNVRamDir     = ".";
    mySettings     = 0;
    mySound        = new Sound(this);
    mySerialPort   = new SerialPort();
    myEventHandler = new EventHandler(this);
    myPropSet      = new PropertiesSet(this);
    Paddles::setDigitalSensitivity(50);
    Paddles::setMouseSensitivity(5);
}

OSystem::~OSystem()
{
    delete mySound;
    delete mySerialPort;
    delete myEventHandler;
    delete myPropSet;
}

bool OSystem::create() { return 1; }
void OSystem::stateChanged(EventHandler::State state) { }

uint64_t OSystem::getTicks() const
{
    // DETERMINISM INVARIANT: this must return *emulated* time, never
    // wall-clock time. Random::initSeed() seeds the RNG from getTicks(),
    // and that RNG initializes cartridge RAM contents (CartXX ctors) and
    // undriven TIA pins. Upstream Stella's getTicks() is wall-clock;
    // if a future sync ever restores that, RAM-init randomness stops
    // being reproducible and netplay/run-ahead/replay determinism
    // silently breaks. TIA::getMilliSeconds() is derived purely from
    // emulated frame counters, which is what keeps this deterministic.
    if (!myConsole) return 0;
    return myConsole->tia().getMilliSeconds();
}

EventHandler::EventHandler(OSystem*) { }
EventHandler::~EventHandler() { }
