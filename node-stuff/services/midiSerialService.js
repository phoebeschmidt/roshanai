var midi = require("midi");
var SerialPort = require("serialport");
var prompt = require("prompt");


module.exports = (solenoidToRelayMap)=> {
  //Configure serial device
  var map = solenoidToRelayMap;
  var ser;
  FAKE_SERIAL = true;
  if (FAKE_SERIAL) {
    ser = {
      write : function(string, callback) {
        console.log(string);
        if (callback) callback();
      },
      drain : function(callback) {
        if (callback) callback();
      }
    };
  } else {
    var serialProps = {
      baudRate: 19200
    };
    var ser = new SerialPort("/dev/tty.usbserial", serialProps, function (err) {
      if (err) {
        console.log('Error: ', err.message);
        process.exit()
      }
    });
  }
  function zeroPad(num, places) {
    var zero = places - num.toString().length + 1;
    return Array(+(zero > 0 && zero)).join("0") + num;
  }
  var sendSignal = function(note, onOff) {
    var relay = (note % 8) + 1;
    var board = Math.floor(note/8);
    if (solenoidToRelayMap)
    var command = "!" + solenoidToRelayMap[note] + onOff + ".";
    ser.write(command, function(err) {
      if (err) console.log('Error: ', err);
    });
  }

  // Configure Midi Input
  var input = new midi.input();
  var output = new midi.output();
  var portCount = input.getPortCount();
  for (var i = 0; i < portCount; i++){
    console.log("Input device " + i + ":" + input.getPortName(i));
  }
  portCount = output.getPortCount();
  for (var i = 0; i < portCount; i++){
    console.log("Output device " + i + ":" + output.getPortName(i));
  }
  input.on('message', function(deltaTime, message) {
    console.log('m:' + message + ' d:' + deltaTime);
    output.sendMessage(message);
    var status = message[0] & 0xf0;
    if (status == 0x90) {
      var note = message[1];
      var velocity = message[2];
      if (velocity == 0) {
        sendSignal(note, 0);// note on with velocity 0 is a note off
      } else {
        sendSignal(note, 1); // note on
      }
    } else if (status == 0x80) {
      var note = message[1];
      sendSignal(note, 0);
    } else if (status == 0xc0) { // Program change
      var program = message[1];
      change_program(program);
    }
  });

  // Use prompt to define input device
  prompt.start();
  var properties = [
    {
      name: 'inputId',
      validator: /^[0-9]*$/,
      warning: 'inputId must be an integer.'
    },
    {
      name: 'outputId',
      validator: /^[0-9]*$/,
      warning: 'outputId must be an integer.'
    }
  ]
  prompt.get(properties, function (err, result) {
    if (err) { return onErr(err); }
    var inputId = parseInt(result.inputId, 0);
    var outputId = parseInt(result.outputId, 0);
    console.log('Selected device: ' + input.getPortName(inputId));
    input.openPort(inputId);
    output.openPort(inputId);
  });
  function onErr(err) {
    console.log(err);
    return 1;
  }

  this.mappingsUpdated = (newMap) => {
    console.log("mappings updated");
    map = newMap;
  }

  this.shutdown = () => {
    console.log("MidiSerialService shutdown");
    input.closePort();
    output.closePort();
  }

  return this;
}
