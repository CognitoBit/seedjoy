/**
 * SeedJoy Main Application
 * 
 * Handles UI initialization and event handling
 */

let ble = null;
let serial = null;
let activeConnection = null;
let connectionMode = 'bluetooth'; // 'bluetooth' or 'serial'
let config = null;
let calibration = null;

// Initialize application
document.addEventListener('DOMContentLoaded', () => {
    console.log('SeedJoy Configurator starting...');
    
    // Initialize modules
    ble = new SeedJoyBLE();
    serial = new SeedJoySerial();
    config = new SeedJoyConfig();
    
    // Set active connection based on mode
    activeConnection = ble;
    
    // Create calibration manager (will use activeConnection)
    calibration = new CalibrationManager(activeConnection, config);
    
    // Set up connection callbacks for both connection types
    setupConnectionCallbacks(ble);
    setupConnectionCallbacks(serial);
    
    // Initialize UI
    initializeUI();
    initializeConnectionMode();
    initializeTabs();
    initializePinConfiguration();
    initializeAxisCalibration();
    initializeButtonMapping();
    initializeAdvancedSettings();
    initializeActionButtons();
    
    // Load config from localStorage if available
    loadLocalConfig();
    
    console.log('SeedJoy Configurator ready!');
});

/**
 * Set up connection callbacks for a connection object
 */
function setupConnectionCallbacks(conn) {
    conn.onConnectionChange = handleConnectionChange;
    conn.onBatteryChange = handleBatteryChange;
    conn.onButtonData = handleButtonData;
}

/**
 * Initialize connection mode switcher
 */
function initializeConnectionMode() {
    const modeBtns = document.querySelectorAll('.mode-btn');
    const modeHint = document.getElementById('mode-hint');
    
    modeBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const mode = btn.getAttribute('data-mode');
            
            // Don't switch if already connected
            if (activeConnection && activeConnection.connected) {
                alert('Please disconnect before switching connection mode');
                return;
            }
            
            // Update mode
            connectionMode = mode;
            
            // Update UI
            modeBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            
            // Update active connection
            if (mode === 'bluetooth') {
                activeConnection = ble;
                calibration.ble = ble;
                modeHint.textContent = 'WebBluetooth - wireless connection (Chrome/Edge only)';
            } else {
                activeConnection = serial;
                calibration.ble = serial;
                modeHint.textContent = 'WebSerial - USB connection (Chrome/Edge 89+)';
            }
            
            console.log('Connection mode:', mode);
        });
    });
}

/**
 * Initialize UI components
 */
function initializeUI() {
    // Check WebBluetooth support
    if (!navigator.bluetooth) {
        alert('WebBluetooth is not supported in this browser. Please use Chrome, Edge, or Opera.');
        document.getElementById('connect-btn').disabled = true;
    }
}

/**
 * Initialize tab switching
 */
function initializeTabs() {
    const tabButtons = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');
    
    tabButtons.forEach(button => {
        button.addEventListener('click', () => {
            const tabName = button.getAttribute('data-tab');
            
            // Update tab buttons
            tabButtons.forEach(btn => btn.classList.remove('active'));
            button.classList.add('active');
            
            // Update tab contents
            tabContents.forEach(content => content.classList.remove('active'));
            document.getElementById(`tab-${tabName}`).classList.add('active');
            
            // Start/stop calibration monitoring based on active tab
            if (tabName === 'axes') {
                calibration.startMonitoring();
            } else {
                calibration.stopMonitoring();
            }

            // Start/stop button streaming on the button-test tab (serial path only)
            if (activeConnection && typeof activeConnection.startButtonStream === 'function') {
                if (tabName === 'buttons') {
                    activeConnection.startButtonStream().catch(() => {});
                } else {
                    activeConnection.stopButtonStream().catch(() => {});
                }
            }
        });
    });
}

/**
 * Initialize pin configuration UI
 */
function initializePinConfiguration() {
    const axisPins = document.getElementById('axis-pins');
    const buttonPins = document.getElementById('button-pins');
    const availablePins = SeedJoyConfig.getAvailablePins();
    
    // Generate axis pin selectors
    for (let i = 0; i < 4; i++) {
        const cfg = config.getConfig().axes[i];
        const div = createPinSelector(`Axis ${i}`, `axis-pin-${i}`, `axis-enable-${i}`, availablePins, cfg.pin, cfg.enabled, 'axis', i);
        axisPins.appendChild(div);
    }
    
    // Generate button pin selectors
    for (let i = 0; i < 16; i++) {
        const cfg = config.getConfig().buttons[i];
        const div = createPinSelector(`Button ${i}`, `button-pin-${i}`, `button-enable-${i}`, availablePins, cfg.pin, cfg.enabled, 'button', i);
        buttonPins.appendChild(div);
    }
    
    // Bulk enable/disable buttons
    document.getElementById('enable-all-axes').addEventListener('click', () => {
        for (let i = 0; i < 4; i++) {
            document.getElementById(`axis-enable-${i}`).checked = true;
            config.updateAxis(i, { enabled: true });
        }
    });
    
    document.getElementById('disable-all-axes').addEventListener('click', () => {
        for (let i = 0; i < 4; i++) {
            document.getElementById(`axis-enable-${i}`).checked = false;
            config.updateAxis(i, { enabled: false });
        }
    });
    
    document.getElementById('enable-all-buttons').addEventListener('click', () => {
        for (let i = 0; i < 16; i++) {
            document.getElementById(`button-enable-${i}`).checked = true;
            config.updateButton(i, { enabled: true });
        }
    });
    
    document.getElementById('disable-all-buttons').addEventListener('click', () => {
        for (let i = 0; i < 16; i++) {
            document.getElementById(`button-enable-${i}`).checked = false;
            config.updateButton(i, { enabled: false });
        }
    });
}

/**
 * Create pin selector UI element with enable checkbox
 */
function createPinSelector(label, pinId, enableId, options, selectedPin, enabled, type, index) {
    const div = document.createElement('div');
    div.className = 'pin-selector';
    
    // Enable checkbox
    const enableCheckbox = document.createElement('input');
    enableCheckbox.type = 'checkbox';
    enableCheckbox.id = enableId;
    enableCheckbox.checked = enabled;
    enableCheckbox.addEventListener('change', (e) => {
        if (type === 'axis') {
            config.updateAxis(index, { enabled: e.target.checked });
        } else if (type === 'button') {
            config.updateButton(index, { enabled: e.target.checked });
        }
    });
    
    const enableLabel = document.createElement('label');
    enableLabel.className = 'checkbox-label';
    enableLabel.appendChild(enableCheckbox);
    enableLabel.appendChild(document.createTextNode(label));
    
    // Pin selector
    const select = document.createElement('select');
    select.id = pinId;
    select.disabled = !enabled;
    
    options.forEach(opt => {
        const option = document.createElement('option');
        option.value = opt.value;
        option.textContent = opt.label;
        if (opt.value === selectedPin) {
            option.selected = true;
        }
        select.appendChild(option);
    });
    
    // Enable/disable pin selector based on checkbox
    enableCheckbox.addEventListener('change', (e) => {
        select.disabled = !e.target.checked;
    });
    
    // Update config when pin changes
    select.addEventListener('change', (e) => {
        if (type === 'axis') {
            config.updateAxis(index, { pin: parseInt(e.target.value) });
        } else if (type === 'button') {
            config.updateButton(index, { pin: parseInt(e.target.value) });
        }
    });
    
    div.appendChild(enableLabel);
    div.appendChild(select);
    
    return div;
}

/**
 * Populate a pin dropdown with available pins
 */
function populatePinDropdown(elementId, pins, selectedValue) {
    const select = document.getElementById(elementId);
    // Clear existing options except the first one (if any)
    select.innerHTML = '';
    
    pins.forEach(pin => {
        const option = document.createElement('option');
        option.value = pin.value;
        option.textContent = pin.label;
        if (pin.value === selectedValue) {
            option.selected = true;
        }
        select.appendChild(option);
    });
}

/**
 * Initialize axis calibration UI
 */
function initializeAxisCalibration() {
    // Axis selector
    const axisSelect = document.getElementById('calibration-axis');
    axisSelect.addEventListener('change', (e) => {
        calibration.setCurrentAxis(parseInt(e.target.value));
    });
    
    // Calibration buttons
    document.getElementById('calibrate-min-btn').addEventListener('click', () => {
        calibration.calibrateMin();
    });
    
    document.getElementById('calibrate-center-btn').addEventListener('click', () => {
        calibration.calibrateCenter();
    });
    
    document.getElementById('calibrate-max-btn').addEventListener('click', () => {
        calibration.calibrateMax();
    });
    
    // Axis settings
    document.getElementById('axis-inverted').addEventListener('change', (e) => {
        const axisIndex = parseInt(document.getElementById('calibration-axis').value);
        config.updateAxis(axisIndex, { inverted: e.target.checked });
    });
    
    const deadzoneSlider = document.getElementById('deadzone-slider');
    deadzoneSlider.addEventListener('input', (e) => {
        document.getElementById('deadzone-value').textContent = e.target.value;
        const axisIndex = parseInt(document.getElementById('calibration-axis').value);
        config.updateAxis(axisIndex, { deadzone: parseInt(e.target.value) });
    });
    
    const curveType = document.getElementById('curve-type');
    curveType.addEventListener('change', (e) => {
        const type = parseInt(e.target.value);
        const axisIndex = parseInt(document.getElementById('calibration-axis').value);
        config.updateAxis(axisIndex, { curveType: type });
        
        // Show/hide expo setting
        document.getElementById('expo-setting').style.display = type === 1 ? 'block' : 'none';
    });
    
    const expoSlider = document.getElementById('expo-slider');
    expoSlider.addEventListener('input', (e) => {
        const value = e.target.value / 100;
        document.getElementById('expo-value').textContent = value.toFixed(2);
        const axisIndex = parseInt(document.getElementById('calibration-axis').value);
        config.updateAxis(axisIndex, { expoFactor: value });
    });
    
    const smoothing = document.getElementById('smoothing-select');
    smoothing.addEventListener('change', (e) => {
        const axisIndex = parseInt(document.getElementById('calibration-axis').value);
        config.updateAxis(axisIndex, { smoothing: parseInt(e.target.value) });
    });
}

/**
 * Initialize button mapping UI
 */
function initializeButtonMapping() {
    const buttonGrid = document.querySelector('.button-grid');
    const testGrid = document.getElementById('button-test-grid');
    
    // Generate button mapping UI
    for (let i = 0; i < 16; i++) {
        const item = document.createElement('div');
        item.className = 'button-item';
        item.innerHTML = `
            <h4>Button ${i}</h4>
            <label>
                Logical Number:
                <select id="button-logical-${i}">
                    ${Array.from({length: 16}, (_, j) => 
                        `<option value="${j}" ${j === i ? 'selected' : ''}>${j}</option>`
                    ).join('')}
                </select>
            </label>
            <label>
                <input type="checkbox" id="button-invert-${i}"> Invert
            </label>
        `;
        buttonGrid.appendChild(item);
    }
    
    // Generate button test indicators (56 = 7 chips × 8)
    for (let i = 0; i < 56; i++) {
        const indicator = document.createElement('div');
        indicator.className = 'button-indicator';
        indicator.id = `button-indicator-${i}`;
        indicator.textContent = i;
        testGrid.appendChild(indicator);
    }
}

/**
 * Initialize advanced settings UI
 */
function initializeAdvancedSettings() {
    const cfg = config.getConfig();
    
    // Device settings
    document.getElementById('device-name-input').value = cfg.deviceName;
    document.getElementById('mode-select').value = cfg.mode;
    
    // USB settings
    document.getElementById('usb-poll-rate').value = cfg.usbPollRate;
    
    // BLE settings
    document.getElementById('ble-interval').value = cfg.bleConnInterval;
    document.getElementById('ble-tx-power').value = cfg.bleTxPower;
    
    // Power management
    document.getElementById('auto-sleep').checked = cfg.autoSleep;
    document.getElementById('sleep-timeout').value = cfg.sleepTimeout;
    
    // Shift registers
    document.getElementById('shift-reg-enabled').checked = cfg.shiftRegisters.enabled;
    document.getElementById('shift-reg-chips').value = cfg.shiftRegisters.numChips;
    document.getElementById('shift-reg-data-pin').value = cfg.shiftRegisters.dataPin;
    document.getElementById('shift-reg-clock-pin').value = cfg.shiftRegisters.clockPin;
    document.getElementById('shift-reg-load-pin').value = cfg.shiftRegisters.loadPin;
    document.getElementById('shift-reg-inverted').checked = cfg.shiftRegisters.inverted;
    
    // Populate shift register pin selectors
    const availablePins = SeedJoyConfig.getAvailablePins();
    populatePinDropdown('shift-reg-data-pin', availablePins, cfg.shiftRegisters.dataPin);
    populatePinDropdown('shift-reg-clock-pin', availablePins, cfg.shiftRegisters.clockPin);
    populatePinDropdown('shift-reg-load-pin', availablePins, cfg.shiftRegisters.loadPin);
    
    // Add change listeners
    document.getElementById('device-name-input').addEventListener('change', (e) => {
        config.updateDeviceSettings({ deviceName: e.target.value });
    });
    
    document.getElementById('mode-select').addEventListener('change', (e) => {
        config.updateDeviceSettings({ mode: parseInt(e.target.value) });
    });
    
    document.getElementById('usb-poll-rate').addEventListener('change', (e) => {
        config.updateDeviceSettings({ usbPollRate: parseInt(e.target.value) });
    });
    
    document.getElementById('ble-interval').addEventListener('change', (e) => {
        config.updateDeviceSettings({ bleConnInterval: parseInt(e.target.value) });
    });
    
    document.getElementById('ble-tx-power').addEventListener('change', (e) => {
        config.updateDeviceSettings({ bleTxPower: parseInt(e.target.value) });
    });
    
    document.getElementById('auto-sleep').addEventListener('change', (e) => {
        config.updateDeviceSettings({ autoSleep: e.target.checked });
    });
    
    document.getElementById('sleep-timeout').addEventListener('change', (e) => {
        config.updateDeviceSettings({ sleepTimeout: parseInt(e.target.value) });
    });
    
    // Shift register event listeners
    document.getElementById('shift-reg-enabled').addEventListener('change', (e) => {
        config.updateShiftRegisters({ enabled: e.target.checked });
    });
    
    document.getElementById('shift-reg-chips').addEventListener('change', (e) => {
        config.updateShiftRegisters({ numChips: parseInt(e.target.value) });
    });
    
    document.getElementById('shift-reg-data-pin').addEventListener('change', (e) => {
        config.updateShiftRegisters({ dataPin: parseInt(e.target.value) });
    });
    
    document.getElementById('shift-reg-clock-pin').addEventListener('change', (e) => {
        config.updateShiftRegisters({ clockPin: parseInt(e.target.value) });
    });
    
    document.getElementById('shift-reg-load-pin').addEventListener('change', (e) => {
        config.updateShiftRegisters({ loadPin: parseInt(e.target.value) });
    });
    
    document.getElementById('shift-reg-inverted').addEventListener('change', (e) => {
        config.updateShiftRegisters({ inverted: e.target.checked });
    });
}

/**
 * Initialize action buttons
 */
function initializeActionButtons() {
    // Connect/Disconnect
    document.getElementById('connect-btn').addEventListener('click', async () => {
        try {
            await activeConnection.connect();
        } catch (error) {
            alert('Failed to connect: ' + error.message);
        }
    });
    
    document.getElementById('disconnect-btn').addEventListener('click', async () => {
        await activeConnection.disconnect();
    });
    
    // Read/Write config
    document.getElementById('read-config-btn').addEventListener('click', async () => {
        try {
            // Check if config is available (different for BLE vs Serial)
            if (connectionMode === 'bluetooth' && !ble.configReadCharacteristic) {
                alert('Configuration service not available.\n\n' +
                      'Please make sure you have uploaded the latest firmware with BLE config service support.');
                return;
            }
            
            showNotification('Reading Configuration', 'Downloading config from device...', 'info', 0);
            
            const deviceConfig = await activeConnection.readConfig();
            config.config = deviceConfig;
            updateUIFromConfig();
            
            showNotification('Success', 'Configuration loaded from device', 'success', 3000);
        } catch (error) {
            showNotification('Error', 'Failed to read config: ' + error.message, 'error', 5000);
            console.error('Read config error:', error);
        }
    });
    
    document.getElementById('write-config-btn').addEventListener('click', async () => {
        // Check if config is available (different for BLE vs Serial)
        if (connectionMode === 'bluetooth' && !ble.configWriteCharacteristic) {
            alert('Configuration service not available.\n\n' +
                  'Please make sure you have uploaded the latest firmware with BLE config service support.');
            return;
        }
        
        if (confirm('Write configuration to device? This will overwrite existing settings and save to Flash.')) {
            try {
                showNotification('Writing Configuration', 'Uploading config to device...', 'info', 0);
                
                await activeConnection.writeConfig(config.getConfig());
                
                showNotification('Success', 'Configuration written to device and saved to Flash', 'success', 3000);
            } catch (error) {
                showNotification('Error', 'Failed to write config: ' + error.message, 'error', 5000);
                console.error('Write config error:', error);
            }
        }
    });
    
    // Reset to defaults
    document.getElementById('reset-config-btn').addEventListener('click', () => {
        if (confirm('Reset to default configuration? This will discard all your settings.')) {
            config.resetToDefaults();
            updateUIFromConfig();
            alert('Configuration reset to defaults');
        }
    });
    
    // Export/Import config
    document.getElementById('export-config-btn').addEventListener('click', () => {
        config.exportToFile();
    });
    
    document.getElementById('import-config-btn').addEventListener('click', () => {
        document.getElementById('import-file-input').click();
    });
    
    document.getElementById('import-file-input').addEventListener('change', async (e) => {
        const file = e.target.files[0];
        if (file) {
            try {
                await config.importFromFile(file);
                updateUIFromConfig();
                alert('Configuration imported successfully');
            } catch (error) {
                alert('Failed to import config: ' + error.message);
            }
        }
    });
}

/**
 * Handle connection state change
 */
function handleConnectionChange(connected) {
    const statusIndicator = document.getElementById('status-indicator');
    const statusText = document.getElementById('status-text');
    const deviceInfo = document.getElementById('device-info');
    const connectBtn = document.getElementById('connect-btn');
    const disconnectBtn = document.getElementById('disconnect-btn');
    const readBtn = document.getElementById('read-config-btn');
    const writeBtn = document.getElementById('write-config-btn');
    
    if (connected) {
        statusIndicator.classList.add('connected');
        statusIndicator.classList.remove('disconnected');
        statusText.textContent = 'Connected';
        deviceInfo.style.display = 'block';
        connectBtn.style.display = 'none';
        disconnectBtn.style.display = 'inline-block';
        readBtn.disabled = false;
        writeBtn.disabled = false;
        
        document.getElementById('device-name').textContent = activeConnection ? activeConnection.getDeviceName() : '';
        document.getElementById('firmware-version').textContent = '0.1.0'; // From status

        // Auto-start button stream if the button-mapping tab is already active
        if (activeConnection && typeof activeConnection.startButtonStream === 'function') {
            const currentTab = document.querySelector('.tab-btn.active')?.getAttribute('data-tab');
            if (currentTab === 'buttons') {
                activeConnection.startButtonStream().catch(() => {});
            }
        }
        
        // Show configuration mode notice
        showNotification(
            'Configuration Mode Active', 
            'The device is in configuration mode for 60 seconds. HID input is disabled to prevent unwanted keystrokes/mouse movements. Hold the MODE button or send \'C\' via serial to stay in config mode.',
            'info',
            10000
        );
    } else {
        statusIndicator.classList.remove('connected');
        statusIndicator.classList.add('disconnected');
        statusText.textContent = 'Not Connected';
        deviceInfo.style.display = 'none';
        connectBtn.style.display = 'inline-block';
        disconnectBtn.style.display = 'none';
        readBtn.disabled = true;
        writeBtn.disabled = true;

        // Clear live button indicators when disconnected
        for (let i = 0; i < 56; i++) {
            const el = document.getElementById(`button-indicator-${i}`);
            if (el) el.classList.remove('pressed');
        }

        // Ensure streaming is halted on the firmware side
        if (activeConnection && typeof activeConnection.stopButtonStream === 'function') {
            activeConnection.stopButtonStream().catch(() => {});
        }
    }
}

/**
 * Handle battery level change
 */
function handleBatteryChange(batteryLevel) {
    document.getElementById('battery-level').textContent = `${batteryLevel}%`;
}

/**
 * Handle real-time button updates from BLE
 */
function handleButtonData(data) {
    // data.states is a Uint8Array of 8 bytes (64 bits, buttons 0-63)
    // We only show 56 indicators (7 SR chips × 8 = 56 buttons)
    for (let i = 0; i < 56; i++) {
        const el = document.getElementById(`button-indicator-${i}`);
        if (!el) continue;
        const byteIndex = Math.floor(i / 8);
        const bitIndex = i % 8;
        const pressed = data.states ? ((data.states[byteIndex] >> bitIndex) & 0x1) === 1 : false;
        el.classList.toggle('pressed', pressed);
    }
}

/**
 * Update UI from current configuration
 */
function updateUIFromConfig() {
    const cfg = config.getConfig();

    // ── Device & connectivity ──────────────────────────────────────────────
    document.getElementById('device-name-input').value      = cfg.deviceName  ?? '';
    document.getElementById('mode-select').value            = cfg.mode        ?? 1;
    document.getElementById('usb-poll-rate').value          = cfg.usbPollRate ?? 1;
    document.getElementById('ble-interval').value           = cfg.bleConnInterval ?? 2;
    document.getElementById('ble-tx-power').value           = cfg.bleTxPower  ?? 0;

    // ── Power management ──────────────────────────────────────────────────
    document.getElementById('auto-sleep').checked           = !!cfg.autoSleep;
    document.getElementById('sleep-timeout').value          = cfg.sleepTimeout ?? 600;

    // ── Shift registers ───────────────────────────────────────────────────
    const sr = cfg.shiftRegisters || {};
    document.getElementById('shift-reg-enabled').checked    = !!sr.enabled;
    document.getElementById('shift-reg-chips').value        = sr.numChips  ?? 7;
    document.getElementById('shift-reg-inverted').checked   = !!sr.inverted;

    // Re-populate pin dropdowns (they may not have been filled yet on first load)
    const availablePins = SeedJoyConfig.getAvailablePins();
    populatePinDropdown('shift-reg-data-pin',  availablePins, sr.dataPin  ?? 0x2B);
    populatePinDropdown('shift-reg-clock-pin', availablePins, sr.clockPin ?? 0x2C);
    populatePinDropdown('shift-reg-load-pin',  availablePins, sr.loadPin  ?? 0x2D);

    // ── Axes ──────────────────────────────────────────────────────────────
    if (cfg.axes) {
        cfg.axes.forEach((ax, i) => {
            const enableEl = document.getElementById(`axis-enable-${i}`);
            const pinEl    = document.getElementById(`axis-pin-${i}`);
            if (enableEl) enableEl.checked    = !!ax.enabled;
            if (pinEl)    pinEl.value         = ax.pin ?? 0xFF;
            if (pinEl)    pinEl.disabled      = !ax.enabled;
        });
    }

    // ── GPIO buttons ──────────────────────────────────────────────────────
    if (cfg.buttons) {
        cfg.buttons.forEach((btn, i) => {
            const enableEl  = document.getElementById(`button-enable-${i}`);
            const pinEl     = document.getElementById(`button-pin-${i}`);
            const logicalEl = document.getElementById(`button-logical-${i}`);
            const invertEl  = document.getElementById(`button-invert-${i}`);
            if (enableEl)  enableEl.checked   = !!btn.enabled;
            if (pinEl)     pinEl.value        = btn.pin ?? 0xFF;
            if (pinEl)     pinEl.disabled     = !btn.enabled;
            if (logicalEl) logicalEl.value    = btn.logicalNumber ?? i;
            if (invertEl)  invertEl.checked   = !!btn.inverted;
        });
    }

    console.log('UI updated from configuration (SR chips:', sr.numChips, ')');
}

/**
 * Load config from localStorage
 */
function loadLocalConfig() {
    try {
        const savedConfig = localStorage.getItem('seedjoy-config');
        if (savedConfig) {
            const parsed = JSON.parse(savedConfig);
            if (config.validateConfig(parsed)) {
                config.config = parsed;
                updateUIFromConfig();
                console.log('Loaded configuration from localStorage');
            }
        }
    } catch (error) {
        console.warn('Failed to load config from localStorage:', error);
    }
}

/**
 * Save config to localStorage
 */
function saveLocalConfig() {
    try {
        const cfg = config.getConfig();
        localStorage.setItem('seedjoy-config', JSON.stringify(cfg));
    } catch (error) {
        console.warn('Failed to save config to localStorage:', error);
    }
}

/**
 * Show a notification message to the user
 */
function showNotification(title, message, type = 'info', duration = 5000) {
    // Create notification element if it doesn't exist
    let notificationContainer = document.getElementById('notification-container');
    if (!notificationContainer) {
        notificationContainer = document.createElement('div');
        notificationContainer.id = 'notification-container';
        notificationContainer.style.position = 'fixed';
        notificationContainer.style.top = '20px';
        notificationContainer.style.right = '20px';
        notificationContainer.style.zIndex = '10000';
        notificationContainer.style.maxWidth = '400px';
        document.body.appendChild(notificationContainer);
    }
    
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.style.backgroundColor = type === 'info' ? '#2196F3' : type === 'success' ? '#4CAF50' : type === 'warning' ? '#FF9800' : '#F44336';
    notification.style.color = 'white';
    notification.style.padding = '16px';
    notification.style.marginBottom = '10px';
    notification.style.borderRadius = '8px';
    notification.style.boxShadow = '0 4px 6px rgba(0,0,0,0.3)';
    notification.style.animation = 'slideIn 0.3s ease';
    
    notification.innerHTML = `
        <div style="display: flex; justify-content: space-between; align-items: flex-start;">
            <div>
                <strong style="display: block; margin-bottom: 4px;">${title}</strong>
                <div style="font-size: 14px; opacity: 0.9;">${message}</div>
            </div>
            <button onclick="this.parentElement.parentElement.remove()" style="background: transparent; border: none; color: white; font-size: 20px; cursor: pointer; padding: 0 0 0 10px;">×</button>
        </div>
    `;
    
    notificationContainer.appendChild(notification);
    
    // Auto-remove after duration
    if (duration > 0) {
        setTimeout(() => {
            notification.style.animation = 'slideOut 0.3s ease';
            setTimeout(() => notification.remove(), 300);
        }, duration);
    }
}

// Add CSS animations for notifications
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from {
            transform: translateX(400px);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }
    
    @keyframes slideOut {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(400px);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);

// Auto-save config to localStorage when changed
setInterval(() => {
    if (config) {
        saveLocalConfig();
    }
}, 5000); // Save every 5 seconds
