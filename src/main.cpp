#include <Arduino.h>

#ifndef UNIT_TEST

#include "infrastructure/ocpp/OCPPClient.h"
#include "infrastructure/ocpp/websocket/ArduinoWebSocketClient.h"
#include "infrastructure/hardware/ESP32Hardware.h"
#include "infrastructure/storage/InMemoryRepositories.h"
#include "infrastructure/storage/LittleFSFileSystem.h"
#include "infrastructure/storage/LittleFSConfigRepository.h"
#include "infrastructure/storage/LittleFSTransactionRepository.h"
#include "infrastructure/time/SystemClock.h"
#include "infrastructure/util/SimpleIdGenerator.h"
#include "../core/application/usecases/OCPPUseCases.h"

using Infrastructure::ArduinoWebSocketClient;
using Infrastructure::ESP32Hardware;
using Infrastructure::InMemoryConfigRepository;
using Infrastructure::InMemoryTransactionRepository;
using Infrastructure::LittleFSConfigRepository;
using Infrastructure::LittleFSFileSystem;
using Infrastructure::LittleFSTransactionRepository;
using Infrastructure::OCPPClient;
using Infrastructure::SimpleIdGenerator;
using Infrastructure::SystemClock;
using Core::Application::UseCaseFactory;
using Core::Domain::IConfigRepository;
using Core::Domain::ITransactionRepository;

std::unique_ptr<ArduinoWebSocketClient> wsClient;
std::unique_ptr<IConfigRepository> configRepo;
std::unique_ptr<ITransactionRepository> transactionRepo;
std::unique_ptr<LittleFSFileSystem> fileSystem;
std::unique_ptr<ESP32Hardware> hardware;
std::unique_ptr<SystemClock> clockSource;
std::unique_ptr<SimpleIdGenerator> idGenerator;
std::unique_ptr<UseCaseFactory> useCaseFactory;
std::unique_ptr<OCPPClient> ocppClient;

void setup() {
    Serial.begin(115200);
    delay(1000);

    fileSystem = std::make_unique<LittleFSFileSystem>();
    if (fileSystem->initialize()) {
        configRepo = std::make_unique<LittleFSConfigRepository>(fileSystem.get(), "");
        transactionRepo = std::make_unique<LittleFSTransactionRepository>(fileSystem.get(), "");
        Serial.println("LittleFS initialized - using persistent repositories");
    } else {
        configRepo = std::make_unique<InMemoryConfigRepository>();
        transactionRepo = std::make_unique<InMemoryTransactionRepository>();
        Serial.println("LittleFS init failed - using in-memory repositories");
    }

    hardware = std::make_unique<ESP32Hardware>();
    clockSource = std::make_unique<SystemClock>();
    idGenerator = std::make_unique<SimpleIdGenerator>(1000);

    useCaseFactory = std::make_unique<UseCaseFactory>(
        transactionRepo.get(),
        hardware.get(),
        configRepo.get(),
        nullptr,
        idGenerator.get(),
        clockSource.get());

    wsClient = std::make_unique<ArduinoWebSocketClient>();

    ocppClient = std::make_unique<OCPPClient>(
        std::move(wsClient),
        configRepo.get(),
        transactionRepo.get(),
        hardware.get(),
        nullptr,
        useCaseFactory.get());

    Serial.println("ESP32 OCPP Charging Station - bootstrap");
}

void loop() {
    if (ocppClient) {
        ocppClient->loop();
    }

    if (hardware) {
        hardware->loop();
    }

    delay(100);
}

#endif
