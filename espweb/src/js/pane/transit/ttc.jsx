import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Button from 'react-bootstrap/Button'

import ConfigContext, { intInteract } from '../../ctx';
import {directions} from './enums.js';
import * as _ from 'lodash-es';

function TTCPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);
	const entries = _.get(cfg, "ttc.entries", []);

	return <div>
		<p><a href="http://retro.umoiq.com/service/publicJSONFeed?command=routeList&a=ttc" target="_blank">route list</a> • <a href="http://retro.umoiq.com/service/publicJSONFeed?command=routeConfig&a=ttc&r=" target="_blank">stop list</a> • <a href="http://retro.umoiq.com/service/publicJSONFeed?command=predictions&a=ttc&stopId=" target="_blank">predictions</a>
		</p>

		{entries.map((x, idx) => {
			const hasSecond = 'alt_dirtag' in x && 'alt_stopid' in x;

			return <Card className="mb-3" key={idx}>
				<Card.Body>
					<Row>
					<Col md="10" lg="11">
						<Form.Check checked={hasSecond} onChange={(e) => {
							const x_new = _.clone(x);
							if (e.target.checked && !hasSecond) {
								x_new.alt_dirtag = [];
								x_new.alt_stopid = -1;
								x_new.dircode = ["na", "na"];
							}
							else if (!e.target.checked && hasSecond) {
								delete x_new.alt_dirtag;
								delete x_new.alt_stopid;
								delete x_new.dircode;
							}
							updateCfg(['ttc', 'entries', idx], x_new);
						}} label="has second direction" />
						<Row>
							<Col xxl>
							<Form.Group className="my-2">
								<Form.Label>dirtags</Form.Label>
								<Row>
									{_.concat(x.dirtag.map((y, idx2) => <Col key={idx2}>
										<Form.Control type="text" value={y} onChange={(e) => {
											if (e.target.value) updateCfg(['ttc', 'entries', idx, 'dirtag', idx2], e.target.value)
											else updateCfg(['ttc', 'entries', idx, 'dirtag'], _.filter(x.dirtag, (_, i) => i != idx2));
										}} />
									</Col>), 
									x.dirtag.length < 4 && <Col key={x.dirtag.length}>
										<Form.Control type="text" value="" placeholder="add new..." onChange={(e) => {updateCfg(['ttc', 'entries', idx, 'dirtag'], _.concat(x.dirtag, e.target.value))}} />
									</Col>)}
								</Row>
							</Form.Group>
							<Form.Group className="my-2" controlId={"stopid_ctrl" + idx}>
								<Form.Label>stop id</Form.Label>
								<FormControl type='text' value={x.stopid} onChange={(e) => {
									updateCfg(['ttc', 'entries', idx, 'stopid'], intInteract(e.target.value));
								}} />
							</Form.Group>
							{hasSecond && <Form.Group className="my-2">
								<Form.Label>direction</Form.Label>
								<Form.Select value={_.get(x, "dircode[0]", "na").toLowerCase()} onChange={(e) => updateCfg(["ttc", "entries", idx, "dircode", 0], e.target.value)}>
									{directions.map((x) => <option value={x} key={x}>{x}</option>)}
							</Form.Select></Form.Group>}
							</Col>
							{hasSecond && <Col xxl>
							<Form.Group className="my-2">
								<Form.Label>alt dirtags</Form.Label>
								<Row>
									{_.concat(x.alt_dirtag.map((y, idx2) => <Col key={idx2}>
										<Form.Control type="text" value={y} onChange={(e) => {
											if (e.target.value) updateCfg(['ttc', 'entries', idx, 'alt_dirtag', idx2], e.target.value)
											else updateCfg(['ttc', 'entries', idx, 'alt_dirtag'], _.filter(x.alt_dirtag, (_, i) => i != idx2));
										}} />
									</Col>), 
									x.alt_dirtag.length < 4 && <Col key={x.alt_dirtag.length}>
										<Form.Control type="text" value="" placeholder="add new..." onChange={(e) => {updateCfg(['ttc', 'entries', idx, 'alt_dirtag'], _.concat(x.alt_dirtag, e.target.value))}} />
									</Col>)}
								</Row>
							</Form.Group>
							<Form.Group className="my-2" controlId={"stopid2_ctrl" + idx}>
								<Form.Label>alt stop id</Form.Label>
								<FormControl type='text' value={x.alt_stopid} onChange={(e) => {
									updateCfg(['ttc', 'entries', idx, 'alt_stopid'], intInteract(e.target.value));
								}} />
							</Form.Group>
							<Form.Group className="my-2">
								<Form.Label>direction</Form.Label>
								<Form.Select value={_.get(x, "dircode[1]", "na").toLowerCase()} onChange={(e) => updateCfg(["ttc", "entries", idx, "dircode", 1], e.target.value)}>
									{directions.map((x) => <option value={x} key={x}>{x}</option>)}
							</Form.Select></Form.Group>
							</Col>}
						</Row>
						<Form.Group className="my-2" controlId={"name_ctrl" + idx}>
							<Form.Label>display name</Form.Label>
							<Form.Control type='text' value={x.name} onChange={(e) => {updateCfg(['ttc', 'entries', idx, 'name'], e.target.value);}} />
						</Form.Group>
						<Form.Group className="my-2">
							<Form.Label>distance on foot (minutes)</Form.Label>
							<Form.Control type="text" onChange={(e) => updateCfg(['ttc', 'entries', idx, 'distance'], intInteract(e.target.value))} placeholder="5" value={x.distance}/>
						</Form.Group>
					</Col>
					<Col md="2" lg="1">
						<Button variant="danger" onClick={() => {
							updateCfg('ttc.entries', _.filter(entries, (_, i) => i != idx));
						}} className="h-100 w-100">X</Button>
					</Col>
					</Row>
				</Card.Body>
			</Card>;}
		)}
		<hr className="hr-gray" />
		<Button variant="dark" disabled={entries.length == 5} onClick={() => {
			updateCfg('ttc.entries', _.concat(entries, {
				dirtag: [],
				stopid: -1,
				name: ''
			}));
		}} className="w-100">+</Button>
		<hr className="hr-gray" />
		
		<Form.Group controlId="alert_ctrl">
			<Form.Label>alert search</Form.Label>
			<FormControl type='text' value={_.get(cfg, "ttc.alert_search", "")} onChange={(e) => {updateCfg('ttc.alert_search', e.target.value ? e.target.value : undefined)}} />
		</Form.Group>

		<hr className="hr-gray" />
	</div>
}

export default TTCPane;
