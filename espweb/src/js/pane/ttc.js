import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'

class TTCPane extends React.Component {
	constructor(props) {
		super(props)
	}

	upload(p, idx, val) {
		let v_s = this.props['configState'];
		v_s[p][idx] = val;
		this.props['updateState'](v_s);
	}

	uploadAlert(val) {
		let v_s = this.props.configState;
		v_s.alert = val;
		this.props['updateState'](v_s);
	}

	add() {
		let v_s = this.props['configState'];
		v_s.dirtags.push("");
		v_s.stopids.push("");
		v_s.names.push("");
		this.props['updateState'](v_s);
	}

	remove(idx) {
		let v_s = this.props['configState'];
		v_s.dirtags.splice(idx, 1);
		v_s.stopids.splice(idx, 1);
		v_s.names.splice(idx, 1);
		this.props['updateState'](v_s);
	}

	render() {
		console.log([...this.props.configState.dirtags.keys()]);
		return <div>
			<hr className="hr-gray" />
			
			<p><a href="http://webservices.nextbus.com/service/publicJSONFeed?command=routeList&a=ttc" target="_blank">route list</a> • <a href="http://webservices.nextbus.com/service/publicJSONFeed?command=routeConfig&a=ttc&r=" target="_blank">stop list</a> • <a href="http://webservices.nextbus.com/service/publicJSONFeed?command=predictions&a=ttc&stopId=" target="_blank">predictions</a>
			</p>

			{[...this.props.configState.dirtags.keys()].map((x) => (
				<Card className="mb-3">
					<Container>
					<Row className="w-100 py-3" noGutters>
						<Col sm="11" xs="10">
							<Form.Group controlId={"dirtags_ctrl" + x}>
								<Form.Label>dirtags (comma seperated)</Form.Label>
								<FormControl type='text' value={this.props.configState.dirtags[x]} onChange={(e) => {this.upload('dirtags', x, e.target.value);}} />
							</Form.Group>
							<Form.Group controlId={"stopid_ctrl" + x}>
								<Form.Label>stop id</Form.Label>
								<FormControl type='text' value={this.props.configState.stopids[x]} onChange={(e) => {this.upload('stopids', x, e.target.value);}} />
							</Form.Group>
							<Form.Group controlId={"name_ctrl" + x}>
								<Form.Label>display name</Form.Label>
								<FormControl type='text' value={this.props.configState.names[x]} onChange={(e) => {this.upload('names', x, e.target.value.substr(0, 16));}} />
							</Form.Group>
						</Col>
						<Col sm="1" xs="2" className="pl-3">
							<Button variant="danger" onClick={() => this.remove(x)} className="h-100 w-100">X</Button>
						</Col>
					</Row>
				</Container>
				</Card>)
			)}
			<hr className="hr-gray" />
			<Button variant="light" disabled={this.props.configState.dirtags.length == 3} onClick={() => {this.add()}} className="w-100">+</Button>
			<hr className="hr-gray" />
			
			<Form.Group controlId="alert_ctrl">
				<Form.Label>alert search</Form.Label>
				<FormControl type='text' value={this.props.configState.alert} onChange={(e) => {this.uploadAlert(e.target.value);}} />
			</Form.Group>

			<hr className="hr-gray" />
		</div>
	}
}

export default TTCPane;
