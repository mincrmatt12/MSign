import React from 'react';

const ConfigContext = React.createContext();

export default ConfigContext;

// Helper typecasters

function asOrEmpty(val, conv, def) {
	if (val.trim() == "") return undefined;
	const res = conv(val);
	if (Number.isNaN(res)) return def;
	return res;
}

function asIntOrEmpty(val, def) {
	return asOrEmpty(val, Number.parseInt, def);
}

function asFltOrEmpty(val, def) {
	return asOrEmpty(val, Number.parseFloat, def);
}

function intInteract(newval) {
	return (def) => asIntOrEmpty(newval, def);
}

function floatInteract(newval) {
	return (def) => asFltOrEmpty(newval, def);
}

export {intInteract, floatInteract};
