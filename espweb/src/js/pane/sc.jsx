import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'

import ConfigContext from "../ctx"
import _ from "lodash"

function parseTime(tm) {
	if (tm === "") return Number.NaN;

	let text = tm.trim();
	if (text.includes(":") && text.length == 5) {
		var hour, minute;
		console.log(text.split(":"));
		[hour, minute] = text.split(":").map((x) => Number.parseInt(x));
		console.log(hour, minute);

		if (Number.isNaN(hour) || Number.isNaN(minute)) return Number.NaN;

		return hour * 3600000 + minute * 60000;
	}
	else if (text.endsWith("h") || text.endsWith("H") && text.length == 3) {
		return Number.parseInt(text.substr(0, text.length - 1)) * 3600000
	}
	else {
		return Number.NaN;
	}
}

function displayTime(tm) {
	if (Number.isNaN(tm)) return "";
	let hour = Math.floor(tm / 3600000), minute = Math.floor((tm % 60000) / 1000);

	return hour.toString().padStart(2, "0") + ":" + minute.toString().padStart(2, "0");
}

const screens = [
	"ttc",
	"weather",
	"model",
	"parcels",
	"printer"
];

function ScEntry({data, updateData}) {
	const alwaysOn = !("times" in data) || data["times"] === undefined;
	const [startText, setStartText] = React.useState(alwaysOn ? "00:00" : displayTime(data.times.start));
	const [endText, setEndText] = React.useState(alwaysOn ? "23:59" : displayTime(data.times.end));

	let startTime = parseTime(startText); if (Number.isNaN(startTime)) startTime = 0;
	let endTime = parseTime(endText); if (Number.isNaN(endTime)) endTime = 86400000-1;

	return <Card className="my-3 p-1">
		<Card.Body>
			<Card.Title>
				<Form.Select value={data.screen.toLowerCase()} onChange={(e) => {updateData(["screen"], e.target.value)}}>
					{screens.map((x) => <option value={x}>{x}</option>)}
				</Form.Select>
			</Card.Title>
			<hr className="hr-darkgray" />
			<Form.Check type="radio" checked={alwaysOn} onChange={(e) => {if (e.target.checked) updateData(["times"], undefined)}} label="always on" />
			<Form.Check type="radio" checked={!alwaysOn} onChange={(e) => {if (e.target.checked) updateData(["times"], {"start": startTime, "end": endTime})}} label="time range"/>
			{!alwaysOn && <>
			<div className="d-flex mt-2 gap-1 align-items-center">show from <Form.Control className="d-inline-block flex-grow-1 w-auto" isInvalid={Number.isNaN(parseTime(startText))} onChange={(e) => {
				setStartText(e.target.value);
				if (!Number.isNaN(parseTime(e.target.value))) {
					updateData(["times", "start"], parseTime(e.target.value));
				}
			}} type="text" value={startText} placeholder="HH:MM" /> to 
			<Form.Control className="d-inline-block flex-grow-1 w-auto" isInvalid={Number.isNaN(parseTime(endText))} onChange={(e) => {
				setEndText(e.target.value);
				if (!Number.isNaN(parseTime(e.target.value))) {
					updateData(["times", "end"], parseTime(e.target.value));
				}
			}} type="text" value={endText} placeholder="HH:MM" /></div></>}

			<hr className="hr-darkgray" />

			<div className="my-2">
				<Form.Label>duration</Form.Label>
				<Form.Control value={data.length / 1000} type="text" onChange={(e) => {
					if (e.target.value == "") e.target.value = "0.0";
					if (Number.isNaN(Number.parseFloat(e.target.value))) e.preventDefault();
					else updateData(["length"], Math.trunc(Number.parseFloat(e.target.value)*1000));
				}} />
			</div>
		</Card.Body>
	</Card>
}

function ScCfgPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	const entries = _.get(cfg, "sccfg.screens", []);

	const add = (at) => {
		let newe = _.clone(entries);
		newe.splice(at, 0, {
			"screen": "ttc",
			"length": 10000
		});
		updateCfg("sccfg.screens", newe);
		console.log(newe);
	}

	const remove = (at) => {
		updateCfg("sccfg.screens", _.filter(
			entries,
			(_, idx) => idx != at
		));
	}

	return <div>
		<hr className="hr-gray" />
		{entries.map((x, idx) => <div key={idx}>
			<div className="d-flex gap-2 w-100">
				<Button className="flex-grow-1" variant="success" onClick={() => add(idx)}>+</Button>{idx != 0 && <Button className="flex-grow-1" variant="danger" onClick={() => remove(idx-1)}>X</Button>}
			</div>
			<ScEntry data={x} updateData={(path, val) => updateCfg(_.concat(["sccfg", "screens", idx], path), val)} />
		</div>)}
		<div className="d-flex gap-2 w-100"><Button className="flex-grow-1" variant="success" onClick={() => add(entries.length)}>+</Button>{entries.length > 0 && <Button className="flex-grow-1" variant="danger" onClick={() => remove(entries.length-1)}>X</Button>}</div>
	</div>;
}

export default ScCfgPane;

