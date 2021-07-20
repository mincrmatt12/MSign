import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'

import _ from 'lodash';

import ConfigContext from '../ctx.js';

function GlobalPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	return <div>
		<hr className="hr-gray" />

		<Form.Group className="my-2" controlId="ssid_ctrl">
			<Form.Label>ssid</Form.Label>
			<FormControl type='text' value={_.get(cfg, "wifi.conn.ssid", "")} onChange={(e) => {updateCfg('wifi.conn.ssid', e.target.value);}} />
		</Form.Group>
		<Form.Group className="my-2" controlId="psk_ctrl">
			<Form.Label>psk</Form.Label>
			<FormControl type='password' value={_.get(cfg, "wifi.conn.psk", "")} onChange={(e) => {updateCfg('wifi.conn.psk', e.target.value);}} />
		</Form.Group>
		<Form.Group className="my-2" controlId="ntp_ctrl">
			<Form.Label>ntp server</Form.Label>
			<InputGroup>
				<InputGroup.Text id="httpaddon1">ntp://</InputGroup.Text>
				<FormControl type='text' value={_.get(cfg, "time.server", "")} onChange={(e) => {updateCfg('time.server', e.target.value);}} placeholder='pool.ntp.org'/>
			</InputGroup>
		</Form.Group>
		<Form.Group className="my-2" controlId="timezone_ctrl">
			<Form.Label>posix timezone</Form.Label>
			<FormControl type='text' value={_.get(cfg, "time.zone", "")} onChange={(e) => {updateCfg('time.zone', e.target.value);}} placeholder="EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00" />
		</Form.Group>

		<hr className="hr-gray" />

		<Form.Group className="my-2" controlId="cuser_ctrl">
			<Form.Label>config login user</Form.Label>
			<FormControl placeholder='admin' type='text' value={_.get(cfg, "webui.login.user", "")} onChange={(e) => {updateCfg('webui.login.user', e.target.value);}} />
		</Form.Group>
		<Form.Group className="my-2" controlId="psk_ctrl">
			<Form.Label>config login password</Form.Label>
			<FormControl placeholder='admin' type='password' value={_.get(cfg, "webui.login.password", "")} onChange={(e) => {updateCfg('webui.login.password', e.target.value);}} />
		</Form.Group>

		<hr className="hr-gray" />
	</div>
}

export default GlobalPane;
