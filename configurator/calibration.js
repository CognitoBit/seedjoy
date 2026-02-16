/**
 * SeedJoy Calibration Module
 * 
 * Handles live axis preview and calibration wizard
 */

class CalibrationManager {
    constructor(ble, config) {
        this.ble = ble;
        this.config = config;
        this.currentAxis = 0;
        this.isMonitoring = false;
        this.monitoringInterval = null;
        this.latestAxisData = null;
        
        // Mock axis values for testing when not connected
        this.mockAxisValues = [2048, 2048, 2048, 2048];
        
        // Set up real-time axis data callback
        this.ble.onAxisData = (data) => {
            this.latestAxisData = data;
        };
    }
    
    /**
     * Start monitoring axis values
     */
    startMonitoring() {
        if (this.isMonitoring) return;
        
        this.isMonitoring = true;
        this.monitoringInterval = setInterval(() => {
            this.updateAxisPreview();
        }, 50); // Update at 20Hz
    }
    
    /**
     * Stop monitoring axis values
     */
    stopMonitoring() {
        this.isMonitoring = false;
        if (this.monitoringInterval) {
            clearInterval(this.monitoringInterval);
            this.monitoringInterval = null;
        }
    }
    
    /**
     * Update axis preview display
     */
    async updateAxisPreview() {
        try {
            // Get current axis values
            let rawValue, processedValue;
            
            if (this.ble.isConnected() && this.latestAxisData) {
                // Use real-time data from device
                rawValue = this.latestAxisData.raw[this.currentAxis];
                processedValue = this.latestAxisData.processed[this.currentAxis];
            } else {
                // Simulate axis movement when disconnected
                this.mockAxisValues[this.currentAxis] = this.simulateAxisMovement();
                rawValue = this.mockAxisValues[this.currentAxis];
                processedValue = this.processAxisValue(rawValue);
            }
            
            // Update UI
            this.updateAxisBars(rawValue, processedValue);
            
        } catch (error) {
            console.warn('Failed to read axis value:', error);
        }
    }
    
    /**
     * Process raw axis value with calibration
     */
    processAxisValue(raw) {
        const cfg = this.config.getConfig();
        const axisCfg = cfg.axes[this.currentAxis];
        
        // Normalize (0.0 to 1.0)
        let normalized;
        if (raw <= axisCfg.center) {
            // Lower half
            if (axisCfg.center === axisCfg.min) {
                normalized = 0.5;
            } else {
                normalized = 0.5 * (raw - axisCfg.min) / (axisCfg.center - axisCfg.min);
            }
        } else {
            // Upper half
            if (axisCfg.max === axisCfg.center) {
                normalized = 0.5;
            } else {
                normalized = 0.5 + 0.5 * (raw - axisCfg.center) / (axisCfg.max - axisCfg.center);
            }
        }
        
        // Clamp
        normalized = Math.max(0, Math.min(1, normalized));
        
        // Apply deadzone
        normalized = this.applyDeadzone(normalized, axisCfg.deadzone);
        
        // Apply curve
        normalized = this.applyCurve(normalized, axisCfg);
        
        // Scale to -32767 to 32767
        let processed = Math.round(normalized * 65534 - 32767);
        
        if (axisCfg.inverted) {
            processed = -processed;
        }
        
        return processed;
    }
    
    /**
     * Apply deadzone
     */
    applyDeadzone(normalized, deadzonePercent) {
        const deadzone = deadzonePercent / 100 / 2; // Half on each side of center
        const center = 0.5;
        const distance = normalized - center;
        
        if (Math.abs(distance) < deadzone) {
            return center;
        }
        
        if (distance > 0) {
            return center + (distance - deadzone) * (0.5 / (0.5 - deadzone));
        } else {
            return center + (distance + deadzone) * (0.5 / (0.5 - deadzone));
        }
    }
    
    /**
     * Apply curve
     */
    applyCurve(input, axisCfg) {
        switch (axisCfg.curveType) {
            case 0: // Linear
                return input;
                
            case 1: // Exponential
                {
                    const centered = (input - 0.5) * 2; // -1 to 1
                    const sign = centered >= 0 ? 1 : -1;
                    const expo = axisCfg.expoFactor;
                    const curved = sign * Math.pow(Math.abs(centered), 1 + expo);
                    return (curved + 1) / 2;
                }
                
            case 2: // Custom
                {
                    const curve = axisCfg.customCurve;
                    const scaledInput = input * (curve.length - 1);
                    const idx = Math.floor(scaledInput);
                    if (idx >= curve.length - 1) return curve[curve.length - 1];
                    
                    const fraction = scaledInput - idx;
                    return curve[idx] + (curve[idx + 1] - curve[idx]) * fraction;
                }
                
            default:
                return input;
        }
    }
    
    /**
     * Simulate axis movement (for testing without hardware)
     */
    simulateAxisMovement() {
        // Simple sine wave simulation
        const time = Date.now() / 1000;
        const value = Math.sin(time * this.currentAxis + 1) * 0.8 + 0.5;
        return Math.round(value * 4095);
    }
    
    /**
     * Update axis bar displays
     */
    updateAxisBars(rawValue, processedValue) {
        // Raw value bar (0-4095)
        const rawPercent = (rawValue / 4095) * 100;
        const rawBar = document.getElementById('raw-value-bar');
        const rawText = document.getElementById('raw-value-text');
        if (rawBar) rawBar.style.width = `${rawPercent}%`;
        if (rawText) rawText.textContent = rawValue;
        
        // Processed value bar (-32767 to 32767)
        const processedPercent = ((processedValue + 32767) / 65534) * 100;
        const processedBar = document.getElementById('processed-value-bar');
        const processedText = document.getElementById('processed-value-text');
        if (processedBar) processedBar.style.width = `${processedPercent}%`;
        if (processedText) processedText.textContent = processedValue;
    }
    
    /**
     * Set current axis for calibration
     */
    setCurrentAxis(axisIndex) {
        this.currentAxis = axisIndex;
    }
    
    /**
     * Calibrate minimum
     */
    async calibrateMin() {
        try {
            if (this.ble.isConnected() && this.ble.calibrateCharacteristic) {
                // Use BLE calibration service (command 0 = CAL_MIN)
                await this.ble.calibrate(this.currentAxis, 0);
                this.showCalibrationFeedback('Minimum calibrated on device!');
            } else {
                // Offline mode - update local config only
                const rawValue = await this.getCurrentRawValue();
                this.config.updateAxis(this.currentAxis, { min: rawValue });
                console.log(`Axis ${this.currentAxis} min set to ${rawValue}`);
                this.showCalibrationFeedback('Minimum calibrated locally!');
            }
        } catch (error) {
            console.error('Calibrate min error:', error);
            this.showCalibrationFeedback('Failed to calibrate minimum', true);
        }
    }
    
    /**
     * Calibrate center
     */
    async calibrateCenter() {
        try {
            if (this.ble.isConnected() && this.ble.calibrateCharacteristic) {
                // Use BLE calibration service (command 1 = CAL_CENTER)
                await this.ble.calibrate(this.currentAxis, 1);
                this.showCalibrationFeedback('Center calibrated on device!');
            } else {
                // Offline mode - update local config only
                const rawValue = await this.getCurrentRawValue();
                this.config.updateAxis(this.currentAxis, { center: rawValue });
                console.log(`Axis ${this.currentAxis} center set to ${rawValue}`);
                this.showCalibrationFeedback('Center calibrated locally!');
            }
        } catch (error) {
            console.error('Calibrate center error:', error);
            this.showCalibrationFeedback('Failed to calibrate center', true);
        }
    }
    
    /**
     * Calibrate maximum
     */
    async calibrateMax() {
        try {
            if (this.ble.isConnected() && this.ble.calibrateCharacteristic) {
                // Use BLE calibration service (command 2 = CAL_MAX)
                await this.ble.calibrate(this.currentAxis, 2);
                this.showCalibrationFeedback('Maximum calibrated on device!');
            } else {
                // Offline mode - update local config only
                const rawValue = await this.getCurrentRawValue();
                this.config.updateAxis(this.currentAxis, { max: rawValue });
                console.log(`Axis ${this.currentAxis} max set to ${rawValue}`);
                this.showCalibrationFeedback('Maximum calibrated locally!');
            }
        } catch (error) {
            console.error('Calibrate max error:', error);
            this.showCalibrationFeedback('Failed to calibrate maximum', true);
        }
    }
    
    /**
     * Save calibration to device Flash
     */
    async saveCalibration() {
        try {
            if (this.ble.isConnected() && this.ble.calibrateCharacteristic) {
                // Use BLE calibration service (command 3 = CAL_SAVE)
                await this.ble.calibrate(this.currentAxis, 3);
                this.showCalibrationFeedback('Calibration saved to Flash!');
            } else {
                this.showCalibrationFeedback('Not connected - calibration not saved', true);
            }
        } catch (error) {
            console.error('Save calibration error:', error);
            this.showCalibrationFeedback('Failed to save calibration', true);
        }
    }
    
    /**
     * Get current raw axis value
     */
    async getCurrentRawValue() {
        if (this.ble.isConnected() && this.latestAxisData) {
            // Use real-time data from device
            return this.latestAxisData.raw[this.currentAxis];
        } else {
            return this.mockAxisValues[this.currentAxis];
        }
    }
    
    /**
     * Show calibration feedback
     */
    showCalibrationFeedback(message) {
        // Simple alert for MVP - c, isError = false) {
        // Simple alert for MVP - could be improved with toast notifications
        const feedback = document.createElement('div');
        feedback.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: ${isError ? '#F44336' : '#4CAF50'}
            padding: 15px 20px;
            border-radius: 5px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
            z-index: 1000;
            animation: slideIn 0.3s ease-out;
        `;
        feedback.textContent = message;
        document.body.appendChild(feedback);
        
        setTimeout(() => {
            feedback.style.animation = 'slideOut 0.3s ease-out';
            setTimeout(() => feedback.remove(), 300);
        }, 2000);
    }
}

// Export for use in other modules
window.CalibrationManager = CalibrationManager;
