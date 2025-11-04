import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'
import Alert from 'react-bootstrap/Alert'

import ConfigContext from '../ctx';
import * as _ from 'lodash-es';

function RawPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	const [current, setCurrent] = React.useState(JSON.stringify(cfg, undefined, 4));

	let isValid = true;
	try {
		JSON.parse(current);
	}
	catch (e) {
		isValid = false;
	}

	return <div>
		<Alert variant="warning">messing with this can corrupt your config; be careful!</Alert>
		<p>raw config json</p>
		<Form.Control as="textarea"
			value={current}
			style={{minHeight: "200px"}}
			onChange={(e) => {
				setCurrent(e.target.value);
				try {
					updateCfg("*", JSON.parse(e.target.value));
				}
				catch (e) {}
			}}
			isInvalid={
				!isValid && "invalid json, not saving"
			}
		/>
	</div>;
}

export default RawPane;
