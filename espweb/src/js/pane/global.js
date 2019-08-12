import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'

class GlobalPane extends React.Component {
	constructor(props) {
		super(props)
	}

	upload(p, val) {
		let v_s = this.props['configState'];
		v_s[p] = val;
		this.props['updateState'](v_s);
	}

	render() {
		return <div>
			<hr className="hr-gray" />

			<Form.Group controlId="ssid_ctrl">
				<Form.Label>ssid</Form.Label>
				<FormControl type='text' value={this.props.configState.ssid} onChange={(e) => {this.upload('ssid', e.target.value);}} />
			</Form.Group>
			<Form.Group controlId="psk_ctrl">
				<Form.Label>psk</Form.Label>
				<FormControl type='password' value={this.props.configState.psk} onChange={(e) => {this.upload('psk', e.target.value);}} />
			</Form.Group>
			<Form.Group controlId="ntp_ctrl">
				<Form.Label>ntp server</Form.Label>
				<InputGroup>
					<InputGroup.Prepend>
						<InputGroup.Text id="httpaddon1">http://</InputGroup.Text>
					</InputGroup.Prepend>
					<FormControl type='text' value={this.props.configState.ntpserver} onChange={(e) => {this.upload('ntpserver', e.target.value);}} placeholder='pool.ntp.org'/>
				</InputGroup>
			</Form.Group>

			<hr className="hr-gray" />

			<Form.Group controlId="cuser_ctrl">
				<Form.Label>config login user</Form.Label>
				<FormControl type='config panel username' value={this.props.configState.configuser} onChange={(e) => {this.upload('configuser', e.target.value);}} />
			</Form.Group>
			<Form.Group controlId="psk_ctrl">
				<Form.Label>config login password</Form.Label>
				<FormControl type='password' value={this.props.configState.configpass} onChange={(e) => {this.upload('configpass', e.target.value);}} />
			</Form.Group>

			<hr className="hr-gray" />
		</div>
	}
}

export default GlobalPane;
