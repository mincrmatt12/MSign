import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import InputGroup from 'react-bootstrap/InputGroup'
import Row from 'react-bootstrap/Row'
import Button from 'react-bootstrap/Button'

class WeatherPane extends React.Component {
	constructor(props) {
		super(props)

		this.state = {
			entered: ""
		}
	}

	upload(p, val) {
		if (Number.isNaN(Number.parseFloat(val))) {
			return false;
		}

		let v_s = this.props['configState'];
		v_s[p] = Number.parseFloat(val);
		this.props['updateState'](v_s);

		return true;
	}

	useCurrentPlace() {
		navigator.geolocation.getCurrentPosition((pos) => {
			this.upload('longitude', pos.coords.longitude);
			this.upload('latitude', pos.coords.latitude);
		}, () => {alert("couldn't get position");}, {enableHighAccuracy: true, timeout: 10000});
	}

	geocode(param) {
		let url = "http://open.mapquestapi.com/geocoding/v1/address?key=gIjVAAXB6OXgGJRKTK5CGZ2u2BUYsM5i&location=" + encodeURIComponent(param);

		fetch(url)
			.then((resp) => {
				if (resp.ok)
					return resp.json();
				throw new Error("invalid resp");
			})
			.then((obj) => {
				this.upload('longitude', obj.results[0].locations[0].latLng.lng);
				this.upload('latitude', obj.results[0].locations[0].latLng.lat);
			})
			.catch((e) => {
				console.log(e);
				alert("couldn't geocode");
			});
	}

	render() {
		return <div>
			<hr className="hr-gray" />

			<Form.Group controlId="latitude_control">
				<Form.Label>latitude</Form.Label>
				<FormControl type='text' value={this.props.configState.latitude} onChange={(e) => {if (!this.upload('latitude', e.target.value)) e.preventDefault();}} />
			</Form.Group>
			<Form.Group controlId="longitude_control">
				<Form.Label>longitude</Form.Label>
				<FormControl type='text' value={this.props.configState.longitude} onChange={(e) => {if (!this.upload('longitude', e.target.value)) e.preventDefault();}} />
			</Form.Group>

			<hr className="hr-gray" />

			<Button disabled={!('geolocation' in navigator)} className="w-100" variant="info" onClick={() => {this.useCurrentPlace();}}>use current location</Button>

			<hr className="hr-gray" />

			<InputGroup className="mb-3">
				<FormControl type='text' value={this.state.entered} placeholder="address" onChange={(e) => {this.setState({entered: e.target.value});}} />
				<InputGroup.Append>
					<Button variant="success" onClick={(e) => {this.geocode(this.state.entered);}}>geocode and use</Button>
				</InputGroup.Append>
			</InputGroup>

			<hr className="hr-gray" />
		</div>
	}
}

export default WeatherPane;
