// test_serial.js
const portName = process.env.ARDUINO_PORT || 'COM7';
console.log('Trying to open port:', portName);

(async () => {
  try {
    const SerialPort = require('serialport');
    console.log('serialport module loaded, version:', require('serialport/package').version);

    // Attempt to open the port (don't set autoOpen -> we'll open explicitly)
    const port = new SerialPort(portName, { baudRate: 9600, autoOpen: false });

    port.open(err => {
      if (err) {
        console.error('OPEN ERROR:', err.message);
        console.error(err);
        process.exit(1);
      } else {
        console.log('Port opened OK');
        // Close immediately
        port.close(closeErr => {
          if (closeErr) {
            console.error('CLOSE ERROR:', closeErr);
            process.exit(1);
          } else {
            console.log('Port closed OK');
            process.exit(0);
          }
        });
      }
    });

    // Also list ports for reference
    SerialPort.list().then(list => {
      console.log('=== serialport.list() ===');
      console.log(list);
    }).catch(listErr => {
      console.error('LIST ERROR:', listErr);
    });

  } catch (e) {
    console.error('FATAL error (could not require serialport):');
    console.error(e);
    process.exit(2);
  }
})();
