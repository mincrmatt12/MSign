import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Tab from 'react-bootstrap/Tab';
import Tabs from 'react-bootstrap/Tabs';

import * as _ from 'lodash-es';

import ConfigContext from '../ctx';
import VersionTag from "./common/vertag";

function GlobalPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	return <div>
		<hr className="hr-gray" />

		<Form.Group className="my-2" controlId="ssid_ctrl">
			<Form.Label>hostname</Form.Label>
			<FormControl placeholder="msign" type='text' value={_.get(cfg, "wifi.conn.hostname", "")} onChange={(e) => {updateCfg('wifi.conn.hostname', e.target.value);}} />
		</Form.Group>

		<Form.Group className="my-2" controlId="ssid_ctrl">
			<Form.Label>ssid</Form.Label>
			<FormControl type='text' value={_.get(cfg, "wifi.conn.ssid", "")} onChange={(e) => {updateCfg('wifi.conn.ssid', e.target.value);}} />
		</Form.Group>
		
		<Tabs className="my-2" fill id="wifi-auth-tabs" defaultActiveKey={_.get(cfg, "wifi.conn.enterprise.username", "") == "" ? "psk" : "enterprise"}>
			<Tab eventKey="psk" title="PSK">
				<Form.Group className="my-2" controlId="psk_ctrl">
					<Form.Label>psk</Form.Label>
					<FormControl placeholder="<open network>" type='password' value={_.get(cfg, "wifi.conn.psk", "")} onChange={(e) => {updateCfg('wifi.conn.psk', e.target.value);}} />
				</Form.Group>
			</Tab>
			<Tab eventKey="enterprise" title="Enterprise">
				<Form.Group className="my-2" controlId="username_ctrl">
					<Form.Label>username</Form.Label>
					<FormControl type='text' value={_.get(cfg, "wifi.conn.enterprise.username", "")} onChange={(e) => {updateCfg('wifi.conn.enterprise.username', e.target.value);}} />
				</Form.Group>
				<Form.Group className="my-2" controlId="identity_ctrl">
					<Form.Label>anon identity</Form.Label>
					<FormControl placeholder={_.get(cfg, "wifi.conn.enterprise.username", "")} 
						type='text' value={_.get(cfg, "wifi.conn.enterprise.identity", "")} onChange={(e) => {updateCfg('wifi.conn.enterprise.identity', e.target.value);}} />
				</Form.Group>
				<Form.Group className="my-2" controlId="password_ctrl">
					<Form.Label>password</Form.Label>
					<FormControl type='password' value={_.get(cfg, "wifi.conn.enterprise.password", "")} onChange={(e) => {updateCfg('wifi.conn.enterprise.password', e.target.value);}} />
				</Form.Group>
			</Tab>
		</Tabs>

		<hr className="hr-gray" />

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

		<VersionTag />
	</div>
}

export default GlobalPane;
