/**
 * SeedJoy Configuration Management
 * 
 * Defines configuration structure and provides helper functions
 */

class SeedJoyConfig {
    constructor() {
        this.config = this.getDefaultConfig();
    }
    
    /**
     * Get default configuration
     */
    getDefaultConfig() {
        return {
            magic: 0x5EED70B7,
            configVersion: 1,
            firmwareMajor: 0,
            firmwareMinor: 1,
            firmwarePatch: 0,
            deviceName: 'SeedJoy',
            mode: 0, // 0=USB, 1=BLE, 2=AUTO
            
            // USB settings
            usbVID: 0x239A,
            usbPID: 0x80F4,
            usbPollRate: 1,
            
            // BLE settings
            bleConnInterval: 6,  // 1.25 ms units; 6 = 7.5 ms (BLE minimum)
            bleTxPower: 0,
            
            // Axes (4 axes)
            axes: [
                this.getDefaultAxis(0, 0),  // A0 / D0 / P0.02
                this.getDefaultAxis(1, 1),  // A1 / D1 / P0.03
                this.getDefaultAxis(2, 2),  // A2 / D2 / P0.28
                this.getDefaultAxis(3, 3)   // A3 / D3 / P0.29
            ],
            
            // Buttons (16 buttons)
            buttons: this.getDefaultButtons(),
            
            // Shift registers (74HC165)
            shiftRegisters: {
                enabled: true,
                numChips: 7,
                dataPin: 6,   // D6 / P1.11 (TX) - default data pin
                clockPin: 7,  // D7 / P1.12 (RX) - default clock pin
                loadPin: 8,   // D8 / P1.13 (SCK) - default load pin
                inverted: true   // pull-up + switch to GND: LOW = pressed
            },
            
            // Power management
            autoSleep: false,
            sleepTimeout: 600,
            
            crc32: 0
        };
    }
    
    /**
     * Get default axis configuration
     */
    getDefaultAxis(index, pin) {
        return {
            pin: pin,
            enabled: false,  // SAFETY: disabled by default (user must enable)
            inverted: false,
            min: 0,
            center: 2048,
            max: 4095,
            deadzone: 5,
            curveType: 0, // 0=linear, 1=expo, 2=custom
            expoFactor: 0.0,
            customCurve: [0.0, 0.25, 0.5, 0.75, 1.0],
            smoothing: 0
        };
    }
    
    /**
     * Get default button configurations
     * Note: Most buttons disabled - primary use case is shift registers
     */
    getDefaultButtons() {
        const buttonPins = [
            4,   // D4 / P0.04 (SDA)
            5,   // D5 / P0.05 (SCL)
            10,  // D10 / P1.15 (MOSI)
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Unused (reserved for shift registers)
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF
        ];
        
        return buttonPins.map((pin, index) => ({
            pin: pin,
            enabled: false,  // SAFETY: disabled by default (user must enable)
            logicalNumber: index,
            inverted: false
        }));
    }
    
    /**
     * Load configuration from device (via BLE)
     */
    async loadFromDevice(ble) {
        try {
            const config = await ble.readConfig();
            this.config = config;
            return config;
        } catch (error) {
            console.error('Failed to load config from device:', error);
            throw error;
        }
    }
    
    /**
     * Save configuration to device (via BLE)
     */
    async saveToDevice(ble) {
        try {
            // Calculate CRC before saving
            this.config.crc32 = this.calculateCRC32();
            await ble.writeConfig(this.config);
            return true;
        } catch (error) {
            console.error('Failed to save config to device:', error);
            throw error;
        }
    }
    
    /**
     * Export configuration as JSON file
     */
    exportToFile() {
        const json = JSON.stringify(this.config, null, 2);
        const blob = new Blob([json], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        
        const a = document.createElement('a');
        a.href = url;
        a.download = `seedjoy_config_${Date.now()}.json`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }
    
    /**
     * Import configuration from JSON file
     */
    async importFromFile(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => {
                try {
                    const config = JSON.parse(e.target.result);
                    if (this.validateConfig(config)) {
                        this.config = config;
                        resolve(config);
                    } else {
                        reject(new Error('Invalid configuration file'));
                    }
                } catch (error) {
                    reject(error);
                }
            };
            reader.onerror = reject;
            reader.readAsText(file);
        });
    }
    
    /**
     * Validate configuration structure
     */
    validateConfig(config) {
        if (!config || typeof config !== 'object') return false;
        if (config.magic !== 0x5EED70B7) return false;
        if (config.configVersion !== 1) return false;
        if (!Array.isArray(config.axes) || config.axes.length !== 4) return false;
        if (!Array.isArray(config.buttons) || config.buttons.length !== 16) return false;
        return true;
    }
    
    /**
     * Reset to default configuration
     */
    resetToDefaults() {
        this.config = this.getDefaultConfig();
        return this.config;
    }
    
    /**
     * Get current configuration
     */
    getConfig() {
        return this.config;
    }
    
    /**
     * Update axis configuration
     */
    updateAxis(index, updates) {
        if (index >= 0 && index < this.config.axes.length) {
            this.config.axes[index] = { ...this.config.axes[index], ...updates };
        }
    }
    
    /**
     * Update button configuration
     */
    updateButton(index, updates) {
        if (index >= 0 && index < this.config.buttons.length) {
            this.config.buttons[index] = { ...this.config.buttons[index], ...updates };
        }
    }
    
    /**
     * Update device settings
     */
    updateDeviceSettings(updates) {
        this.config = { ...this.config, ...updates };
    }
    
    /**
     * Update shift register configuration
     */
    updateShiftRegisters(updates) {
        this.config.shiftRegisters = { ...this.config.shiftRegisters, ...updates };
    }
    
    /**
     * Calculate CRC32 (simple implementation)
     * In production, should match firmware's CRC calculation
     */
    calculateCRC32(str) {
        // Standard CRC32 (poly 0xEDB88320) over UTF-8 bytes
        // Note: firmware ignores incoming crc32 on JSON writes and recalculates
        // over the binary struct, so this is for client-side validation only.
        let crc = 0xFFFFFFFF;
        const bytes = new TextEncoder().encode(str);
        for (let i = 0; i < bytes.length; i++) {
            crc ^= bytes[i];
            for (let j = 0; j < 8; j++) {
                crc = (crc & 1) ? ((crc >>> 1) ^ 0xEDB88320) : (crc >>> 1);
            }
        }
        return (crc ^ 0xFFFFFFFF) >>> 0;
    }
    
    /**
     * Get pin name from pin number
     */
    static getPinName(pinNum) {
        const pin = SeedJoyConfig.getAvailablePins().find(p => p.value === pinNum);
        return pin ? pin.label : `Pin ${pinNum}`;
    }
    
    /**
     * Get all available pins
     */
    static getAvailablePins() {
        // Values are Arduino pin numbers (D0..D10 = 0..10), exactly what the
        // firmware passes to pinMode()/digitalRead()/analogRead() on the
        // Seeed XIAO nRF52840 core. Do NOT use nRF GPIO (P0.xx/P1.xx) numbers.
        // Reference: https://wiki.seeedstudio.com/XIAO_BLE/
        return [
            { value: 0,  label: 'A0/D0 (P0.02)' },
            { value: 1,  label: 'A1/D1 (P0.03)' },
            { value: 2,  label: 'A2/D2 (P0.28)' },
            { value: 3,  label: 'A3/D3 (P0.29)' },
            { value: 4,  label: 'D4/SDA (P0.04)' },
            { value: 5,  label: 'D5/SCL (P0.05)' },
            { value: 6,  label: 'D6/TX (P1.11)' },
            { value: 7,  label: 'D7/RX (P1.12)' },
            { value: 8,  label: 'D8/SCK (P1.13)' },
            { value: 9,  label: 'D9/MISO (P1.14) - mode select' },
            { value: 10, label: 'D10/MOSI (P1.15)' },
            { value: 0xFF, label: 'Not Used' }
        ];
    }
}

// Export for use in other modules
window.SeedJoyConfig = SeedJoyConfig;
