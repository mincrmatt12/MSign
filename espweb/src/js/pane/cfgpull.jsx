import React from 'react'
import Form from 'react-bootstrap/Form'

import _ from 'lodash';

import ConfigContext from '../ctx';
import {HostnameEdit} from './common/hostname';

export default function CfgPullPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);

	return <div>
		<Form.Check checked={_.get(cfg, "cfgpull.enabled", false)} label="enabled" onChange={(e) => updateCfg("cfgpull.enabled", e.target.checked)} />
		<Form.Check checked={_.get(cfg, "cfgpull.only_firm", false)} label="only download firmware" onChange={(e) => updateCfg("cfgpull.only_firm", e.target.checked)} />
		{_.get(cfg, "cfgpull.enabled", false) && <div>
			<hr className="hr-gray" />
			<HostnameEdit value={_.get(cfg, "cfgpull.host")} onChange={(v) => updateCfg("cfgpull.host", v)} />
		</div>}
		<hr className="hr-gray" />
	</div>
}
