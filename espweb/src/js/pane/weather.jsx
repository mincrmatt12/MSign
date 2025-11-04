import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import InputGroup from 'react-bootstrap/InputGroup'
import Row from 'react-bootstrap/Row'
import Button from 'react-bootstrap/Button'

import ConfigContext, { floatInteract } from '../ctx';
import * as _ from 'lodash-es';

function WeatherPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	const useCurrentPlace = () => {
		navigator.geolocation.getCurrentPosition((pos) => {
			updateCfg('weather.coord', [pos.coords.latitude, pos.coords.longitude]);
		}, () => {alert("couldn't get position");}, {enableHighAccuracy: true, timeout: 10000});
	}

	return <div>
		<hr className="hr-gray" />

		<Form.Group className="my-2" controlId="latitude_control">
			<Form.Label>latitude</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'weather.coord[0]', 0.0)} onChange={(e) => {
				updateCfg('weather.coord[0]', floatInteract(e.target.value));
			}} />
		</Form.Group>
		<Form.Group className="my-2" controlId="longitude_control">
			<Form.Label>longitude</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'weather.coord[1]', 0.0)} onChange={(e) => {
				updateCfg('weather.coord[1]', floatInteract(e.target.value));
			}} />
		</Form.Group>

		<hr className="hr-gray" />

		<Button disabled={!('geolocation' in navigator)} className="w-100" variant="info" onClick={useCurrentPlace}>use current location</Button>

		<hr className="hr-gray" />
	</div>
}

export default WeatherPane;
