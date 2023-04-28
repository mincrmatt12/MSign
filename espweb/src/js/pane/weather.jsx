import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import InputGroup from 'react-bootstrap/InputGroup'
import Row from 'react-bootstrap/Row'
import Button from 'react-bootstrap/Button'

import ConfigContext from '../ctx';
import _ from 'lodash';

function WeatherPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);
	const [address, setAddress] = React.useState("");

	const geocode = () => {
		let url = "http://open.mapquestapi.com/geocoding/v1/address?key=gIjVAAXB6OXgGJRKTK5CGZ2u2BUYsM5i&location=" + encodeURIComponent(address);

		fetch(url)
			.then((resp) => {
				if (resp.ok)
					return resp.json();
				throw new Error("invalid resp");
			})
			.then((obj) => {
				updateCfg('weather.coord', [
					obj.results[0].locations[0].latLng.lat,
					obj.results[0].locations[0].latLng.lng,
				])
			})
			.catch((e) => {
				console.log(e);
				alert("couldn't geocode");
			});
	};

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
				const value = Number.parseFloat(e.target.value);
				if (Number.isNaN(value)) e.preventDefault();
				else updateCfg('weather.coord[0]', value);
			}} />
		</Form.Group>
		<Form.Group className="my-2" controlId="longitude_control">
			<Form.Label>longitude</Form.Label>
			<FormControl type='text' value={_.get(cfg, 'weather.coord[1]', 0.0)} onChange={(e) => {
				const value = Number.parseFloat(e.target.value);
				if (Number.isNaN(value)) e.preventDefault();
				else updateCfg('weather.coord[1]', value);
			}} />
		</Form.Group>

		<hr className="hr-gray" />

		<Button disabled={!('geolocation' in navigator)} className="w-100" variant="info" onClick={useCurrentPlace}>use current location</Button>

		<hr className="hr-gray" />
	</div>
}

export default WeatherPane;
