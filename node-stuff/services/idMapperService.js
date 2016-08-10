'use strict'
var fs = require('fs');

module.exports = () => {
  let json;
  var listeners = [];
  this.getMappings = () => {
    return new Promise((resolve, reject) => {
      fs.readFile('idMap.json', (err, data) => {
        if (err) reject(err);
        else {
          json = JSON.parse(data);
          resolve(json);
        }
      });
    });
  }

  //call this to initialize local json obj
  this.getMappings();

  this.getMapping = (key) => {
    return json[key];
  }

  this.updateMapping = (key, val) => {
    // maybe do some validation here??
    // seeing weird behavior with Object.keys() 8/7
    json[key] = val;
    return new Promise((resolve, reject) => {
      fs.writeFile('idMap.json', JSON.stringify(json), (err) => {
        if (err) reject(err);
        else {
          this.updateListeners()
          resolve();
      }
      });
    })
  }

  this.addListener = (listener) => {
    console.log("Adding listener: " + listener); // TODO: Always prints undefined? Why is that?
    if (listeners.indexOf(listener) < 0) {
      listeners.push(listener);
    }
  }

  this.updateListeners = () => {
    listeners.forEach((listener) => {
      console.log(listener); // TODO: Always prints undefined? Why is that?
      listener.mappingsUpdated(json);
    })
  }

  this.removeListener = (listener) => {
    let idx = listeners.indexOf(listener);
    if (idx >= 0) {
      listeners.splice(idx, 1);
    }
  }
  return this;
}
