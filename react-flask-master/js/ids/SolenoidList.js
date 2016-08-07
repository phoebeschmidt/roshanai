'use strict'
import React from 'react';
import Solenoid from './Solenoid';
import ListGroup from 'react-bootstrap/lib/ListGroup';
import ListGroupItem from 'react-bootstrap/lib/ListGroupItem';

var SolenoidList  = React.createClass({

    render: function() {
      var solenoidIds = this.props.solenoidIds;
      console.log("solenoidIds", solenoidIds);
      return (
        <ListGroup>
          {this.props.solenoidIds.map((s) => {
            return(
              <ListGroupItem key={s}>
              <Solenoid id={s} />
              </ListGroupItem>
            )
          })}
        </ListGroup>
      )
    }
})

export default SolenoidList;
