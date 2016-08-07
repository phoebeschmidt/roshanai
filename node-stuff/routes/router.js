'use strict'
var express = require('express');
var router = function(app, idMapper) {
  app.get('/hello', (req, res) => {
    res.send('hello there!!!!');
  });

  app.get('/solenoids/addresses', (req, res) => {
    idMapper.getMappings().then(
      (data) => { res.send(data); },
      (err) => { res.sendStatus(400).send({Error: err}); }
    )
  });

  app.get('/solenoids/:id/address', (req, res) => {
    res.json(idMapper.getMapping(req.params.id));
  });

  app.post('/solenoids/:id/address', (req, res) => {
    var newAddr = req.body.address;
    if (newAddr) {
      var result = idMapper.updateMapping(req.params.id, newAddr);
      result.then(
        () => {
          res.sendStatus(200)
        },
        (err) => {
          console.log("Error:", err);
          res.sendStatus(400).send(err);
        });
    } else {
      console.log('newaddr was not in the body');
      res.sendStatus(400).send({error: "Error: No address included in request."});
    }
  })
}


module.exports = router;
