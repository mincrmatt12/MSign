import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Dropdown from 'react-bootstrap/Dropdown'
import DropdownButton from 'react-bootstrap/DropdownButton'

import _ from 'lodash';

function getURLPrefix(underscoreEncoded) {
	if (underscoreEncoded[0] == "_") return "https://" + underscoreEncoded.slice(1);
	else return "http://" + underscoreEncoded;
}

function HostnameEdit({value, placeholder = "", onChange}) {
	const realPlaceholder = placeholder.length > 0 && placeholder[0] == "_" ? placeholder.slice(1) : placeholder;

	let isHttps = value !== undefined && value.length > 0 && value[0] == "_";
	if (value === undefined || value === null || value === "" && placeholder != "") isHttps = placeholder.length > 0 && placeholder[0] == "_";
	const realText = (value !== undefined && isHttps) ? value.slice(1) : value;

	return <InputGroup>
		<InputGroup.Text>{isHttps ? "https://" : "http://"}</InputGroup.Text>
		<DropdownButton title="" variant="dark" disabled={value === undefined}>
			<Dropdown.Item onClick={() => {
				if (isHttps) onChange(value.slice(1));
				else onChange("_" + value);
			}}>{isHttps ? "http://" : "https://"}</Dropdown.Item>
		</DropdownButton>
		<FormControl placeholder={realPlaceholder} type="text" value={realText} onChange={(e) => {
			if (isHttps) onChange("_" + e.target.value);
			else onChange(e.target.value);
		}} />
	</InputGroup>
}

export {getURLPrefix, HostnameEdit};
