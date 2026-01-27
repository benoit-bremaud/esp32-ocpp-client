# Git Repository Setup Guide

## 1. Initialize Git Repository (if not already done)

```bash
git init
git add .
git commit -m "Initial commit: ESP32 OCPP Client with security profiles and core message handlers"
```

## 2. Create GitHub Repository

1. Go to https://github.com/benoit-bremaud/
2. Click "New repository"
3. Repository name: `esp32-ocpp-client`
4. Set as **Private**
5. Don't initialize with README (we already have one)
6. Click "Create repository"

## 3. Connect Local Repository to GitHub

```bash
git remote add origin https://github.com/benoit-bremaud/esp32-ocpp-client.git
git branch -M main
git push -u origin main
```

## 4. Branching Strategy for Infrastructure Implementation

### Feature Branches to Create:

- `feature/wifi-manager` - WiFi connection management
- `feature/storage-layer` - LittleFS repositories and persistence
- `feature/hardware-controller` - ESP32 hardware abstraction
- `feature/certificate-manager` - Certificate management implementation
- `feature/application-services` - Application layer services

### Commands for Each Feature:

```bash
# Start new feature
git checkout main
git pull origin main
git checkout -b feature/wifi-manager

# After implementation
git add .
git commit -m "feat: implement WiFi manager with connection handling"
git push -u origin feature/wifi-manager

# Create pull request on GitHub, then merge and cleanup
git checkout main
git pull origin main
git branch -d feature/wifi-manager
```

## 5. Ready for Next Steps

Once the repository is set up, we'll implement each infrastructure component on separate branches as requested.