import React from 'react'
import Form from 'react-bootstrap/Form'
import Button from 'react-bootstrap/Button'
import InputGroup from 'react-bootstrap/InputGroup'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'

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

	submitFirm(e) {
		e.preventDefault();

		let file_in = new FormData(e.target);

		if (file_in.has("esp")) {
			fetch('/a/updatefirm', {
				method: 'POST',
				body: file_in
			}).then((resp) => {
				if (resp.ok) {
					alert("update sent. check sign for progress");
					window.close();
				}
				else {
					alert("update not sent.");
				}
			});
		}
		else {
			fetch('/a/updatestm', {
				method: 'POST',
				body: file_in
			}).then((resp) => {
				if (resp.ok) {
					alert("updating stm only.");
					window.close();
				}
				else {
					alert("update not sent.");
				}
			});
		}

		alert("sending update...");
	}

	render() {
		return <div>
			<hr className="hr-gray" />

			<Form onSubmit={(e) => this.submitUI(e)}>
				<Row>
					<Col md="9" sm="7">
						<Form.Group controlId="update_webui">
							<Form.Label>web archive file</Form.Label>
							<Form.Control name="file" required type="file" accept=".ar" />
						</Form.Group>
					</Col>
					<Col sm="3" xs="5" className="border-left hr-darkgray d-flex flex-column justify-content-center">
						<Button className="w-100" type="submit" variant="danger">upload new ui</Button>
					</Col>
				</Row>
			</Form>

			<hr className="hr-gray" />

			<Form onSubmit={(e) => this.submitFirm(e)}>
				<Row>
					<Col md="9" sm="7">
						<Form.Group controlId="update_stm">
							<Form.Label>stm firmware</Form.Label>
							<Form.Control name="stm" required type="file" accept=".bin" />
						</Form.Group>

						<Form.Group controlId="update_esp">
							<Form.Label>esp firmware</Form.Label>
							<Form.Control name="esp" type="file" accept=".bin" />
						</Form.Group>
					</Col>

					<Col sm="3" xs="5" className="border-left hr-darkgray d-flex flex-column justify-content-center">
						<Button type="submit" variant="danger">upload new firmware</Button>
					</Col>
				</Row>
			</Form>

			<hr className="hr-gray" />
		</div>
	}
}

export default UpdatePane;
