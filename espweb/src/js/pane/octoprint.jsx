import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import InputGroup from 'react-bootstrap/InputGroup'
import Row from 'react-bootstrap/Row'
import Button from 'react-bootstrap/Button'
import {HostnameEdit} from './common/hostname';

import ConfigContext, { floatInteract } from '../ctx';
import _ from 'lodash';

function convertColorToString(color) {
	return "#" + color.map((x) => x.toString(16).padStart(2, "0")).join("");
}

function convertStringToColor(cstring) {
	return [
		Number.parseInt(cstring.slice(1, 3), 16),
		Number.parseInt(cstring.slice(3, 5), 16),
		Number.parseInt(cstring.slice(5, 7), 16)
	];
}

function OctoprintPage() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);
	const current_prefixes = _.get(cfg, "octoprint.filter_prefixes", []);

	return <div>
		<hr className="hr-gray" />

		<Form.Group className="my-2" controlId="host_control">
			<Form.Label>hostname</Form.Label>
			<HostnameEdit value={_.get(cfg, "octoprint.host")} onChange={(v) => updateCfg("octoprint.host", v)} />
		</Form.Group>
		<Form.Check checked={_.get(cfg, "octoprint.g_relative_is_e", false)} label="g90 affects relative extruder mode" onChange={(e) => updateCfg("octoprint.g_relative_is_e", e.target.checked)} />

		<hr className="hr-gray" />

		<Form.Group className="my-2" controlId="color_control">
			<Form.Label>filament color</Form.Label>
			<Form.Control type="color" id="color_control_control" title="pick filament color" value={
				convertColorToString(_.get(cfg, "octoprint.filament_color", [0xff, 0x77, 0x10]))
				} onChange={(e) => updateCfg("octoprint.filament_color", convertStringToColor(e.target.value))} />
		</Form.Group>

		<hr className="hr-gray" />
		<Form.Group className="my-2">
			<Form.Label>filtered filename prefixes</Form.Label>
			<div>
				{ // _.concat is so that the entire set is one list so react tracks focus between the new/old elements
					_.concat(current_prefixes.map((y, idx) =>
					<Form.Control type="text" className="mb-2" value={y} key={idx} onChange={(e) => {
						if (e.target.value) updateCfg(['octoprint', 'filter_prefixes', idx], e.target.value);
						else updateCfg(['octoprint', 'filter_prefixes'], _.filter(current_prefixes, (_, i) => i != idx));
					}} />),
					(current_prefixes.length < 3 && <Form.Control type="text" value="" key={current_prefixes.length} placeholder="add new..." onChange={(e) => 
						updateCfg('octoprint.filter_prefixes', _.concat(current_prefixes, e.target.value))} />))}
			</div>
		</Form.Group>
		<hr className="hr-gray" />
	</div>
}

export default OctoprintPage;
