var http = require('http');
var fs = require('fs');
var index = fs.readFileSync('index.html');

const ARDUINO_PORT = process.env.ARDUINO_PORT;
var port;

if (!ARDUINO_PORT) {
    console.log('Running in SIMULATION mode (no ARDUINO_PORT set)');
    port = {
        write: function(data) {
            console.log('→ Arduino would receive:', data.trim());
        }
    };
} else {
    try {
        const { SerialPort } = require('serialport');
        const { ReadlineParser } = require('@serialport/parser-readline');

        port = new SerialPort({ 
            path: ARDUINO_PORT,
            baudRate: 9600
        });

        const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

        port.on('open', function() {
            console.log('✓ Arduino connected on', ARDUINO_PORT);
        });

        port.on('error', function(err) {
            console.error('✗ Serial port error:', err.message);
        });
        
        parser.on('data', function(data) {
            console.log('Arduino:', data);
        });
        
    } catch(e) {
        console.error('✗ Error:', e.message);
        process.exit(1);
    }
}

var app = http.createServer(function(req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end(index);
});

var io = require('socket.io').listen(app);

io.on('connection', function(socket) {
    console.log('✓ Web client connected');
    
    socket.on('arduino', function(command) {
        console.log('→ Sending:', command);
        port.write(command + '\n');
    });
    
    socket.on('disconnect', function() {
        console.log('✗ Web client disconnected');
    });
});

app.listen(3000, function() {
    console.log('\n================================');
    console.log('  TRACK LED PACE CONTROL');
    console.log('================================');
    console.log('Server: http://localhost:3000');
    console.log('================================\n');
});