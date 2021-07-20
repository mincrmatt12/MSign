import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'

import ConfigContext from '../ctx.js';
import _ from 'lodash';

function ApiPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	return <div>
		<hr className="hr-gray" />

		<Form.Group controlId="dsky_ctrl">
			<Form.Label>darksky</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'weather.key', '')} onChange={(e) => {updateCfg('weather.key', e.target.value ? e.target.value : undefined)}} />
		</Form.Group>

		<hr className="hr-gray" />
	</div>
}

export default ApiPane;
