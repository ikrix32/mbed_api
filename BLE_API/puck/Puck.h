/**
 * Copyright 2014 Nordic Semiconductor
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */


#ifndef __PUCK_HPP__
#define __PUCK_HPP__
 
#include "BLEDevice.h"
#include "mbed.h"
#include "Log.h"
#include <vector>

enum PuckState {
    CONNECTING,
    CONNECTED,
    ADVERTISING,
    DISCONNECTED
};

const UUID stringToUUID(const char* str);

typedef void (*CharacteristicWriteCallback)(const uint8_t* value, uint8_t length);
 
 typedef struct {
     const UUID* uuid;
     std::vector<CharacteristicWriteCallback>* callbacks;
} CharacteristicWriteCallbacks;
  
/**
 *  @brief A library for easier setup and prototyping of IoT devices (pucks), by collecting everything that is common for all pucks in one place.
 *
 */ 
class Puck {
    private:
        Puck() {}
        Puck(const Puck&);
        Puck& operator=(const Puck&);
        
        BLEDevice ble;        
        uint8_t beaconPayload[25];
        PuckState state;
        std::vector<GattService*> services;
        std::vector<GattCharacteristic*> characteristics;
        std::vector<CharacteristicWriteCallbacks*> writeCallbacks;
        std::vector<CharacteristicWriteCallback> pendingCallbackStack;
        std::vector<const uint8_t*> pendingCallbackParameterDataStack;
        std::vector<uint8_t> pendingCallbackParameterLengthStack;
        
        GattCharacteristic **previousCharacteristics;
        
    public:
        static Puck &getPuck();
        
        BLEDevice &getBle() { return ble; }
        PuckState getState() { return state; }
        void setState(PuckState state);        
        void init(uint16_t minor);
        void startAdvertising();
        void stopAdvertising();
        void disconnect();
        bool drive();
        int countFreeMemory();
        void onDataWritten(GattAttribute::Handle_t handle, const uint8_t* data, const uint8_t length);
        void addCharacteristic(const UUID serviceUuid, const UUID characteristicUuid, int bytes, int properties = 0xA);

        void onCharacteristicWrite(const UUID* uuid, CharacteristicWriteCallback callback);
        void updateCharacteristicValue(const UUID uuid, uint8_t* value, int length);

        uint8_t* getCharacteristicValue(const UUID uuid);
};

/**
 *  @brief Returns singleton instance of puck object.
 *
 *  @return singleton instance of puck object.
 */
Puck &Puck::getPuck() {
    static Puck _puckSingletonInstance;
    return _puckSingletonInstance;
}

void onDisconnection(Gap::DisconnectionEventCallback_t disconnectionCallback){//Gap::Handle_t handle, Gap::DisconnectionReason_t disconnectReason) {
    LOG_INFO("Disconnected.\n");
    Puck::getPuck().setState(DISCONNECTED);
}

void onConnection(const Gap::ConnectionCallbackParams_t *params) {
    LOG_INFO("Connected.\n");
    Puck::getPuck().setState(CONNECTED);
}

void onDataWrittenCallback(const GattWriteCallbackParams *params) {
    Puck::getPuck().onDataWritten(params->handle, params->data, params->len);
}

bool isEqualUUID(const UUID* uuidA, const UUID uuidB) {
    const uint8_t* uuidABase = uuidA->getBaseUUID();
    const uint8_t* uuidBBase = uuidB.getBaseUUID();
    
    for(int i = 0; i < 16; i++) {
        if(uuidABase[i] != uuidBBase[i]) {
            return false;
        }
    }
    if(uuidA->getShortUUID() != uuidB.getShortUUID()) {
        return false;
    }
    return true;
}

/**
 * @brief Returns UUID representation of a 16-character string.
 *
 */
const UUID stringToUUID(const char* str) {
    uint8_t array[16];
    for(int i = 0; i < 16; i++) {
        array[i] = str[i];    
    }
    return UUID(array);
}

void Puck::disconnect() {
    ble.disconnect(Gap::LOCAL_HOST_TERMINATED_CONNECTION);    
}

/**
 *  @brief Approximates malloc-able heap space. Do not use in production code, as it may crash.
 *
 */
int Puck::countFreeMemory() {
    int blocksize = 256;
    int amount = 0;
    while (blocksize > 0) {
        amount += blocksize;
        LOG_VERBOSE("Trying to malloc %i bytes... ", amount);
        char *p = (char *) malloc(amount);
        if (p == NULL) {
            LOG_VERBOSE("FAIL!\n", amount);
            amount -= blocksize;
            blocksize /= 2;
        } else {
            free(p);
            LOG_VERBOSE("OK!\n", amount);
        }
    }
    LOG_DEBUG("Free memory: %i bytes.\n", amount);
    return amount;
}

void Puck::setState(PuckState state) {
    LOG_DEBUG("Changed state to %i\n", state);
    this->state = state;    
}

/**
 *  @brief  Call after finishing configuring puck (adding services, characteristics, callbacks).
            Starts advertising over bluetooth le. 
 *
 *  @parameter  minor
 *              Minor number to use for iBeacon identifier.
 *
 */
void Puck::init(uint16_t minor) {
    /*
     * The Beacon payload (encapsulated within the MSD advertising data structure)
     * has the following composition:
     * 128-Bit UUID = E2 0A 39 F4 73 F5 4B C4 A1 2F 17 D1 AD 07 A9 61
     * Major/Minor  = 1337 / XXXX
     * Tx Power     = C8
     */
    uint8_t beaconPayloadTemplate[] = {
        0x00, 0x00, // Company identifier code (0x004C == Apple)
        0x02,       // ID
        0x15,       // length of the remaining payload
        0xE2, 0x0A, 0x39, 0xF4, 0x73, 0xF5, 0x4B, 0xC4, // UUID
        0xA1, 0x2F, 0x17, 0xD1, 0xAD, 0x07, 0xA9, 0x61,
        0x13, 0x37, // the major value to differenciate a location (Our app requires 1337 as major number)
        0x00, 0x00, // the minor value to differenciate a location (Change this to differentiate location pucks)
        0xC8        // 2's complement of the Tx power (-56dB)
    };
    beaconPayloadTemplate[22] = minor >> 8;
    beaconPayloadTemplate[23] = minor & 255;
    
    for (int i=0; i < 25; i++) {
        beaconPayload[i] = beaconPayloadTemplate[i];
    }
    
    ble.init();
    LOG_DEBUG("Inited BLEDevice.\n");
    setState(DISCONNECTED);
    
    char deviceName[10];
    sprintf(&deviceName[0], "Puck %04X", minor);
    deviceName[9] = '\0';
    ble.setDeviceName((const uint8_t*) deviceName);
    
    ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    LOG_DEBUG("Accumulate advertising payload: BREDR_NOT_SUPPORTED | LE_GENERAL_DISCOVERABLE.\n");
    
    ble.accumulateAdvertisingPayload(GapAdvertisingData::MANUFACTURER_SPECIFIC_DATA, beaconPayload, sizeof(beaconPayload));
    LOG_DEBUG("Accumulate advertising payload: beacon data.\n");
    
    ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    LOG_DEBUG("Setting advertising type: ADV_CONNECTABLE_UNDIRECTED.\n");
    
    int hundredMillisecondsInAdvertisingIntervalFormat = 160;
    ble.setAdvertisingInterval(hundredMillisecondsInAdvertisingIntervalFormat); 
    LOG_DEBUG("Set advertising interval: 160 (100 ms).\n");
    
    ble.onDisconnection(onDisconnection);
    ble.onConnection(onConnection);
    ble.onDataWritten(onDataWrittenCallback);
    LOG_DEBUG("Hooked up internal event handlers.\n");
    
    for(uint i = 0; i < services.size(); i++) {
        ble.addService(*services[i]);
        LOG_DEBUG("Added service %x to BLEDevice\n", services[i]);
    }
    
    LOG_INFO("Inited puck as 0x%X.\n", minor);
}

void Puck::startAdvertising() {
    ble.startAdvertising();
    LOG_INFO("Starting to advertise.\n");
    setState(ADVERTISING);
}

void Puck::stopAdvertising() {
    if(state == ADVERTISING) {
        ble.stopAdvertising();
        LOG_INFO("Stopped advertising.\n");
        setState(DISCONNECTED);
    } else {
        LOG_WARN("Tried to stop advertising, but advertising is already stopped!\n");    
    }
}

/** 
 *  @brief  Extends the given gatt service with the given gatt characteristic.
 *          If the service doesn't exist, it is created.
 *
 *  @param  serviceUuid
            UUID of the gatt service to be extended.
 *
 *  @param  characteristicUuid
 *          UUID to use for this characteristic.
 *
 *  @param  bytes
 *          Length in bytes of this characteristic's value.
 *
 *  @param  properties
 *          8-bit bit field containing the characteristic's properties. See @ref ble_gatt_char_properties_t.
 *
 *  @return Void.
 */
void Puck::addCharacteristic(const UUID serviceUuid, const UUID characteristicUuid, int bytes, int properties) {
    MBED_ASSERT(bytes <= 20);
    uint16_t size = sizeof(uint8_t) * bytes;
    uint8_t* value = (uint8_t*) malloc(size);
    if(value == NULL) {
        LOG_ERROR("Unable to malloc value for characteristic. Possibly out of memory!\n");    
    }
    
    GattCharacteristic* characteristic = new GattCharacteristic(characteristicUuid, value, size, size, properties);
    characteristics.push_back(characteristic);
    
    
    GattService* service = NULL;
    
    int removeIndex = -1;
    for(int i = 0; i < services.size(); i++) {
        if(isEqualUUID(&services[i]->getUUID(), serviceUuid)) {
            service = services[i];
            removeIndex = i;
            break;
        }
    }
    GattCharacteristic** characteristics = NULL;
    int characteristicsLength = 0;
    if(service != NULL) {
        characteristicsLength = service->getCharacteristicCount() + 1;
        characteristics = (GattCharacteristic**) malloc(sizeof(GattCharacteristic*) * characteristicsLength);
        if(characteristics == NULL) {
            LOG_ERROR("Unable to malloc array of characteristics for service creation. Possibly out of memory!\n");    
        }
        for(int i = 0; i < characteristicsLength; i++) {
            characteristics[i] = service->getCharacteristic(i);    
        }
        services.erase(services.begin() + removeIndex);
        delete service;
        free(previousCharacteristics);
    } else {
        characteristicsLength = 1;
        characteristics = (GattCharacteristic**) malloc(sizeof(GattCharacteristic*) * characteristicsLength);
        if(characteristics == NULL) {
            LOG_ERROR("Unable to malloc array of characteristics for service creation. Possibly out of memory!\n");    
        }
    }
    
    characteristics[characteristicsLength - 1] = characteristic;
    previousCharacteristics = characteristics;
    service = new GattService(serviceUuid, characteristics, characteristicsLength);
    services.push_back(service);
    LOG_DEBUG("Added characteristic.\n");
}


/** 
 *  @brief  Update the value of the given gatt characteristic.
 *
 *  @param  uuid
            UUID of the gatt characteristic to be updated.
 *
 *  @param  value
 *          New value of the characteristic.
 *
 *  @param  length
 *          Length in bytes of the characteristic's value.
 *
 *  @return Void.
 */
void Puck::updateCharacteristicValue(const UUID uuid, uint8_t* value, int length) {
    GattCharacteristic* characteristic = NULL;
    for(int i = 0; i < characteristics.size(); i++) {
        GattAttribute &gattAttribute = characteristics[i]->getValueAttribute();
        if(isEqualUUID(&gattAttribute.getUUID(), uuid)) {
            characteristic = characteristics[i];
            break;
        }
    }
    if(characteristic != NULL) {
        ble.updateCharacteristicValue(characteristic->getValueHandle(), value, length);
        LOG_VERBOSE("Updated characteristic value.\n");
    } else {
        LOG_WARN("Tried to update an unkown characteristic!\n");    
    }
}

/** 
 *  @brief Pass control to the bluetooth stack, executing pending callbacks afterwards. Should be used inside a while condition loop.
 *
 *  Example:
 *  @code
 *  while (puck->drive()) {
 *      // Do stuff
 *  }
 *  @endcode
 *
 * @return true.
 *
 */
bool Puck::drive() {
    if(state == DISCONNECTED) {
        startAdvertising();
    }

    ble.waitForEvent();

    while(pendingCallbackStack.size() > 0) {
        pendingCallbackStack.back()(pendingCallbackParameterDataStack.back(), pendingCallbackParameterLengthStack.back());
        pendingCallbackStack.pop_back();
        pendingCallbackParameterDataStack.pop_back();
        pendingCallbackParameterLengthStack.pop_back();
    }
    return true;
}

/** 
 *  @brief Register callback to be triggered on characteristic write.
 *
 *  @parameter  uuid
 *              UUID of the gatt characteristic to bind callback to.
 *
 *  @parameter  callback
 *              CharacteristicWriteCallback to be executed on characteristic write.It's signature should be void (*CharacteristicWriteCallback)(const uint8_t* value, uint8_t length); "value" is the value that was written, and "length" is the length of the value that was written.
 *
 *  @return Void.
 *
 */
void Puck::onCharacteristicWrite(const UUID* uuid, CharacteristicWriteCallback callback) {
    CharacteristicWriteCallbacks* cb = NULL;
    for(int i = 0; i< writeCallbacks.size(); i++) {
        if(isEqualUUID(writeCallbacks[i]->uuid, *uuid)) {
            cb = writeCallbacks[i];    
            break;
        }
    }
    if(cb == NULL) {
        cb = (CharacteristicWriteCallbacks*) malloc(sizeof(CharacteristicWriteCallbacks));
        if(cb == NULL) {
            LOG_ERROR("Could not malloc CharacteristicWriteCallbacks container. Possibly out of memory!\n");    
        }
        cb->uuid = uuid;
        cb->callbacks = new std::vector<CharacteristicWriteCallback>();
        writeCallbacks.push_back(cb);
    }
    cb->callbacks->push_back(callback);
    LOG_VERBOSE("Bound characteristic write callback (uuid: %x, callback: %x)\n", uuid, callback);
}

/**
 *  @brief Returns current value of provided gatt characteristic.
 *
 */
uint8_t* Puck::getCharacteristicValue(const UUID uuid) {
    LOG_VERBOSE("Reading characteristic value for UUID %x\n", uuid);
    for(int i = 0; i < characteristics.size(); i++) {
        GattAttribute &gattAttribute = characteristics[i]->getValueAttribute();
        if(isEqualUUID(&gattAttribute.getUUID(), uuid)) {
            return gattAttribute.getValuePtr();
        }
    }
    LOG_WARN("Tried to read an unknown characteristic!");
    return NULL;
}

/**
 * @brief For internal use only. Exposed to hack around mbed framework limitation.
 *
 */
void Puck::onDataWritten(GattAttribute::Handle_t handle, const uint8_t* data, uint8_t length) {
    for (int i = 0; i < characteristics.size(); i++) {
        GattCharacteristic* characteristic = characteristics[i];
        
        if (characteristic->getValueHandle() == handle) {
            GattAttribute &gattAttribute = characteristic->getValueAttribute();

            for(int j = 0; j < writeCallbacks.size(); j++) {    
                CharacteristicWriteCallbacks* characteristicWriteCallbacks = writeCallbacks[j];

                if(isEqualUUID(characteristicWriteCallbacks->uuid, gattAttribute.getUUID())) {
                    for(int k = 0; k < characteristicWriteCallbacks->callbacks->size(); k++) {
                        pendingCallbackStack.push_back(characteristicWriteCallbacks->callbacks->at(k));
                        
                        pendingCallbackParameterDataStack.push_back(data);
                        pendingCallbackParameterLengthStack.push_back(length);
                    }
                    return;
                }
            }
        }
    }
}


 #endif // __PUCK_HPP__
