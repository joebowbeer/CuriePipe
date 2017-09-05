/*  OpenPipe Breakout with BLE MIDI
 *
 *  Connect the OpenPipe Breakout wires to Arduino 101 as follows:
 *  
 *  RED -> A2 (VCC)
 *  BLACK -> A3 (GND)
 *  WHITE-> A4 (SDA)
 *  GREEN-> A5 (SCL)
 *  
 *  www.openpipe.cc
 */

#include <Wire.h> // required by OpenPipe
#include <OpenPipe.h>
#include <CurieBLE.h>
#include "BleMidiEncoder.h"

// Select fingering here
#define FINGERING FINGERING_GAITA_GALEGA
//#define FINGERING FINGERING_GAITA_ASTURIANA
//#define FINGERING FINGERING_GREAT_HIGHLAND_BAGPIPE
//#define FINGERING FINGERING_UILLEANN_PIPE
//#define FINGERING FINGERING_SACKPIPA

// MIDI channels
#define CHANNEL 0

// MIDI messages
#define NOTEOFF 0x80
#define NOTEON 0x90

#define LED_PIN LED_BUILTIN
#define LED_ACTIVE HIGH

// BLE MIDI
BLEService midiSvc("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
BLECharacteristic midiChar("7772E5DB-3868-4112-A1A9-F2669D106BF3",
    BLEWrite | BLEWriteWithoutResponse | BLENotify | BLERead,
    BLE_MIDI_PACKET_SIZE);

class CurieBleMidiEncoder: public BleMidiEncoder {
  boolean setValue(const unsigned char value[], unsigned char length) {
    return midiChar.setValue(value, length);
  }
};

CurieBleMidiEncoder encoder;
unsigned long previousFingers;
uint8_t previousNote;
boolean playing;
boolean connected;

void setup() {
  previousFingers = 0xFF;
  previousNote = 0;
  playing = false;
  connected = false;

  pinMode(LED_PIN, OUTPUT);
  displayConnectionState();

  setupOpenPipe();
  setupBle();
}

void loop() {
  BLE.poll();
  connected = !!BLE.central();
  displayConnectionState();
  if (connected) {
    readFingers();
  }
}

void readFingers() {
  unsigned long fingers = OpenPipe.readFingers();

  // If fingers have changed...
  if (fingers != previousFingers) {
    previousFingers = fingers;

    // Check the low right thumb sensor
    if (OpenPipe.isON()) {
      playing = true;

      // If note changed...
      if (OpenPipe.note != previousNote && OpenPipe.note != 0xFF) {
        // Stop previous note and start current
        encodeNoteOff(CHANNEL, previousNote);
        encodeNoteOn(CHANNEL, OpenPipe.note, 127);
        encoder.sendMessages();
        previousNote = OpenPipe.note;
      }
    } else {
      if (playing) {
        encodeNoteOff(CHANNEL, previousNote); // Stop the note
        encoder.sendMessages();
        playing = false;
      }
    }
  }
}

void displayConnectionState() {
  // LED is lit until we're connected
  digitalWrite(LED_PIN, connected ? !LED_ACTIVE : LED_ACTIVE);
}

void setupOpenPipe() {
  OpenPipe.power(A2, A3); // VCC PIN in A2 and GND PIN in A3
  OpenPipe.config();
  OpenPipe.setFingering(FINGERING);
}

void setupBle() {
  BLE.begin();

  BLE.setConnectionInterval(6, 12); // 7.5 to 15 millis

  // set the local name peripheral advertises
  BLE.setLocalName("Pipe101");

  // set the UUID for the service this peripheral advertises
  BLE.setAdvertisedServiceUuid(midiSvc.uuid());

  // add service and characteristic
  BLE.addService(midiSvc);
  midiSvc.addCharacteristic(midiChar);

  // set an initial value for the characteristic
  encoder.sendMessage(0, 0, 0);

  BLE.advertise();
}

boolean encodeNoteOn(uint8_t channel, uint8_t note, uint8_t volume) {
  return encoder.appendMessage(NOTEON | channel, note & 0x7F, volume & 0x7F);
}

boolean encodeNoteOff(uint8_t channel, uint8_t note) {
  return encoder.appendMessage(NOTEOFF | channel, note & 0x7F, 0);
}

