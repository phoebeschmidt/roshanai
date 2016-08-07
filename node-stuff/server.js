'use strict'
var express = require('express');
var fs = require('fs');
var bodyParser = require('body-parser');
var app = express();
app.use(express.static(__dirname + '/public'));
app.use(bodyParser.json())
var idMapper = require('./services/idMapper.js')();

var routes = require('./routes/router.js')(app, idMapper);

app.listen(3000, function () {
    console.log("Listening on port 3000");
});
