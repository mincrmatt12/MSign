import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'

import ConfigContext from '../../ctx.js';
import _ from 'lodash';

import {getURLPrefix, HostnameEdit} from '../common/hostname';

export default function GTFSPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);
	const entries = _.get(cfg, "gtfs.entries", []);

	return <div>
		<Form.Group className="my-2">
			<Form.Label>feed host</Form.Label>
			<HostnameEdit placeholder={"_webapps.regionofwaterloo.ca"} value={_.get(cfg, "gtfs.feed.host")} onChange={(v) => updateCfg("gtfs.feed.host", v)} />
		</Form.Group>
		<Form.Group>
			<Form.Label>feed (tripupdates) url</Form.Label>
			<InputGroup>
				<InputGroup.Text>{getURLPrefix(_.get(cfg, "gtfs.feed.host", "_webapps.regionofwaterloo.ca"))}</InputGroup.Text>
				<FormControl type="text" value={_.get(cfg, "gtfs.feed.url")} placeholder={"/api/grt-routes/api/tripupdates"} onChange={(e) => updateCfg("gtfs.feed.url", e.target.value)} />
			</InputGroup>
		</Form.Group>
		<hr className="hr-gray" />
		{entries.map((x, idx) => (
			<Card className="my-3" key={idx}>
				<Card.Body>
					<Row>
						<Col sm="11" xs="10">
							<Form.Group className="my-2">
								<Form.Label>route id</Form.Label>
								<Form.Control type='text' value={x.route} onChange={(e) => {updateCfg(['gtfs', 'entries', idx, 'route'], e.target.value);}} />
							</Form.Group>
							<Form.Group className="my-2">
								<Form.Label>stop id</Form.Label>
								<Form.Control type='text' value={x.stop} onChange={(e) => {updateCfg(['gtfs', 'entries', idx, 'stop'], e.target.value);}} />
							</Form.Group>
							<Form.Group className="my-2">
								<Form.Label>display name</Form.Label>
								<Form.Control type='text' value={x.name} onChange={(e) => {updateCfg(['gtfs', 'entries', idx, 'name'], e.target.value);}} />
							</Form.Group>
						</Col>
						<Col sm="1" xs="2">
							<Button variant="danger" onClick={() => {
								updateCfg('gtfs.entries', _.filter(entries, (_, i) => i != idx));
							}} className="h-100 w-100">X</Button>
						</Col>
					</Row>
				</Card.Body>
			</Card>
		))}
		<hr className="hr-gray" />
		<Button variant="dark" disabled={entries.length == 3} onClick={() => {
			updateCfg('gtfs.entries', _.concat(entries, {
				route: '',
				stop: '',
				name: ''
			}));
		}} className="w-100">+</Button>
		<hr className="hr-gray" />
	</div>;
}