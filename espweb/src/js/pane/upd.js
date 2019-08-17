import React from 'react'
import Form from 'react-bootstrap/Form'
import Button from 'react-bootstrap/Button'
import InputGroup from 'react-bootstrap/InputGroup'
import Row from 'react-bootstrap/Row'

class UpdatePane extends React.Component {
	constructor(props) {
		super(props)
	}

	submitUI(e) {
		e.preventDefault();

		let file_in = new FormData(e.target);

		fetch('/a/newui', {
			method: 'POST',
			body: file_in
		}).then((resp) => {
			if (resp.ok) {
				alert("update sent, ESP will now update, reopen in a bit.");
				window.close();
			}
			else {
				alert("update not sent.");
			}
		});
	}

	render() {
		return <div>
			<hr className="hr-gray" />

			<Form onSubmit={(e) => this.submitUI(e)} inline>
				<Form.Group controlId="update_webui">
					<Form.Label>web archive file</Form.Label>
					<Form.Control name="file" required type="file" accept=".ar" />
				</Form.Group>

				<Button type="submit" variant="danger">upload new ui</Button>
			</Form>

			<hr className="hr-gray" />
		</div>
	}
}

export default UpdatePane;
