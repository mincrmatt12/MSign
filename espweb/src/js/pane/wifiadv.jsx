import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import InputGroup from 'react-bootstrap/InputGroup'

import ConfigContext, { intInteract } from '../ctx';
import * as _ from 'lodash-es';
import { parseInt } from 'lodash-es';

function parseBssid(str) {
	const regex = /^([0-9A-F]{2}[:-])([0-9A-F]{2}[:-])([0-9A-F]{2}[:-])([0-9A-F]{2}[:-])([0-9A-F]{2}[:-])([0-9A-F]{2})$/i;

	const m = str.match(regex);
	if (m === null) return null;
	else return [1, 2, 3, 4, 5, 6].map(x => parseInt(m[x], 16));
}

function genBssid(bss) {
	if (bss == null) return "";
	
	return bss.map(x => x.toString(16).padStart(2, "0").toUpperCase()).join(":");
}

function BssidElement() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);
	const current = _.get(cfg, "wifi.conn.bssid", null);

	const [currentText, setCurrentText] = React.useState(genBssid(current));

	return <Form.Control type="text" placeholder="<use any>" value={currentText} isInvalid={
		currentText !== "" && parseBssid(currentText) === null
		} onChange={e => {
			setCurrentText(e.target.value);
			if (parseBssid(e.target.value) !== null) updateCfg("wifi.conn.bssid", parseBssid(e.target.value));
		}} />
}

function NumberInput({path, ...extra}) {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	return <FormControl type='text' {...extra} value={_.get(cfg, path, '')} onChange={(e) => {updateCfg(path, intInteract(e.target.value));}} />
}

function WifiPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	const code = _.get(cfg, 'wifi.conn.country.code', '');
	let invalid = false;
	if (code !== "" && !code.match(/^[A-Z]{2}$/)) invalid = true;

	return <div>
		<hr className="hr-gray" />

		<Form.Group controlId="chan_ctrl" className="my-2">
			<Form.Label>force channel</Form.Label>
			<NumberInput placeholder="<use any>" path="wifi.conn.channel" />
		</Form.Group>
		
		<Form.Group controlId="bss_ctrl" className="my-2">
			<Form.Label>force bssid</Form.Label>
			<BssidElement />
		</Form.Group>

		<hr className="hr-gray" />

		<Form.Group controlId="wfc_ctrl" className="my-2">
			<Form.Label>country code</Form.Label>
			<FormControl placeholder="auto" isInvalid={invalid} type='text' value={_.get(cfg, 'wifi.conn.country.code', '')} onChange={(e) => {updateCfg('wifi.conn.country.code', e.target.value ? e.target.value : undefined)}} />
		</Form.Group>

		<Form.Group controlId="sc_ctrl" className="my-2">
			<Form.Label>start channel</Form.Label>
			<NumberInput path="wifi.conn.country.schan" />
		</Form.Group>

		<Form.Group controlId="nc_ctrl" className="my-2">
			<Form.Label>number of channels</Form.Label>
			<NumberInput path="wifi.conn.country.nchan" />
		</Form.Group>

		<Form.Group controlId="mp_ctrl" className="my-2">
			<Form.Label>max power</Form.Label>
			<InputGroup>
				<NumberInput path="wifi.conn.country.max_power_db" />
				<InputGroup.Text>dB</InputGroup.Text>
			</InputGroup>
		</Form.Group>

		<hr className="hr-gray" />
	</div>
}

export default WifiPane;
