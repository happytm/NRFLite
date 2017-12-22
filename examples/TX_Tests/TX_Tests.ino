/*

This is used for internal testing of the library.

Goal is to test all bitrates, all packet sizes, all ACK packet sizes, with and without
interrupts, while measuring bitrates.

Manually adjusted for shared CE/CSN operation, and 2-pin operation.

*/

#include <SPI.h>
#include <NRFLite.h>

const static uint8_t RADIO_ID = 1;
const static uint8_t DESTINATION_RADIO_ID = 0;
const static uint8_t PIN_RADIO_CE = 9; // 9 or 10 depending on test condition
const static uint8_t PIN_RADIO_CSN = 10;

#define SERIAL_SPEED 115200
#define debug(input)   { Serial.print(input);   }
#define debugln(input) { Serial.println(input); }

enum RadioStates { StartSync, RunDemos };

struct RadioPacket     { uint8_t Counter; RadioStates RadioState; uint8_t Data[29]; };
struct RadioAckPacketA { uint8_t Counter; uint8_t Data[31]; };
struct RadioAckPacketB { uint8_t Counter; uint8_t Data[30]; };

NRFLite _radio;
RadioPacket _radioData;
RadioAckPacketA _radioDataAckA;
RadioAckPacketB _radioDataAckB;

const uint16_t DEMO_LENGTH_MILLIS = 4200;
const uint16_t DEMO_INTERVAL_MILLIS = 300;

uint8_t _hadInterrupt, _showMessageInInterrupt;
uint32_t _bitsPerSecond, _packetCount, _successPacketCount, _failedPacketCount, _ackAPacketCount, _ackBPacketCount;
uint32_t _currentMillis, _lastMillis, _endMillis;
float _packetLoss;

void setup()
{
    Serial.begin(SERIAL_SPEED);
    
    if (!_radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE250KBPS))
    {
        debugln("Cannot communicate with radio");
        while (1); // Wait here forever.
    }
    else
    {
        debugln();
        debugln("250KBPS bitrate");
        _radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE250KBPS);
        runDemos();
        
        debugln();
        debugln("1MBPS bitrate");
        _radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE1MBPS);
        runDemos();
        
        debugln();
        debugln("2MBPS bitrate");
        _radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE2MBPS);
        runDemos();
        
        debugln();
        debugln("Complete");
    }
}

void loop() {}

void radioInterrupt()
{
    uint8_t txOk, txFail, rxReady;
    _radio.whatHappened(txOk, txFail, rxReady);
    
    if (txOk) {
        
        _successPacketCount++;
        if (_showMessageInInterrupt) debugln("...Success");

        uint8_t ackLength = _radio.hasAckData();
        
        while (ackLength > 0)
        {
            if (ackLength == sizeof(_radioDataAckA))
            {
                _radio.readData(&_radioDataAckA);
                _ackAPacketCount++;
            }
            else if (ackLength == sizeof(_radioDataAckB))
            {
                _radio.readData(&_radioDataAckB);
                _ackBPacketCount++;
            }
            
            ackLength = _radio.hasAckData();
        }
    }
    
    if (txFail)
    {
        _failedPacketCount++;
        if (_showMessageInInterrupt) debugln("...Failed");
    }
}

void runDemos()
{
    startSync();
    demoPolling();
    demoInterrupts();
    demoAckPayload();
    demoPollingBitrate();
    demoInterruptsBitrate();
    demoPollingBitrateNoAck();
    demoInterruptsBitrateNoAck();
    demoPollingBitrateAckPayload();
    demoInterruptsBitrateAckPayload();
    demoPollingBitrateAllPacketSizes();
    demoPollingBitrateAllAckSizes();
}

void startSync()
{
    debugln("  Starting sync");
    
    detachInterrupt(1);
    _radioData.RadioState = StartSync;
    
    while (!_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData)))
    {
        debugln("  Retrying in 4 seconds");
        delay(4000);
    }
    
    _radioData.RadioState = RunDemos;
}

void demoPolling() 
{
    /*
    Most demos have the same steps:
        Wait demo interval time.
        Show demo message.
        Reset shared variables necessary for the demo.
        Calculate demo end time.
        Run a loop that lasts until the end time.
    */
    
    delay(DEMO_INTERVAL_MILLIS);
    
    debugln("Polling");
    
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        if (millis() - _lastMillis > 999) 
        {
            _lastMillis = millis();
            
            debug("  Send "); debug(++_radioData.Counter);
            
            if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData))) 
            {
                debugln("...Success");
            }
            else 
            {
                debugln("...Failed");
            }
        }
    }
}

void demoInterrupts() 
{
    delay(DEMO_INTERVAL_MILLIS);

    debugln("Interrupts");
    
    attachInterrupt(1, radioInterrupt, FALLING);
    _showMessageInInterrupt = 1;
    
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        if (millis() - _lastMillis > 999) 
        {
            _lastMillis = millis();
            debug("  Start send "); debug(++_radioData.Counter);
            _radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData));
        }
    }
    
    detachInterrupt(1);
}

void demoAckPayload() 
{
    delay(DEMO_INTERVAL_MILLIS);

    debugln("ACK payloads");
    
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        if (millis() - _lastMillis > 1500) 
        {
            _lastMillis = millis();
            
            debug("  Send "); debug(++_radioData.Counter);
            
            if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData))) 
            {
                debug("...Success");
                uint8_t ackLength = _radio.hasAckData();
                
                while (ackLength > 0) 
                {
                    // Determine type of ACK packet.
                    if (ackLength == sizeof(_radioDataAckA)) 
                    {
                        _radio.readData(&_radioDataAckA);
                        debug(" - Received ACK A ");
                        debug(_radioDataAckA.Counter);
                    }
                    else if (ackLength == sizeof(_radioDataAckB)) 
                    {
                        _radio.readData(&_radioDataAckB);
                        debug(" - Received ACK B ");
                        debug(_radioDataAckB.Counter);
                    }
                    
                    ackLength = _radio.hasAckData();
                }
                
                debugln();
            }
            else 
            {
                debugln("...Failed");
            }
        }
    }
}

void demoPollingBitrate() 
{
    delay(DEMO_INTERVAL_MILLIS);
    
    debugln("Polling bitrate");
    
    _successPacketCount = 0;
    _failedPacketCount = 0;
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 999) 
        {
            _packetCount = _successPacketCount + _failedPacketCount;
            _bitsPerSecond = sizeof(_radioData) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            _packetLoss = _successPacketCount / (float)_packetCount * 100.0;
            debug("  ");
            debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
            debug(_packetLoss); debug("% ");
            debug(_bitsPerSecond); debugln(" bps");
            _successPacketCount = 0;
            _failedPacketCount = 0;
            _lastMillis = _currentMillis;
        }
        
        if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData))) 
        {
            _successPacketCount++;
        }
        else 
        {
            _failedPacketCount++;
        }
    }
}

void demoPollingBitrateNoAck() {
    
    delay(DEMO_INTERVAL_MILLIS);
    
    debugln("Polling bitrate NO_ACK");
    
    _packetCount = 0;
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 999) 
        {
            _bitsPerSecond = sizeof(_radioData) * _packetCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            debug("  ");
            debug(_packetCount); debug(" packets ");
            debug(_bitsPerSecond); debugln(" bps");
            _packetCount = 0;
            _lastMillis = _currentMillis;
        }
        
        if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData), NRFLite::NO_ACK)) 
        {
            _packetCount++;
        }
        else {
            // NO_ACK sends are always treated as successful.
        }
    }
}

void demoInterruptsBitrate() 
{
    delay(DEMO_INTERVAL_MILLIS);
    
    debugln("Interrupts bitrate");
    
    attachInterrupt(1, radioInterrupt, FALLING);
    _showMessageInInterrupt = 0;
    _successPacketCount = 0;
    _failedPacketCount = 0;
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 999) 
        {
            _packetCount = _successPacketCount + _failedPacketCount;
            _bitsPerSecond = sizeof(_radioData) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            _packetLoss = _successPacketCount / (float)_packetCount * 100;
            debug("  ");
            debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
            debug(_packetLoss); debug("% ");
            debug(_bitsPerSecond); debugln(" bps");
            _successPacketCount = 0;
            _failedPacketCount = 0;
            _lastMillis = _currentMillis;
        }
        
        _radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData));
    }
    
    detachInterrupt(1);
}

void demoInterruptsBitrateNoAck() 
{
    delay(DEMO_INTERVAL_MILLIS);
    
    debugln("Interrupts bitrate NO_ACK");
    
    attachInterrupt(1, radioInterrupt, FALLING);
    _showMessageInInterrupt = 0;
    _successPacketCount = 0;
    _failedPacketCount = 0;
    uint32_t sendCount = 0;
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 999) 
        {
            _bitsPerSecond = sizeof(_radioData) * sendCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            _lastMillis = _currentMillis;
            debug("  ");
            debug(sendCount); debug(" packets ");
            debug(_bitsPerSecond); debugln(" bps");
            sendCount = 0;
        }
        
        _radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData), NRFLite::NO_ACK);
        
        // Found that using _successPacketCount set in the interrupt handler was giving
        // inconsistent results and not agreeing with the received packet counts.
        // Sends are always considered a success when NO_ACK is used so we'll just track
        // the number of sends that we do and use it for the bitrate calculation.
        sendCount++;
    }
    
    detachInterrupt(1);
}

void demoPollingBitrateAckPayload() 
{
    delay(DEMO_INTERVAL_MILLIS);

    debugln("Polling bitrate ACK payload");
    
    _ackAPacketCount = 0;
    _ackBPacketCount = 0;
    _successPacketCount = 0;
    _failedPacketCount = 0;
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 999) 
        {
            _packetCount = _successPacketCount + _failedPacketCount;
            _bitsPerSecond = sizeof(_radioData) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            _packetLoss = _successPacketCount / (float)_packetCount * 100.0;
            debug("  ");
            debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
            debug(_packetLoss); debug("% A");
            debug(_ackAPacketCount);
            debug(" B");
            debug(_ackBPacketCount);
            debug(" "); debug(_bitsPerSecond); debugln(" bps");
            _successPacketCount = 0;
            _failedPacketCount = 0;
            _ackAPacketCount = 0;
            _ackBPacketCount = 0;
            _lastMillis = _currentMillis;
        }
        
        if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData))) 
        {
            _successPacketCount++;
            uint8_t ackLength = _radio.hasAckData();
            
            while (ackLength > 0) 
            {
                // Count the number of each type of ACK packet we receive for display later.
                if (ackLength == sizeof(_radioDataAckA)) 
                {
                    _radio.readData(&_radioDataAckA);
                    _ackAPacketCount++;
                }
                else if (ackLength == sizeof(_radioDataAckB)) 
                {
                    _radio.readData(&_radioDataAckB);
                    _ackBPacketCount++;
                }
                
                ackLength = _radio.hasAckData();
            }
        }
        else 
        {
            _failedPacketCount++;
        }
    }
}

void demoInterruptsBitrateAckPayload() 
{
    delay(DEMO_INTERVAL_MILLIS);

    debugln("Interrupts bitrate ACK payload");
    
    attachInterrupt(1, radioInterrupt, FALLING);
    _showMessageInInterrupt = 0;
    _successPacketCount = 0;
    _failedPacketCount = 0;
    _ackAPacketCount = 0;
    _ackBPacketCount = 0;
    _endMillis = millis() + DEMO_LENGTH_MILLIS;
    _lastMillis = millis();
    
    while (millis() < _endMillis) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 999) 
        {
            _packetCount = _successPacketCount + _failedPacketCount;
            _bitsPerSecond = sizeof(_radioData) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            _packetLoss = _successPacketCount / (float)_packetCount * 100.0;
            debug("  ");
            debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
            debug(_packetLoss); debug("% A");
            debug(_ackAPacketCount);
            debug(" B");
            debug(_ackBPacketCount);
            debug(" "); debug(_bitsPerSecond); debugln(" bps");
            _successPacketCount = 0;
            _failedPacketCount = 0;
            _ackAPacketCount = 0;
            _ackBPacketCount = 0;
            _lastMillis = _currentMillis;
        }
        
        _radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData));
    }
    
    detachInterrupt(1);
}

void demoPollingBitrateAllPacketSizes() 
{
    delay(DEMO_INTERVAL_MILLIS);
    
    debugln("Polling bitrate all packet sizes");
    
    _successPacketCount = 0;
    _failedPacketCount = 0;
    _lastMillis = millis();
    
    uint8_t packet[32];
    uint8_t packetSize = 1; // Start with this packet size and work our way up.
    
    while (1) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 2999) // Send 3 seconds worth of packets for each size.
        { 
            _packetCount = _successPacketCount + _failedPacketCount;
            _bitsPerSecond = packetSize * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            _packetLoss = _successPacketCount / (float)_packetCount * 100.0;
            
            debug("  Size "); debug(packetSize); debug(" ");
            debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
            debug(_packetLoss); debug("% ");
            debug(_bitsPerSecond); debugln(" bps");
            
            _successPacketCount = 0;
            _failedPacketCount = 0;
            _lastMillis = _currentMillis;
            
            if (packetSize == 32) break; // Stop demo when we reach the max packet size.

            packetSize++; // Increase size of the packet to send.
            delay(DEMO_INTERVAL_MILLIS);
        }
        
        if (_radio.send(DESTINATION_RADIO_ID, &packet, packetSize)) 
        {
            _successPacketCount++;
        }
        else 
        {
            _failedPacketCount++;
        }
    }
}

void demoPollingBitrateAllAckSizes() 
{
    // Sends a packet which is 32 bytes and receives ACK packets that increase in length.
    
    delay(DEMO_INTERVAL_MILLIS);
    
    debugln("Polling bitrate all ACK sizes");
    
    _successPacketCount = 0;
    _failedPacketCount = 0;
    _lastMillis = millis();
    
    uint8_t ackPacket[32];
    uint8_t ackPacketSize;
    uint8_t lastAckPacketSize;
    
    while (1) 
    {
        _currentMillis = millis();
        
        if (_currentMillis - _lastMillis > 2999) { // Send 3 seconds worth of packets for each size.
            
            _packetCount = _successPacketCount + _failedPacketCount;
            _bitsPerSecond = sizeof(_radioData) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
            _packetLoss = _successPacketCount / (float)_packetCount * 100.0;
            
            debug("  ACK Size "); debug(lastAckPacketSize); debug(" ");
            debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
            debug(_packetLoss); debug("% ");
            debug(_bitsPerSecond); debugln(" bps");
            
            _successPacketCount = 0;
            _failedPacketCount = 0;
            _lastMillis = _currentMillis;
            
            if (lastAckPacketSize == 32) break; // Stop demo when we reach the max packet size.

            delay(DEMO_INTERVAL_MILLIS);
        }
        
        if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(_radioData))) 
        {
            _successPacketCount++;
            ackPacketSize = _radio.hasAckData();
            
            while (ackPacketSize > 0) 
            {
                lastAckPacketSize = ackPacketSize; // Store ACK size for display.
                _radio.readData(&ackPacket);
                ackPacketSize = _radio.hasAckData();
            }
        }
        else 
        {
            _failedPacketCount++;
        }
    }
}