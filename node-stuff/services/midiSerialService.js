var midi = require("midi");
var SerialPort = require("serialport");
var prompt = require("prompt");
var FAKE_SERIAL = require("../config.js").FAKE_SERIAL;

module.exports = (solenoidToRelayMap, programMap)=> {
  //Configure serial device
  this.program = 104;
  midiToSolenoidMap = programMap[this.program];
  var ser;
  // FAKE_SERIAL = tr;
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
    ser = new SerialPort("/dev/tty.usbserial", serialProps, function (err) {
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

  var sendSignal = function(solenoidId, onOff) {
    var relayAddress = solenoidToRelayMap[solenoidId];
    if (relayAddress) {
      var command = "!" + relayAddress + onOff + ".";
      ser.write(command, function(err) {
        if (err) console.log('Error: ', err);
      });
    }
  }

  var buildCommand = function(note, onOff) {
    if (!midiToSolenoidMap[note]) {
      return;
    } else {
      switch (midiToSolenoidMap[note].type) {
        case "sequence":
          midiToSolenoidMap[note].solenoids.forEach(function(s, i) {
            setTimeout(sendSignal, 100*i, s, onOff);
          })
          break;
        case "simultaneous":
          midiToSolenoidMap[note].solenoids.forEach(function(s) {
            sendSignal(s, onOff);
          })
          break;
      }
    }
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

  var changeProgram = function(programNumber) {
    this.program = programNumber;
    midiToSolenoidMap = programMap[this.program];
  }

  this.handleMidiMessage = function(deltaTime, message) {
    console.log('m:' + message + ' d:' + deltaTime);
    output.sendMessage(message);
    var status = message[0] & 0xf0;
    if (status == 0x90) {
      var note = message[1];
      var velocity = message[2];
      if (velocity == 0) {
        buildCommand(note, 0);// note on with velocity 0 is a note off
      } else {
        buildCommand(note, 1); // note on
      }
    } else if (status == 0x80) {
      var note = message[1];
      buildCommand(note, 0);
    } else if (status == 0xB0) { //TODO Program change is actually 0xC0. 0xB0 are the side launchpad buttons-- for testing
      var program = message[1];
      changeProgram(program);
    }
  }
  input.on('message', this.handleMidiMessage);

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
