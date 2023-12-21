import React from 'react'
import Form from 'react-bootstrap/Form'
import Button from 'react-bootstrap/Button'
import InputGroup from 'react-bootstrap/InputGroup'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'

import VersionTag from "./common/vertag";

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

		alert("sending update... (this will take a while)");
	}

	submitCA(e) {
		e.preventDefault();

		let file_in = new FormData(e.target);

		fetch('/a/newca', {
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

		alert("sending update... (this will take a while)");
	}

	submitFirm(e) {
		e.preventDefault();

		let file_in = new FormData(e.target);

		if (!e.target['stm'].files.length) return;
		if (!e.target['esp'].files.length) file_in.delete("esp");

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

		alert("sending update...");
	}

	render() {
		return <div>
			<hr className="hr-gray" />

			<Form onSubmit={(e) => this.submitUI(e)}>
				<Row>
					<Col md="9" sm="7">
						<Form.Group className="my-2" controlId="update_webui">
							<Form.Label>web archive file</Form.Label>
							<Form.Control name="file" required type="file" accept=".ar" />
						</Form.Group>
					</Col>
					<Col md="3" sm="5" className="border-start hr-darkgray d-flex flex-column justify-content-center">
						<Button className="w-100" type="submit" variant="danger">upload new ui</Button>
					</Col>
				</Row>
			</Form>

			<hr className="hr-gray" />

			<Form onSubmit={(e) => this.submitFirm(e)}>
				<Row>
					<Col md="9" sm="7">
						<Form.Group className="my-2" controlId="update_stm">
							<Form.Label>stm firmware</Form.Label>
							<Form.Control name="stm" required type="file" accept=".bin" />
						</Form.Group>

						<Form.Group className="my-2" controlId="update_esp">
							<Form.Label>esp firmware</Form.Label>
							<Form.Control name="esp" type="file" accept=".bin" />
						</Form.Group>
					</Col>

					<Col md="3" sm="5" className="border-start hr-darkgray d-flex flex-column justify-content-center">
						<Button type="submit" variant="danger">upload new firmware</Button>
					</Col>
				</Row>
			</Form>

			<hr className="hr-gray" />

			<Form onSubmit={(e) => this.submitCA(e)}>
				<Row>
					<Col md="9" sm="7">
						<Form.Group className="my-2" controlId="update_certs">
							<Form.Label>ca certs bundle</Form.Label>
							<Form.Control name="file" required type="file" accept=".bin" />
						</Form.Group>
					</Col>
					<Col md="3" sm="5" className="border-start hr-darkgray d-flex flex-column justify-content-center">
						<Button className="w-100" type="submit" variant="danger">upload new CAs</Button>
					</Col>
				</Row>
			</Form>

			<hr className="hr-gray" />

			<VersionTag />
		</div>
	}
}

export default UpdatePane;
