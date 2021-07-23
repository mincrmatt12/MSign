import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'

import ConfigContext from '../ctx.js';
import _ from 'lodash';

function TTCPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	const entries = _.get(cfg, "ttc.entries", []);

	return <div>
		<hr className="hr-gray" />
		
		<p><a href="http://retro.umoiq.com/service/publicJSONFeed?command=routeList&a=ttc" target="_blank">route list</a> • <a href="http://retro.umoiq.com/service/publicJSONFeed?command=routeConfig&a=ttc&r=" target="_blank">stop list</a> • <a href="http://retro.umoiq.com/service/publicJSONFeed?command=predictions&a=ttc&stopId=" target="_blank">predictions</a>
		</p>

		{entries.map((x, idx) => (
			<Card className="mb-3" key={idx}>
				<Card.Body>
					<Row>
					<Col sm="11" xs="10">
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
								if (e.target.value == "") e.target.value = "0"
								const value = parseInt(e.target.value)
								if (Number.isNaN(value)) e.preventDefault();
								else updateCfg(['ttc', 'entries', idx, 'stopid'], value);
							}} />
						</Form.Group>
						<Form.Group className="my-2" controlId={"name_ctrl" + idx}>
							<Form.Label>display name</Form.Label>
							<Form.Control type='text' value={x.name} onChange={(e) => {updateCfg(['ttc', 'entries', idx, 'name'], e.target.value);}} />
						</Form.Group>
					</Col>
					<Col sm="1" xs="2">
						<Button variant="danger" onClick={() => {
							updateCfg('ttc.entries', _.filter(entries, (_, i) => i != idx));
						}} className="h-100 w-100">X</Button>
					</Col>
					</Row>
				</Card.Body>
			</Card>)
		)}
		<hr className="hr-gray" />
		<Button variant="dark" disabled={entries.length == 3} onClick={() => {
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
