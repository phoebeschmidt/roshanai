'use strict'
import React from 'react';
import InputGroup from 'react-bootstrap/lib/InputGroup';
import FormControl from 'react-bootstrap/lib/FormControl';
import Button from 'react-bootstrap/lib/Button';
import styles from '../../styles/solenoids.css';

var Solenoid = React.createClass({
  changeAddress: function(newAddr) {
    console.log("go button to change address was clicked");
  },

    render() {
      return (
          <InputGroup >
            <InputGroup.Addon>{this.props.id}</InputGroup.Addon>
            <FormControl type="text" placeholder="address"/>
            <FormControl.Feedback />
            <InputGroup.Button> <Button onClick={this.changeAddress}>Go!</Button></InputGroup.Button>
          </InputGroup>
      )
    }
})

export default Solenoid;
