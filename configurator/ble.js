/*
 * SeedJoy WebBluetooth Communication Module (clean, fixed)
 * - Adds support for real-time button monitoring (buttons monitor characteristic)
 * - Backwards-compatible: missing BLE characteristic is handled gracefully
 */

class SeedJoyBLE {
  constructor() {
    this.device = null;
    this.server = null;
    this.configService = null;
    this.configReadCharacteristic = null;
    this.configWriteCharacteristic = null;
    this.statusCharacteristic = null;
    this.axesMonitorCharacteristic = null;
    this.buttonsMonitorCharacteristic = null; // new
    this.calibrateCharacteristic = null;
    this.batteryService = null;
    this.batteryCharacteristic = null;

    // UUIDs
    this.HID_SERVICE_UUID = 0x1812; // Standard HID Service
    this.BATTERY_SERVICE_UUID = 0x180F;
    this.BATTERY_LEVEL_UUID = 0x2A19;

    // Custom config service + characteristics
    this.CONFIG_SERVICE_UUID = 'e95d0001-251d-470a-a062-fa1922dfa9a8';
    this.CONFIG_READ_UUID = 'e95d0002-251d-470a-a062-fa1922dfa9a8';
    this.CONFIG_WRITE_UUID = 'e95d0003-251d-470a-a062-fa1922dfa9a8';
    this.STATUS_UUID = 'e95d0004-251d-470a-a062-fa1922dfa9a8';
    this.AXES_MONITOR_UUID = 'e95d0005-251d-470a-a062-fa1922dfa9a8';
    this.BUTTONS_MONITOR_UUID = 'e95d0007-251d-470a-a062-fa1922dfa9a8'; // new
    this.CALIBRATE_UUID = 'e95d0006-251d-470a-a062-fa1922dfa9a8';

    // Callbacks
    this.onAxisData = null;
    this.onButtonData = null; // new
    this.onStatusMessage = null;
    this.onConnectionChange = null;
    this.onBatteryChange = null;

    this.connected = false;
  }

  isSupported() {
    return !!navigator.bluetooth;
  }

  async connect() {
    if (!this.isSupported()) throw new Error('WebBluetooth not supported. Use Chrome/Edge/Opera.');

    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: 'SeedJoy' }],
      optionalServices: [this.HID_SERVICE_UUID, this.BATTERY_SERVICE_UUID, this.CONFIG_SERVICE_UUID]
    });

    this.device.addEventListener('gattserverdisconnected', () => this.onDisconnected());
    this.server = await this.device.gatt.connect();

    // Battery (optional)
    try {
      this.batteryService = await this.server.getPrimaryService(this.BATTERY_SERVICE_UUID);
      this.batteryCharacteristic = await this.batteryService.getCharacteristic(this.BATTERY_LEVEL_UUID);
      await this.batteryCharacteristic.startNotifications();
      this.batteryCharacteristic.addEventListener('characteristicvaluechanged', (ev) => {
        const v = ev.target.value.getUint8(0);
        if (this.onBatteryChange) this.onBatteryChange(v);
      });
      const initial = await this.batteryCharacteristic.readValue();
      if (this.onBatteryChange) this.onBatteryChange(initial.getUint8(0));
    } catch (err) {
      console.warn('Battery service not available');
    }

    // Config service (custom)
    console.log('Attempting to discover config service:', this.CONFIG_SERVICE_UUID);
    try {
      this.configService = await this.server.getPrimaryService(this.CONFIG_SERVICE_UUID);
      console.log('✓ Config service discovered successfully');

      // Config characteristics (some may be missing on older firmware)
      console.log('Discovering config characteristics...');
      this.configReadCharacteristic = await this.configService.getCharacteristic(this.CONFIG_READ_UUID).catch(() => null);
      console.log('Read characteristic:', this.configReadCharacteristic ? '✓' : '✗');
      
      this.configWriteCharacteristic = await this.configService.getCharacteristic(this.CONFIG_WRITE_UUID).catch(() => null);
      console.log('Write characteristic:', this.configWriteCharacteristic ? '✓' : '✗');
      
      this.statusCharacteristic = await this.configService.getCharacteristic(this.STATUS_UUID).catch(() => null);
      this.axesMonitorCharacteristic = await this.configService.getCharacteristic(this.AXES_MONITOR_UUID).catch(() => null);
      this.buttonsMonitorCharacteristic = await this.configService.getCharacteristic(this.BUTTONS_MONITOR_UUID).catch(() => null);
      this.calibrateCharacteristic = await this.configService.getCharacteristic(this.CALIBRATE_UUID).catch(() => null);

      // Status notifications
      if (this.statusCharacteristic) {
        await this.statusCharacteristic.startNotifications();
        this.statusCharacteristic.addEventListener('characteristicvaluechanged', (ev) => {
          const status = JSON.parse(new TextDecoder().decode(ev.target.value.buffer));
          if (this.onStatusMessage) this.onStatusMessage(status);
        });
      }

      // Axis notifications
      if (this.axesMonitorCharacteristic) {
        await this.axesMonitorCharacteristic.startNotifications();
        this.axesMonitorCharacteristic.addEventListener('characteristicvaluechanged', (ev) => {
          const parsed = this.parseAxisData(ev.target.value);
          if (this.onAxisData) this.onAxisData(parsed);
        });
      }

      // Buttons notifications (optional - firmware must support)
      if (this.buttonsMonitorCharacteristic) {
        await this.buttonsMonitorCharacteristic.startNotifications();
        this.buttonsMonitorCharacteristic.addEventListener('characteristicvaluechanged', (ev) => {
          const parsed = this.parseButtonsData(ev.target.value);
          if (this.onButtonData) this.onButtonData(parsed);
        });
      } else {
        console.info('Buttons monitor characteristic not present on device (firmware may be older)');
      }

    } catch (err) {
      console.error('✗ Config service discovery failed!');
      console.error('Error details:', err);
      console.error('Error name:', err.name);
      console.error('Error message:', err.message);
    }

    this.connected = true;
    if (this.onConnectionChange) this.onConnectionChange(true);
    return true;
  }

  async disconnect() {
    if (this.device && this.device.gatt.connected) await this.device.gatt.disconnect();
    this.onDisconnected();
  }

  onDisconnected() {
    this.connected = false;
    this.server = null;
    this.configService = null;
    this.configReadCharacteristic = null;
    this.configWriteCharacteristic = null;
    this.statusCharacteristic = null;
    this.axesMonitorCharacteristic = null;
    this.buttonsMonitorCharacteristic = null;
    this.calibrateCharacteristic = null;
    if (this.onConnectionChange) this.onConnectionChange(false);
  }

  // Parse axis notification (packed little-endian): int16[4], uint16[4], uint32
  parseAxisData(dataView) {
    const dv = new DataView(dataView.buffer);
    let off = 0;
    const processed = [];
    for (let i = 0; i < 4; i++) { processed.push(dv.getInt16(off, true)); off += 2; }
    const raw = [];
    for (let i = 0; i < 4; i++) { raw.push(dv.getUint16(off, true)); off += 2; }
    const timestamp = dv.getUint32(off, true);
    return { processed, raw, timestamp };
  }

  // Parse buttons notification: uint16 bitmask, uint32 timestamp
  parseButtonsData(dataView) {
    const dv = new DataView(dataView.buffer);
    const bitmask = dv.getUint16(0, true);
    const timestamp = dv.getUint32(2, true);
    return { bitmask, timestamp };
  }

  async readConfig() {
    if (!this.configReadCharacteristic) throw new Error('Config read characteristic unavailable');
    const val = await this.configReadCharacteristic.readValue();
    return JSON.parse(new TextDecoder().decode(val.buffer));
  }

  async writeConfig(config) {
    if (!this.configWriteCharacteristic) throw new Error('Config write characteristic unavailable');
    const json = JSON.stringify(config);
    const enc = new TextEncoder();
    const buf = enc.encode(json);
    const MTU = 512;
    if (buf.byteLength <= MTU) {
      await this.configWriteCharacteristic.writeValue(buf);
    } else {
      await this.writeFragmented(this.configWriteCharacteristic, buf, MTU);
    }
    return true;
  }

  // Fragmented write helper
  async writeFragmented(chr, buffer, fragmentSize = 512) {
    const num = Math.ceil(buffer.byteLength / fragmentSize);
    for (let i = 0; i < num; i++) {
      const start = i * fragmentSize;
      const end = Math.min(start + fragmentSize, buffer.byteLength);
      const frag = buffer.slice(start, end);
      await chr.writeValue(frag);
      await new Promise(r => setTimeout(r, 30));
    }
  }

  async calibrate(axisIndex, command) {
    if (!this.calibrateCharacteristic) throw new Error('Calibrate characteristic unavailable');
    const data = new Uint8Array([axisIndex, command]);
    await this.calibrateCharacteristic.writeValue(data);
  }

  async readStatus() {
    if (!this.statusCharacteristic) return null;
    const v = await this.statusCharacteristic.readValue();
    return JSON.parse(new TextDecoder().decode(v.buffer));
  }

  getDeviceName() { return this.device ? this.device.name : 'Not connected'; }
  isConnected() { return this.connected && this.device && this.device.gatt.connected; }
}

window.SeedJoyBLE = SeedJoyBLE;
