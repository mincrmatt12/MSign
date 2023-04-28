import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'
import ListGroup from 'react-bootstrap/ListGroup'

import ConfigContext from "../ctx"
import _ from "lodash"

function ParcelAddPanel({disabled, onNewId}) {
	const [curCode, setCurCode] = React.useState("");
	const [curCarrier, setCurCarrier] = React.useState("");
	
	return <div>
		<Form.Group className="my-2" controlId="new-track-code">
			<Form.Label>tracking code</Form.Label>
			<Form.Control type="text" value={curCode} placeholder="enter tracking code" onChange={(e) => setCurCode(e.target.value)} />
		</Form.Group>
		<Form.Group className="my-2" controlId="new-track-car">
			<Form.Label>carrier (from <a href="https://www.easypost.com/docs/api#carrier-tracking-strings">this list</a>)</Form.Label>
			<Form.Control type="text" value={curCarrier} placeholder="leave blank for auto" onChange={(e) => setCurCarrier(e.target.value)} />
		</Form.Group>
		<Button className="w-100" disabled={disabled} variant="success" onClick={() => {
			let params = {};
			if (curCode === "") {
				alert("missing code");
				return;
			}
			params["code"] = curCode;
			if (curCarrier !== "") params["carrier"] = curCarrier;
			// talk to api
			fetch("/a/newparcel", {method: "POST", body: JSON.stringify(params)})
				.then((res) => {
					if (!res.ok) return res.text().then(t => {
						alert("failed: " + t);
						throw new Error(t);
					});
					else return res.text();
				}).then((text) => onNewId(text));
		}}>create tracker</Button>
	</div>;
}

function ParcelEntry({data, updateData}) {
	return <Card className="my-3 p-1">
		<Card.Body>
			<hr className="hr-darkgray" />

		<Form.Group className="my-2" controlId="tc2">
			<Form.Label>name</Form.Label>
			<Form.Control type="text" value={data["name"]} placeholder="" 
				onChange={(e) => updateData(["name"], e.target.value)} />
		</Form.Group>
		<Form.Group className="my-2" controlId="tc">
			<Form.Label>tracker id</Form.Label>
			<Form.Control type="text" value={data["tracker_id"]} placeholder="ep tracker id" 
				onChange={(e) => updateData(["tracker_id"], e.target.value)} />
		</Form.Group>

			<hr className="hr-darkgray" />

			<div className="my-2">
				<Form.Check type="checkbox" checked={data["enabled"]} onChange={(e) => {updateData(["enabled"], e.target.checked);}} label="enabled" />
			</div>
		</Card.Body>
	</Card>;
}

export default function ParcelPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	const entries = _.get(cfg, "parcels.trackers", []);
	const add = (theId) => {
		let newe = _.clone(entries);
		newe.push({
			"name": "",
			"enabled": true,
			"tracker_id": theId
		});
		updateCfg("parcels.trackers", newe);
	}

	const remove = (at) => {
		updateCfg("parcels.trackers", _.filter(
			entries,
			(_, idx) => idx != at
		));
	}

	return <div>
		<hr className="hr-gray" />
		<ParcelAddPanel disabled={entries.length >= 6 || _.get(cfg, "parcels.key", "") == ""} onNewId={add} />
		<hr className="hr-gray" />
		{entries.map((x, idx) => <div key={idx}>
			<ParcelEntry data={x} updateData={(path, val) => updateCfg(_.concat(["parcels", "trackers", idx], path), val)} />
			<Button className="w-100" variant="danger" onClick={() => remove(idx)}>X</Button>
		</div>)}
	</div>;
}
