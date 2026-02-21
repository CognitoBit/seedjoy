/**
 * SeedJoy WebSerial Communication Module
 * 
 * Provides USB Serial connection as an alternative to WebBluetooth
 */

class SeedJoySerial {
  constructor() {
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.connected = false;
    this.readBuffer = '';
    
    // Callbacks (same as BLE for compatibility)
    this.onConnectionChange = null;
    this.onBatteryChange = null;
    this.onButtonData = null;
    this.onAxisData = null;
    this.onStatusMessage = null;

    // Pending promise handles
    this.pendingConfigResolve = null;
    this.pendingWriteResolve  = null;
    this.pendingWriteReject   = null;
    this.pendingWriteTimeout  = null;
  }

  isSupported() {
    return 'serial' in navigator;
  }

  async connect() {
    if (!this.isSupported()) {
      throw new Error('WebSerial not supported. Use Chrome/Edge 89+');
    }

    // Request port from user
    try {
      this.port = await navigator.serial.requestPort();
    } catch (err) {
      throw new Error('No port selected');
    }

    // Open port (115200 baud, 8N1)
    await this.port.open({ baudRate: 115200 });

    this.writer = this.port.writable.getWriter();
    this.reader = this.port.readable.getReader();

    // Start reading loop
    this.connected = true;
    this.readLoop();

    if (this.onConnectionChange) this.onConnectionChange(true);

    // Send initial command to verify connection
    await this.sendCommand('ping');

    return true;
  }

  async disconnect() {
    this.connected = false;

    if (this.reader) {
      await this.reader.cancel();
      this.reader.releaseLock();
      this.reader = null;
    }

    if (this.writer) {
      this.writer.releaseLock();
      this.writer = null;
    }

    if (this.port) {
      await this.port.close();
      this.port = null;
    }

    if (this.onConnectionChange) this.onConnectionChange(false);
  }

  async readLoop() {
    try {
      while (this.connected && this.reader) {
        const { value, done } = await this.reader.read();
        if (done) break;

        // Decode incoming bytes to text
        const text = new TextDecoder().decode(value);
        this.readBuffer += text;

        // Process complete lines
        this.processBuffer();
      }
    } catch (err) {
      console.error('Serial read error:', err);
      this.disconnect();
    }
  }

  processBuffer() {
    let lines = this.readBuffer.split('\n');
    this.readBuffer = lines.pop() || ''; // Keep incomplete line

    for (let line of lines) {
      line = line.trim();
      if (!line) continue;

      // Try to parse as JSON response
      if (line.startsWith('{')) {
        try {
          const msg = JSON.parse(line);
          this.handleMessage(msg);
        } catch (err) {
          console.warn('Failed to parse JSON:', line);
        }
      } else {
        // Regular text output (debug logs, etc.)
        console.log('[Device]', line);
      }
    }
  }

  handleMessage(msg) {
    if (msg.type === 'config') {
      // Config data received
      if (this.pendingConfigResolve) {
        this.pendingConfigResolve(msg.data);
        this.pendingConfigResolve = null;
      }
    } else if (msg.type === 'status') {
      if (this.onStatusMessage) this.onStatusMessage(msg);
      // Resolve / reject a pending writeConfig promise
      if (this.pendingWriteResolve) {
        clearTimeout(this.pendingWriteTimeout);
        const resolve = this.pendingWriteResolve;
        const reject  = this.pendingWriteReject;
        this.pendingWriteResolve = null;
        this.pendingWriteReject  = null;
        this.pendingWriteTimeout = null;
        if (msg.success) {
          resolve(msg);
        } else {
          reject(new Error(msg.message || 'Write failed'));
        }
      }
    } else if (msg.type === 'axes') {
      if (this.onAxisData) this.onAxisData(msg.data);
    } else if (msg.type === 'buttons') {
      if (this.onButtonData) this.onButtonData(msg.data);
    } else if (msg.type === 'battery') {
      if (this.onBatteryChange) this.onBatteryChange(msg.level);
    } else if (msg.type === 'pong') {
      console.log('Device responded to ping');
    }
  }

  async sendCommand(cmd, data = null) {
    if (!this.writer) throw new Error('Not connected');

    const msg = data ? `${cmd}:${JSON.stringify(data)}\n` : `${cmd}\n`;
    const encoded = new TextEncoder().encode(msg);
    await this.writer.write(encoded);
  }

  async readConfig() {
    return new Promise((resolve, reject) => {
      this.pendingConfigResolve = resolve;
      this.sendCommand('read_config').catch(reject);

      // Timeout after 5 seconds
      setTimeout(() => {
        if (this.pendingConfigResolve) {
          this.pendingConfigResolve = null;
          reject(new Error('Read config timeout'));
        }
      }, 5000);
    });
  }

  async writeConfig(config) {
    return new Promise(async (resolve, reject) => {
      this.pendingWriteResolve = resolve;
      this.pendingWriteReject  = reject;
      this.pendingWriteTimeout = setTimeout(() => {
        this.pendingWriteResolve = null;
        this.pendingWriteReject  = null;
        this.pendingWriteTimeout = null;
        reject(new Error('Write config timeout'));
      }, 5000);

      try {
        await this.sendCommand('write_config', config);
      } catch (err) {
        clearTimeout(this.pendingWriteTimeout);
        this.pendingWriteResolve = null;
        this.pendingWriteReject  = null;
        this.pendingWriteTimeout = null;
        reject(err);
      }
    });
  }

  /** Ask firmware to start emitting button states at ~20 Hz (Web Serial button test) */
  async startButtonStream() {
    if (this.connected) await this.sendCommand('stream_buttons');
  }

  /** Stop firmware button state streaming */
  async stopButtonStream() {
    if (this.connected) await this.sendCommand('stop_stream');
  }

  getDeviceName() {
    return this.port?.getInfo()?.usbProductId 
      ? `USB Serial (VID:${this.port.getInfo().usbVendorId})` 
      : 'USB Serial';
  }
}
