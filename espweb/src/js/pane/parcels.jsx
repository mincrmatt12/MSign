import React from 'react'
import Form from 'react-bootstrap/Form'
import Card from 'react-bootstrap/Card'
import Button from 'react-bootstrap/Button'
import ButtonGroup from 'react-bootstrap/ButtonGroup';
import Dropdown from 'react-bootstrap/Dropdown';

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
		<Dropdown className="w-100" as={ButtonGroup}>
			<Button disabled={disabled} variant="success" onClick={() => {
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
			<Dropdown.Toggle className="flex-grow-0" split variant="success" id="create-parcel-drop" />
			<Dropdown.Menu>
				<Dropdown.Item as="button" onClick={() => onNewId("")}>create empty</Dropdown.Item>
			</Dropdown.Menu>
		</Dropdown>
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
				<Form.Check type="checkbox" checked={("consider_local_tz" in data) ? data["consider_local_tz"] : false} onChange={(e) => {updateData(["consider_local_tz"], e.target.checked);}} label="treat times as local" />
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
