import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'

import ConfigContext from '../ctx';
import _ from 'lodash';

function ApiPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	return <div>
		<hr className="hr-gray" />

		<Form.Group controlId="dsky_ctrl" className="my-2">
			<Form.Label>tomorrow.io</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'weather.key', '')} onChange={(e) => {updateCfg('weather.key', e.target.value ? e.target.value : undefined)}} />
		</Form.Group>

		<Form.Group controlId="cfgc_ctrl" className="my-2">
			<Form.Label>config server</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'cfgpull.secret', '')} onChange={(e) => {updateCfg('cfgpull.secret', e.target.value ? e.target.value : undefined)}} />
		</Form.Group>

		<Form.Group controlId="17track_ctrl" className="my-2">
			<Form.Label>17track</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'parcels.key', '')} onChange={(e) => {updateCfg('parcels.key', e.target.value ? e.target.value : undefined)}} />
		</Form.Group>

		<Form.Group controlId="octoprint_ctrl" className="my-2">
			<Form.Label>octoprint</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'octoprint.key', '')} onChange={(e) => {updateCfg('octoprint.key', e.target.value ? e.target.value : undefined)}} />
		</Form.Group>

		<hr className="hr-gray" />
	</div>
}

export default ApiPane;
