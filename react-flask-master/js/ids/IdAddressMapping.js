'use strict'
import SolenoidList from './SolenoidList';
import React from 'react';
import RelayBoard from './RelayBoard';
var solenoids = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28];
var solenoids2 = [29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56];
var board1 = ["011", "012", "013", "014", "015", "016", "017", "018"];
var board2 = ["021", "022", "023", "024", "025", "026", "027", "028"];
var board3 = ["031", "032", "033", "034", "035", "036", "037", "038"];
var board4 = ["041", "042", "043", "044", "045", "046", "047", "048"];
var board5 = ["051", "052", "053", "054", "055", "056", "057", "058"];
var board6 = ["061", "062", "063", "064", "065", "066", "067", "068"];
var board7 = ["071", "072", "073", "074", "075", "076", "077", "078"];
var board8 = ["081", "082", "083", "084", "085", "086", "087", "088"];

var relayBoards = [board1, board2, board3, board4]
var relayBoards2 = [board5, board6, board7, board8]
import styles from '../../styles/diagram.css';
import Grid from 'react-bootstrap/lib/Grid';
import Col from 'react-bootstrap/lib/Col';

var IdAddressMapping  = React.createClass({
    componentDidMount: function() {
      
    },

    render: function() {
      return(
      <Grid className="diagram">
        <Col xs={4} md={2}>
          <SolenoidList solenoidIds={solenoids} />
        </Col>
        <Col xs={2} md={1}>
          {relayBoards.map(function(b, i) {
            return (<RelayBoard key={i} addresses={b} />)
          })}
        </Col>
        <Col xs={2} md={1}>
          {relayBoards2.map(function(b, i) {
            return (<RelayBoard key={i} addresses={b} />)
          })}
        </Col>
        <Col xs={4} md={2}>
          <SolenoidList solenoidIds={solenoids2} />
        </Col>
      </Grid>
    )
  }
})

export default IdAddressMapping;
