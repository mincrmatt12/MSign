import React from 'react'
import Form from 'react-bootstrap/Form'
import Card from 'react-bootstrap/Card'
import Button from 'react-bootstrap/Button'
import InputGroup from 'react-bootstrap/InputGroup'
import Dropdown from 'react-bootstrap/Dropdown';
import Modal from 'react-bootstrap/Modal';
import Row from 'react-bootstrap/Row';
import Col from 'react-bootstrap/Col';

import ConfigContext from "../ctx"
import _ from "lodash"

const TrackerListSource = React.createContext([]);

function searchInCarriers(allCarriers, name) {
	let results = [];
	if (name == "") return [];
	for (const {
		key, _name
	} of allCarriers) {
		if (results.length >= 10) return results;
		if (_name.toLowerCase().includes(name.toLowerCase())) results.push({key, name: _name});
	}
	return results;
}

function nameById(allCarriers, keyF) {
	if (keyF == 0) return "<auto>";
	for (const {key, _name} of allCarriers) {
		if (key == keyF) return _name;
	}
	return "<unknown>";
}

function ChooseCarrierDialog({updateCode, close, isOpen}) {
	const allCarriers = React.useContext(TrackerListSource);
	const [search, updateSearch] = React.useState("");
	const results = searchInCarriers(allCarriers, search);

	return <Modal show={isOpen} onHide={close} size="lg">
		<Modal.Header>
			<Modal.Title>choose carrier</Modal.Title>
		</Modal.Header>
		<Modal.Body className="mx-2">
			<Form.Group controlId="choosecarriersearch">
				<Form.Control type="text" placeholder="search" autoFocus value={search} onChange={(e) => {updateSearch(e.target.value)}} />
			</Form.Group>
			{results.length > 0 && <hr className="hr-gray" />}
			{results.map(({key, name}) => <div className="bg-secondary d-flex align-items-center rounded p-2 my-2">
				<Col xs="8" md="10" as="p" className="my-0 mb-1">{name}</Col>
				<Col xs="4" md="2"><Button className="w-100" onClick={() => {
					updateCode(key); close();
				}}>use</Button></Col>
			</div>)}
		</Modal.Body>
		<Modal.Footer>
			<Button variant="secondary" onClick={close}>cancel</Button>
		</Modal.Footer>
	</Modal>
}

function CarrierChooser({code, updateCode}) {
	const allCarriers = React.useContext(TrackerListSource);
	const resolved = nameById(allCarriers, code);
	const [modalOpen, setModalOpen] = React.useState(false);

	const actualInput = <Form.Control placeholder="auto" value={code == 0 ? "" : code} onChange={(e) => updateCode(
		e.target.value === "" ? 0 : (Number.parseInt(e.target.value) || code)
	)} />;

	if (allCarriers.length == 0) {
		return <InputGroup>
			{actualInput}
		</InputGroup>;
	}
	else {
		return <>
			<InputGroup>
				{actualInput}
				{code == 0 || <InputGroup.Text>{resolved}</InputGroup.Text>}
				<Dropdown>
					<Dropdown.Toggle variant="secondary" className="border-black" />
					<Dropdown.Menu>
						<Dropdown.Item onClick={() => setModalOpen(true)}>search</Dropdown.Item>
					</Dropdown.Menu>
				</Dropdown>
			</InputGroup>
			<ChooseCarrierDialog updateCode={updateCode} close={() => setModalOpen(false)} isOpen={modalOpen} />
		</>
	}
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
			<Form.Label>tracking number</Form.Label>
			<Form.Control type="text" value={data["tracking_number"]} placeholder="" 
				onChange={(e) => updateData(["tracking_number"], e.target.value)} />
		</Form.Group>

		<hr className="hr-darkgray" />

		<Row className="my-2">
			<Col md className="mb-1">
				<Form.Group controlId="tc">
					<Form.Label>initial carrier</Form.Label>
					<CarrierChooser code={data["carrier_id"] ?? 0} updateCode={(v) => {updateData(["carrier_id"], v)}} />
				</Form.Group>
			</Col>
			<Col md>
				<Form.Group controlId="fc">
					<Form.Label>final carrier</Form.Label>
					<CarrierChooser code={data["final_carrier_id"] ?? 0} updateCode={(v) => {updateData(["final_carrier_id"], v)}} />
				</Form.Group>
			</Col>
		</Row>

		<Form.Check checked={data["translate_messages"] ?? true} label="translate messages" onChange={(e) => updateData(["translate_messages"], e.target.checked)} />
		<Form.Check checked={data["raw_timestamp"] ?? false} label="ignore timezone" onChange={(e) => updateData(["raw_timestamp"], e.target.checked)} />

		</Card.Body>
	</Card>;
}

export default function ParcelPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);
	const [allCarriers, updateAllCarriers] = React.useState([]);
	React.useEffect(() => {
		fetch("https://res.17track.net/asset/carrier/info/apicarrier.all.json")
			.then((x) => x.json())
			.then((x) => {updateAllCarriers(x)});
	}, []);

	const entries = _.get(cfg, "parcels.trackers", []);
	const add = () => {
		let newe = _.clone(entries);
		newe.push({
			"name": "",
			"tracking_number": ""
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
		<TrackerListSource.Provider value={allCarriers}>
			<hr className="hr-gray" />
			<Button disabled={entries.length >= 6 || _.get(cfg, "parcels.key", "") == ""} className="w-100" variant="success" onClick={add}>add new</Button>
			<hr className="hr-gray" />
			{entries.map((x, idx) => <div key={idx}>
				<ParcelEntry data={x} updateData={(path, val) => updateCfg(_.concat(["parcels", "trackers", idx], path), val)} />
				<Button className="w-100" variant="danger" onClick={() => remove(idx)}>X</Button>
			</div>)}
		</TrackerListSource.Provider>
	</div>;
}
