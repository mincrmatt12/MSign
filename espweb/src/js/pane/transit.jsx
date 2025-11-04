import React from 'react'
import Pagination from 'react-bootstrap/Pagination';

import ConfigContext from '../ctx';
import * as _ from 'lodash-es';

import TTCPane from './transit/ttc';
import GTFSPane from './transit/gtfs';

export default function TransitPane() {
	const [cfg, updateCfg] = React.useContext(ConfigContext);
	const mode = _.get(cfg, "transit_implementation", "ttc").toLowerCase();

	return <div>
		<div className="d-flex align-items-center gap-2">backend <Pagination className="mb-0">
			<Pagination.Item active={mode === "none"} onClick={() => updateCfg("transit_implementation", "none")}>none</Pagination.Item>
			<Pagination.Item active={mode === "ttc"} onClick={() => updateCfg("transit_implementation", "ttc")}>ttc</Pagination.Item>
			<Pagination.Item active={mode === "gtfs"} onClick={() => updateCfg("transit_implementation", "gtfs")}>gtfs</Pagination.Item>
		</Pagination></div>
		<hr className="hr-gray" />
		{mode === "ttc" && <TTCPane />}
		{mode === "gtfs" && <GTFSPane />}
	</div>;
}
