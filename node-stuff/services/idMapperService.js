'use strict'
var fs = require('fs');

module.exports = () => {
  let json;

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
        else resolve();
      });
    })
  }

  return this;
}
