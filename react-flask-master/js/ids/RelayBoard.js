'use strict'
import React from 'react';
import Checkbox from 'react-bootstrap/lib/Checkbox';
import Panel from 'react-bootstrap/lib/Panel';

var RelayBoard = React.createClass({
  render() {
    return (
      <Panel>
        {this.props.addresses.map((a, i)=> {
          return (<Checkbox key={i} address={a}><strong>{a}</strong></Checkbox>)
        })}
      </Panel>
    )
  }
})

export default RelayBoard;
