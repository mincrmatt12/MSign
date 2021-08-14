import React from 'react';
import BackendVersionContext from '../../ver';

export default function VersionTag() {
	const remoteVer = React.useContext(BackendVersionContext);
	const localVer = VERSION;

	return <p><span class="text-muted">webui</span> {localVer} • <span class="text-muted">remote</span> {remoteVer}</p>;
};
