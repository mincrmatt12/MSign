import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'

class ApiPane extends React.Component {
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

			<Form.Group controlId="dsky_ctrl">
				<Form.Label>darksky</Form.Label>
				<FormControl type='text' value={this.props.configState.darksky} onChange={(e) => {this.upload('darksky', e.target.value);}} />
			</Form.Group>

			<hr className="hr-gray" />
		</div>
	}
}

export default ApiPane;
