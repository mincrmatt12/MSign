import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'

class TdsbPane extends React.Component {
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

			<Form.Group controlId="tdsb_username">
				<Form.Label>tdsb username</Form.Label>
				<FormControl type='text' value={this.props.configState.user} onChange={(e) => {this.upload('user', e.target.value);}} />
			</Form.Group>

			<Form.Group controlId="tdsb_password">
				<Form.Label>tdsb password</Form.Label>
				<FormControl type='password' value={this.props.configState.pass} onChange={(e) => {this.upload('pass', e.target.value);}} />
			</Form.Group>

			<hr className="hr-gray" />

			<Form.Group controlId="tdsb_sid">
				<Form.Label>school code</Form.Label>
				<FormControl type='text' value={this.props.configState.code} onChange={(e) => {this.upload('code', e.target.value);}} />
			</Form.Group>

			<hr className="hr-gray" />
		</div>
	}
}

export default TdsbPane;
